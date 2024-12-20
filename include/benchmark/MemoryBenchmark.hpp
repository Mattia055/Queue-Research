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

#include "BenchmarkUtils.hpp"
#include "ThreadGroup.hpp"
#include "AdditionalWork.hpp"

#define MEM_MGR_PATH "./memoryManager"
#define GRANULARITY 500 //number of iterations before checking time

namespace bench{


class MemoryBenchmark{
public:
    struct Results{
        double RSS_mean;
        double RSS_stddev;
        double VM_mean;
        double VM_stddev;
    };

    static constexpr short SYNC_NONE    = 0;
    static constexpr short SYNC_PRODS   = 1;
    static constexpr short SYNC_CONS    = 2;
    static constexpr short SYNC_BOTH    = 4;

private:
    static constexpr size_t WARMUP      = 1'000'000ULL;
    static constexpr size_t RINGSIZE    = 4096;

public:
    size_t producers, consumers;
    size_t warmup;
    size_t ringSize;
    double additionalWork;
    double producerAdditionalWork;
    double consumerAdditionalWork;
    bool balancedLoad;
    Arguments   flags;
    MemoryArguments memoryFlags;

    MemoryBenchmark(size_t numProducers, size_t numConsumers, double additionalWork,
                bool balancedLoad, size_t ringSize = RINGSIZE, 
                MemoryArguments mflags = MemoryArguments(), size_t warmup = WARMUP,
                Arguments args = Arguments())
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
            int sleepFlag = 1; //flag to check with mainù
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
            if (execl(  MEM_MGR_PATH, 
                        MEM_MGR_PATH,
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

            auto prod_routine = prod_lambda;
            auto cons_routine = cons_lambda;

            //Thread scheduling

            if(memoryFlags.synchro == SYNC_NONE || memoryFlags.synchro == SYNC_CONS){
                for(size_t iProd = 0; iProd < producers; iProd++)
                    threads.thread(prod_lambda);
            } else{ 
                for(size_t iProd = 0; iProd < producers; iProd++)
                    threads.thread(synchro_prod_lambda);
            }

            if(memoryFlags.synchro == SYNC_NONE || memoryFlags.synchro == SYNC_PRODS){
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

        //compute mean and stddev incrementally
        std::ifstream file(fileName);
        string line;

        double RSS_mean = 0;
        double RSS_stddev = 0;
        double VM_mean = 0;
        double VM_stddev = 0;

        size_t count = 0;
        while(std::getline(file,line)){
            double rssSize,vmSize;
            std::sscanf(line.c_str(), "%lf,%lf", &vmSize, &rssSize);
            RSS_mean += rssSize;
            VM_mean += vmSize;
            count++;
        }

        RSS_mean /= count;
        VM_mean /= count;

        file.clear();
        file.seekg(0);
        count = 0;
        while(std::getline(file,line)){
            double rssSize,vmSize;
            count++;
            std::sscanf(line.c_str(), "%lf,%lf", &vmSize, &rssSize);
            RSS_stddev  += pow((rssSize - RSS_mean),2);
            VM_stddev   += pow((vmSize - VM_mean),2);
        }

        RSS_stddev  = sqrt(RSS_stddev/count);
        VM_stddev   = sqrt(VM_stddev/count);

        file.close();
        return Results{RSS_mean,RSS_stddev,VM_mean,VM_stddev};

        
    }

    std::string randomDigits(size_t n){
        std::string digits = "0123456789";
        std::string result;
        for(size_t i = 0; i < n; i++){
            result += digits[rand() % 10];
        }
        return result;
    }



public: 
    template<template<typename> typename Q>
    void MemoryRun(seconds runDuration,milliseconds granularity,string fileName=""){
        //filename is the file where the memory monitor writes the data [temporary]
        if(fileName == "")
            fileName = Q<UserData>::className() + "_Memory_" + "" + ".csv";

        Results res = __MemoryBenchmark<Q>(runDuration,granularity,fileName);
        if(flags._stdout){
            printBenchmarkResults("MemoryBenchmark","RSS",res.RSS_mean,res.RSS_stddev);
            printBenchmarkResults("MemoryBenchmark","VM",res.VM_mean,res.VM_stddev);
        }
    }
};

} //namespace bench