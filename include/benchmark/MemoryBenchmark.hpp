#pragma once
#include <atomic>
#include <barrier>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <cstring>
#include <sys/wait.h>
#include <cmath>
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <string>

#include "Benchmark.hpp"
#include "ThreadGroup.hpp"
#include "AdditionalWork.hpp"



namespace bench{


class MemoryBenchmark: public Benchmark{
private:
    static constexpr std::string MEM_MGR_PATH    = "./memoryManager";
    static constexpr size_t GRANULARITY     = 500; //number of iterations before checking time

public:
    struct Results{
        size_t RSS_max      = 0;
        size_t VM_max       = 0;
        size_t RSS_min      = 0;
        size_t VM_min       = 0;
        size_t RSS_start    = 0;
        size_t VM_start     = 0;
        size_t RSS_end      = 0;
        size_t VM_end       = 0;
        double RSS_mean     = 0;
        double VM_mean      = 0;
        double RSS_stddev   = 0;
        double VM_stddev    = 0;
    };

    struct MemoryInfo {
        long vmSize = 0;  // Virtual memory size in KB
        long rssSize = 0; // RSS size in KB
    };

    struct MemoryArguments {

    size_t max_memory;
    size_t min_memory;
    milliseconds supSleep;  //producers sleep if max_memory is reached
    milliseconds infSleep;  //consumers sleep if min_memory is reached
    short synchro;         //Bitmask for synchro
    milliseconds producerInitialDelay;
    milliseconds consumerInitialDelay;
    milliseconds producerSleep; //should be multiple of granularity
    milliseconds producerUptime;
    milliseconds consumerSleep;
    milliseconds consumerUptime;

    
    MemoryArguments(    milliseconds supSleepProducer = 100ms,
                        milliseconds infSleepConsumer = 100ms,
                        size_t max = getTotalMemory(false) / 3 * 2,
                        size_t min = 0
                    )
    :   max_memory{max}, min_memory{min}, supSleep{supSleepProducer}, 
        infSleep{infSleepConsumer}, synchro{0} 
    {}

};


    static constexpr short SYNC_PRODS   = 1;
    static constexpr short SYNC_CONS    = 2;

    size_t producers, consumers;
    size_t warmup;
    size_t ringSize;
    double additionalWork;
    double producerAdditionalWork;
    double consumerAdditionalWork;
    bool balancedLoad;
    Arguments   flags;
    MemoryArguments memoryFlags;

    MemoryBenchmark(    size_t numProducers, size_t numConsumers, double additionalWork,
                        bool balancedLoad, size_t ringSize = RINGSIZE, 
                        MemoryArguments mflags = MemoryArguments(), 
                        size_t warmup = WARMUP,Arguments args = Arguments()
                    )
    : producers{numProducers}, consumers{numConsumers}, additionalWork{additionalWork},
      ringSize{ringSize}, balancedLoad{balancedLoad}, warmup{warmup}, flags{args},
      memoryFlags{mflags}{};

    
    ~MemoryBenchmark(){};

    //fork exec the process
        //first allocate all resources necessary
        //start threads
        //wait for warmup
        //send signal to the process
        //wait signal from the process
        //start measuring
private:
    template<template<typename> typename Q> 
    Results __MemoryBenchmark(seconds runDuration,milliseconds granularity,string fileName){

        //allocate resources
        Q<UserData> *queue = nullptr;
        barrier<> barrier(producers + consumers + 1);
        std::atomic<bool> stopFlag{false};
        std::atomic<int> producerFlag{0};
        std::atomic<int> consumerFlag{0};


        //lambdas for threads routines
        const auto synchro_prod_lambda = [this,&stopFlag,&queue,&barrier,&producerFlag,granularity](const int tid){
            UserData ud{0,0};
            int sleepFlag = 1; //flag to check with main
            barrier.arrive_and_wait();
            for(size_t iter = 0; iter < warmup/producers; ++iter)
                queue->enqueue(&ud,tid);
            barrier.arrive_and_wait();
            //consumers drain the queue
            barrier.arrive_and_wait();
            //wait for main

            //reduce long sleeps in a for cycles of sleep
            milliseconds sleepTime = this->memoryFlags.producerSleep;
            size_t sleepCycles = sleepTime/granularity;
            if(sleepCycles == 0) sleepCycles = 1;   //at least one cycle

            barrier.arrive_and_wait();
            //start measuring
            
            auto time = steady_clock::now();
            auto endtime = time + memoryFlags.producerInitialDelay;

            while(steady_clock::now() < endtime){
                for(int i = 0; i< GRANULARITY; i++){
                    if(i % 31 == 0){
                        if(stopFlag.load()) 
                            return;
                        if(producerFlag.load() == sleepFlag){
                            std::this_thread::sleep_for(this->memoryFlags.infSleep);
                            sleepFlag = producerFlag.load() + 1;
                        }
                    }
                    queue->enqueue(&ud,tid);
                    random_additional_work(producerAdditionalWork);
                }
            }

            while(true){
            for(int i = 0; i< sleepCycles; i++){
                this_thread::sleep_for(granularity);
                if(stopFlag.load()) return;
            }

            auto time = steady_clock::now();
            auto endtime = time + memoryFlags.producerUptime;
            while(steady_clock::now() < endtime){
                for(int i = 0; i< GRANULARITY; i++){
                if(i % 31 == 0){
                    if(stopFlag.load()) 
                    return;
                    if(producerFlag.load() == sleepFlag){
                    std::this_thread::sleep_for(this->memoryFlags.infSleep);
                    sleepFlag = producerFlag.load() + 1;
                    }
                }
                queue->enqueue(&ud,tid);
                random_additional_work(producerAdditionalWork);
                }
            }
            }
            
        };
        const auto synchro_cons_lambda = [this,&stopFlag,&queue,&barrier,&consumerFlag,granularity](const int tid){
            UserData *ud;
            int sleepFlag = 1; //flag to check with main
            barrier.arrive_and_wait();
            for(size_t iter = 0; iter < warmup/consumers; ++iter){
                ud = queue->dequeue(tid);
                if(ud != nullptr && ud->seq > 0){
                    std::cerr << "This will never appear" << std::endl;
                }
            }
            barrier.arrive_and_wait();
            while(queue->dequeue(tid) != nullptr);
            barrier.arrive_and_wait();
            //wait for main

            //reduce long sleeps in a for cycles of sleep
            milliseconds sleepTime = this->memoryFlags.consumerSleep;
            size_t sleepCycles = sleepTime/granularity;
            if(sleepCycles == 0) sleepCycles = 1;   //at least one cycle

            barrier.arrive_and_wait();
            //start measuring
            
            auto time = steady_clock::now();
            auto endtime = time + memoryFlags.consumerInitialDelay;
            while(steady_clock::now() < endtime){
                for(int i = 0; i< GRANULARITY; i++){
                    if(i % 31 == 0){
                        if(stopFlag.load()) 
                            return;
                        if(consumerFlag.load() == sleepFlag){
                            std::this_thread::sleep_for(this->memoryFlags.infSleep);
                            sleepFlag = consumerFlag.load() + 1;
                        }
                    }
                    ud = queue->dequeue(tid);
                    if(ud != nullptr && ud->seq > 0){
                    std::cerr << "This will never appear" << std::endl;
                    }
                    random_additional_work(consumerAdditionalWork);
                }
            }
            while(true){
                for(int i = 0; i< sleepCycles; i++){
                    this_thread::sleep_for(granularity);
                    if(stopFlag.load()){
                         return;
                    }
                }
                auto time = steady_clock::now();
                auto endtime = time + memoryFlags.consumerUptime;
                while(steady_clock::now() < endtime){
                    for(int i = 0; i< GRANULARITY; i++){
                        if(i % 31 == 0){
                            if(stopFlag.load()){
                                puts("CONSUMER OUT");
                                return;
                            }
                            if(consumerFlag.load() == sleepFlag){
                            std::this_thread::sleep_for(this->memoryFlags.infSleep);
                            sleepFlag = consumerFlag.load() + 1;
                            }
                        }
                        ud = queue->dequeue(tid);
                        if(ud != nullptr && ud->seq > 0){
                            std::cerr << "This will never appear" << std::endl;
                        }
                        random_additional_work(consumerAdditionalWork);
                    }
                }
            }
            
        };

        const auto prod_lambda = [this,&stopFlag,&queue,&barrier,&producerFlag](const int tid){
            UserData ud{0,0};
            int sleepFlag = 1; //flag to check with main
            barrier.arrive_and_wait();
            for(size_t iter = 0; iter < warmup/producers; ++iter)
                queue->enqueue(&ud,tid);
            barrier.arrive_and_wait();
            //consumers drain the queue
            barrier.arrive_and_wait();
            //wait for main
            barrier.arrive_and_wait();
            //start measuring
            while(!stopFlag){
                if(producerFlag.load() == sleepFlag){
                    std::this_thread::sleep_for(this->memoryFlags.supSleep);
                    sleepFlag = producerFlag.load() + 1;
                }
                queue->enqueue(&ud,tid);
                random_additional_work(producerAdditionalWork);
            }
            return;
        };
        const auto cons_lambda = [this,&stopFlag,&queue,&barrier,&consumerFlag](const int tid){
            UserData *ud;
            int sleepFlag = 1; //flag to check with main

            barrier.arrive_and_wait();
            for(size_t iter = 0; iter < warmup/consumers; ++iter)
                ud = queue->dequeue(tid);
                if(ud != nullptr && ud->seq > 0){
                    std::cerr << "This will never appear" << std::endl;
                }
            barrier.arrive_and_wait();
            while(queue->dequeue(tid) != nullptr);
            barrier.arrive_and_wait();
            //wait for main
            barrier.arrive_and_wait();
            //start measuring
            while(!stopFlag){
                if(consumerFlag.load() == sleepFlag){
                    std::this_thread::sleep_for(this->memoryFlags.infSleep);
                    sleepFlag = consumerFlag.load() + 1;
                }
                ud = queue->dequeue(tid);
                if(ud != nullptr && ud->seq > 0){
                    std::cerr << "This will never appear" << std::endl;
                }
                random_additional_work(consumerAdditionalWork);
            }
            return;
        };

        //create the queue
        queue = new Q<UserData>(producers + consumers, ringSize);

        //fork exec memory monitor
        pid_t pid = vfork();
        if(pid == 0)
        {
            if (execl(  MEM_MGR_PATH.c_str(), 
                        MEM_MGR_PATH.c_str(),
                        std::to_string(getppid()).c_str(),              //parent pid
                        std::to_string(runDuration.count()).c_str(),    //duration
                        std::to_string(granularity.count()).c_str(),    //granularity
                        fileName.c_str(),                               //filename
                        std::to_string(memoryFlags.max_memory).c_str(), //max memory
                        std::to_string(memoryFlags.min_memory).c_str(), //min memory
                        (char *)NULL) == -1)
            {
                std::cerr << "execl() failed: " << std::strerror(errno) << std::endl;
                exit(-1);
            }

        } else {
            //set signal mask
            sigset_t mask,oldmask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGUSR1);
            sigaddset(&mask, SIGUSR2);
            sigaddset(&mask, SIGALRM);
            sigprocmask(SIG_SETMASK, &mask, &oldmask);

            ThreadGroup threads{};

            //Thread scheduling

            if(memoryFlags.synchro == 0 || memoryFlags.synchro == SYNC_CONS){
                for(size_t iProd = 0; iProd < producers; iProd++)
                    threads.thread(prod_lambda);
            } else{ 
                for(size_t iProd = 0; iProd < producers; iProd++)
                    threads.thread(synchro_prod_lambda);
            }

            if(memoryFlags.synchro == 0 || memoryFlags.synchro == SYNC_PRODS){
                for(size_t iCons = 0; iCons < consumers; iCons++)
                    threads.thread(cons_lambda);
            } else{
                for(size_t iCons = 0; iCons < consumers; iCons++)
                    threads.thread(synchro_cons_lambda);
            }

            barrier.arrive_and_wait();  //init
            barrier.arrive_and_wait();  //warmup
            barrier.arrive_and_wait();  //drain queue
            barrier.arrive_and_wait();  //measurement

            kill(pid,SIGUSR1);  //start measuring
            int sig;
            do{
                sigwait(&mask, &sig);
                if(sig == SIGUSR1){
                    producerFlag.fetch_add(1);
                }
                else if(sig == SIGUSR2){
                    consumerFlag.fetch_add(1);
            }
            }while(sig != SIGALRM);

            //restore old mask
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            stopFlag.store(true);
            threads.join();
            while(queue->dequeue(0) != nullptr);
            delete (Q<UserData>*) queue;
            int status;
            pid_t result = waitpid(pid, &status, 0);

        }

    return computeMetrics(fileName);

        
    }

Results computeMetrics(string filename){
    std::ifstream file(filename);
    std::string line;
    double RSS_mean, VM_mean;
    double RSS_stddev, VM_stddev;
    size_t RSS_max,VM_max;
    size_t RSS_min,VM_min;
    size_t RSS_start,VM_start;
    size_t RSS_end,VM_end;

    size_t RSS_curr, VM_curr;
    size_t count = 1;

    //get first line
    bool eFile = true;  //assume empty file

    while(std::getline(file,line)){
        if(std::sscanf(line.c_str(), "%ld,%ld",&RSS_curr,&VM_curr) != 2) continue;
        RSS_mean = RSS_curr = RSS_max = RSS_min = RSS_start;
        VM_mean = VM_curr = VM_max = VM_min = VM_start;
        eFile = false;
        break;
    }

    if(eFile){
        return Results{};
    }

    while(std::getline(file,line)){
        if(std::sscanf(line.c_str(), "%ld,%ld",&RSS_curr,&VM_curr) != 2) continue;
        RSS_mean += RSS_curr;
        VM_mean += VM_curr;
        if(RSS_curr > RSS_max) RSS_max = RSS_curr;
        if(RSS_curr < RSS_min) RSS_min = RSS_curr;
        if(VM_curr > VM_max) VM_max = VM_curr;
        if(VM_curr < VM_min) VM_min = VM_curr;
        count++;
    }

    //get last results
    RSS_end = RSS_curr;
    VM_end = VM_curr;

    RSS_mean /= count;
    VM_mean /= count;

    //compute stddev
    file.clear();
    file.seekg(0);

    RSS_stddev = 0; 
    VM_stddev = 0;

    while(std::getline(file,line)){
        if(std::sscanf(line.c_str(), "%ld,%ld",&RSS_curr,&VM_curr) != 2) continue;
        RSS_stddev += pow((RSS_curr - RSS_mean),2);
        VM_stddev += pow((VM_curr - VM_mean),2);
    }

    RSS_stddev = sqrt(RSS_stddev/(count-1));
    VM_stddev = sqrt(VM_stddev/(count-1));

    file.close();

    Results retval;
    retval.RSS_max = RSS_max;
    retval.VM_max = VM_max;
    retval.RSS_min = RSS_min;
    retval.VM_min = VM_min;
    retval.RSS_start = RSS_start;
    retval.VM_start = VM_start;
    retval.RSS_end = RSS_end;
    retval.VM_end = VM_end;
    retval.RSS_mean = RSS_mean;
    retval.VM_mean = VM_mean;
    retval.RSS_stddev = RSS_stddev;
    retval.VM_stddev = VM_stddev;


    return retval;

}

public: 
    template<template<typename> typename Q>
    void MemoryRun(seconds runDuration,milliseconds granularity,string fileName=""){
        bool deleteAfter = false;
        if(fileName == ""){
            fileName = Q<UserData>::className() + "_Mem" + ".tmp";
            deleteAfter = true;
        }

        Results res = __MemoryBenchmark<Q>(runDuration,granularity,fileName);

        if(deleteAfter){
            std::remove(fileName.c_str());
        }

        if(flags._stdout){
            printBenchmarkResults("MemoryBenchmark","RSS",res.RSS_mean,res.RSS_stddev);
            printBenchmarkResults("MemoryBenchmark","VM",res.VM_mean,res.VM_stddev);
        }
    }

public:
static inline size_t getTotalMemory(bool includeSwap) {
    using namespace std;
#ifndef __linux__
    cerr << "Can't get total memory : only linux systems" << endl;
#ifdef SYS_CAUTIOUS
    exit(0);
#else
    return 0;
#endif
#endif

    ifstream memInfo("/proc/meminfo");
    if (!memInfo.is_open()) {
        std::cerr << "Failed to open /proc/meminfo" << std::endl;
#ifdef SYS_CAUTIOUS
        exit(-1);
#endif
        return 0;
    }

    size_t totalMemory = 0;
    size_t totalSwap = 0;
    string line;
    
    while (getline(memInfo, line)) {
        istringstream iss(line);
        string key;
        size_t value;

        // Read the first word (key) and the second number (value)
        if (iss >> key >> value) {
            if (key == "MemTotal:") {
                totalMemory = value;  // In KB
                if(!includeSwap) break;
            } else if (key == "SwapTotal:") {
                totalSwap = value;  // In KB
            }
        }
    }

    // Close the file
    memInfo.close();

    // Return total memory considering the swap flag
    return includeSwap? totalMemory + totalSwap : totalMemory;
}

    // Function to read and return the RSS and VM size of a process
    static inline MemoryInfo getProcessMemoryInfo(pid_t pid) {
        using namespace std;
        
        MemoryInfo memoryInfo{0, 0};
        std::ifstream file("/proc/" + std::to_string(pid) + "/status");

        if (!file.is_open()) {
            std::cerr << "Error opening file for PID " << pid << std::endl;
            return memoryInfo;
        }

        long rssSize    = -1;
        long vmSize     = -1;
        std::string line;

        while (std::getline(file, line) && (rssSize == -1 || vmSize == -1)) {
            // Search for VmSize and VmRSS in the file
            if(std::sscanf(line.c_str(), "VmSize: %ld kB", &vmSize) == 1)
                memoryInfo.vmSize = rssSize;
            if(std::sscanf(line.c_str(), "VmRSS: %ld kB", &rssSize) == 1)
                memoryInfo.rssSize = vmSize;
        }
        
        file.close();
        return memoryInfo;
    }

};


} //namespace bench