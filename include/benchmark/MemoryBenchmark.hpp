#pragma once
#include <string>
#include <chrono>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <barrier>
#include <functional>
#include "Benchmark.hpp"
#include "ThreadGroup.hpp"
#include "AdditionalWork.hpp"

namespace bench{
class MemoryBenchmark: public Benchmark{
private:
    //Private static variables
    static constexpr size_t BATCH_SIZE  = 100;
    static constexpr size_t GRANULARITY = 100; //number of iterations before checking time
    static constexpr std::chrono::milliseconds SLEEP_TIME = 100ms;
public:
    struct Results{
        struct Stats{
            size_t max;
            size_t min;
            size_t start;
            size_t end;
            double mean;
            double stddev;
            Stats(size_t max = 0, size_t min = 0, size_t start = 0, size_t end = 0,
                    double mean = 0, double stddev = 0);
        };
        Stats RSS;
        Stats VM;

         // Constructor for Results
        Results(size_t rssMax = 0, size_t rssMin = 0, size_t rssStart = 0, size_t rssEnd = 0,
                double rssMean = 0, double rssStddev = 0,
                size_t vmMax = 0, size_t vmMin = 0, size_t vmStart = 0, size_t vmEnd = 0,
                double vmMean = 0, double vmStddev = 0);

        // Methods for setting RSS and VM
        void setRSS(size_t max, size_t min, size_t start, size_t end,
                    double mean, double stddev);
        void setVM(size_t max, size_t min, size_t start, size_t end,
                double mean, double stddev);
        void setResults(size_t rssMax, size_t rssMin, size_t rssStart, size_t rssEnd,
                        double rssMean, double rssStddev,
                        size_t vmMax, size_t vmMin, size_t vmStart, size_t vmEnd,
                        double vmMean, double vmStddev);


    };

    struct MemoryInfo {
        long vmSize;  // Virtual memory size in KB
        long rssSize; // RSS size in KB

        MemoryInfo(long rss = 0,long vm = 0): rssSize{rss}, vmSize{vm}{};
        ~MemoryInfo(){};
    };

    struct MemoryControl {
        struct ThreadControl{
            std::chrono::milliseconds initUptime;
            std::chrono::milliseconds sleep;
            std::chrono::milliseconds uptime;

            ThreadControl(std::chrono::milliseconds init = 0ms,
                            std::chrono::milliseconds sleep = 0ms,
                            std::chrono::milliseconds uptime = 0ms);
        };
    
    private:
    short level;
    static constexpr short NONE     = 0;
    static constexpr short PRODS    = 1;
    static constexpr short CONS     = 2;

    ThreadControl producer;
    ThreadControl consumer;
    public:
    //Default member initialization
    size_t max_memory{std::numeric_limits<size_t>::max()}; //biggestsize_t
    size_t min_memory{0};
    std::chrono::milliseconds maxReachSleep{0};
    std::chrono::milliseconds minReachSleep{0};

    MemoryControl(){}; //Default constructor

    MemoryControl(  size_t max, std::chrono::milliseconds maxReachSleep,
                    size_t min, std::chrono::milliseconds minReachSleep
    );
    void producers( std::chrono::milliseconds init,
                    std::chrono::milliseconds sleep,
                    std::chrono::milliseconds uptime
    );
    void consumers( std::chrono::milliseconds init,
                    std::chrono::milliseconds sleep,
                    std::chrono::milliseconds uptime
    );

    friend class MemoryBenchmark;
};

    size_t producers, consumers;
    size_t ringSize;
    double producerAdditionalWork;
    double consumerAdditionalWork;
    bool balancedLoad;
    Arguments flags;
    MemoryControl memoryFlags;
    //Later for memory Arguments
    MemoryBenchmark(    size_t prodCount, size_t consCount, double additionalWork,
                        bool balancedLoad, size_t ringSize = RINGSIZE,Arguments args = Arguments(), MemoryControl memoryFlags = MemoryControl()
                    );
    
    ~MemoryBenchmark(){};

    static inline MemoryInfo getProcessMemoryInfo(pid_t pid){
        using namespace std;
        ifstream file("/proc/" + to_string(pid) + "/status");
        if(!file.is_open()) 
            cerr << "Error opening file for Pid" << pid << endl;
        else{
            long rss = -1;
            long vm = -1;
            string line;
            while (getline(file, line) && (rss == -1 || vm == -1)) {
                (void) sscanf(line.c_str(),"VmSize: %ld kB",&vm);
                (void) sscanf(line.c_str(),"VmRSS: %ld kB",&rss);
            }
            file.close();
            if(rss != -1 && vm != -1)
                return MemoryInfo(rss,vm);
            else cerr << "Error reading file for Pid" << pid << endl;
        }
        return MemoryInfo(-1,-1);
    }

private:
    static void printHeader(std::string filePath);
    static int memoryMonitor(pid_t proc, std::chrono::seconds runDuration, std::chrono::milliseconds granularity, std::string fileName, size_t maxMemory, size_t minMemory);
    static Results computeMetrics(std::string fileName);

    template<template<typename> typename Q>
    Results __MemoryBenchmark(seconds runDuration,milliseconds granularity,std::string fileName){
        using namespace std;
        using namespace chrono;
        
        Q<UserData> *queue = nullptr;
        barrier<> barrier(producers + consumers + 1);
        atomic<bool> stopFlag{false};
        atomic<int> producerFlag{0};
        atomic<int> consumerFlag{0};

        const auto prod = [this,&stopFlag,&queue,&barrier,&producerFlag](const int tid){
            UserData ud{};
            size_t currentFlag;
            size_t sleepFlag = 1;
            barrier.arrive_and_wait();  //wait main to do stuff
            while(!stopFlag){
                if(currentFlag = producerFlag.load() == sleepFlag){
                    this_thread::sleep_for(memoryFlags.maxReachSleep);
                    sleepFlag = ++currentFlag;
                }
                queue->push(&ud,tid);
                random_additional_work(producerAdditionalWork);
            }
        };
        const auto cons = [this,&stopFlag,&queue,&barrier,&consumerFlag](const int tid){
            UserData *ud;
            size_t currentFlag;
            size_t sleepFlag = 1;
            barrier.arrive_and_wait();  //wait main to do stuff
            while(!stopFlag){
                if(currentFlag = consumerFlag.load() == sleepFlag){
                    this_thread::sleep_for(memoryFlags.minReachSleep);
                    sleepFlag = ++currentFlag;
                }
                queue->pop(tid);
                random_additional_work(consumerAdditionalWork);
            }
        };
        const auto prod_sync = [this,&stopFlag,&queue,&barrier,&producerFlag](const int tid){
            UserData ud{};
            size_t currentFlag;
            size_t sleepFlag = 1;
            //Reduce long sleeps in sleep cycles
            milliseconds sleepTime = memoryFlags.producer.sleep;
            size_t sleepCycles = (sleepTime.count())/(SLEEP_TIME.count());
            if(sleepCycles == 0) sleepCycles = 1;   //at least one cycle

            barrier.arrive_and_wait(); //Wait for main to do stuff
            auto endtime = steady_clock::now() + memoryFlags.producer.initUptime;
            while(steady_clock::now() < endtime){   //initial Uptime
                for(int i = 0; i < GRANULARITY; i++){
                    if(stopFlag.load()) return;
                    if(currentFlag = producerFlag.load() == sleepFlag){ //MemoryControl
                        this_thread::sleep_for(memoryFlags.maxReachSleep);
                        sleepFlag = currentFlag + 1;
                    }
                    queue->push(&ud,tid);
                    random_additional_work(producerAdditionalWork);
                }
            }
            while(true){    //Subsequent Uptimes
                //Sleep cycles
                for(int i = 0; i < sleepCycles; i++){
                    this_thread::sleep_for(SLEEP_TIME);
                    if(stopFlag.load()) return;
                }
                auto endtime = steady_clock::now() + memoryFlags.producer.uptime;
                while(steady_clock::now() < endtime){
                    for(int i = 0; i < GRANULARITY; i++){
                        if(stopFlag.load()) return;
                        if(currentFlag = producerFlag.load() == sleepFlag){
                            this_thread::sleep_for(memoryFlags.minReachSleep);
                            sleepFlag = currentFlag + 1;
                        }
                        queue->push(&ud,tid);
                        random_additional_work(producerAdditionalWork);
                    }
                }
            }
        };
        const auto cons_sync = [this,&stopFlag,&queue,&barrier,&consumerFlag](const int tid){
            UserData *ud;
            size_t currentFlag;
            size_t sleepFlag = 1;
            //Reduce long sleeps in sleep cycles
            milliseconds sleepTime = memoryFlags.consumer.sleep;
            size_t sleepCycles = (sleepTime.count())/(SLEEP_TIME.count());
            if(sleepCycles == 0) sleepCycles = 1;   //at least one cycle

            barrier.arrive_and_wait(); //Wait for main to do stuff
            auto endtime = steady_clock::now() + memoryFlags.consumer.initUptime;
            while(steady_clock::now() < endtime){   //initial Uptime
                for(int i = 0; i < GRANULARITY; i++){
                    if(stopFlag.load()) return;
                    if(currentFlag = consumerFlag.load() == sleepFlag){ //MemoryControl
                        this_thread::sleep_for(memoryFlags.minReachSleep);
                        sleepFlag = currentFlag + 1;
                    }
                    queue->pop(tid);
                    random_additional_work(consumerAdditionalWork);
                }
            }
            while(true){    //Subsequent Uptimes
                //Sleep cycles
                for(int i = 0; i < sleepCycles; i++){
                    this_thread::sleep_for(SLEEP_TIME);
                    if(stopFlag.load()) return;
                }
                auto endtime = steady_clock::now() + memoryFlags.consumer.uptime;
                while(steady_clock::now() < endtime){
                    for(int i = 0; i < GRANULARITY; i++){
                        if(stopFlag.load()) return;
                        if(currentFlag = consumerFlag.load() == sleepFlag){
                            this_thread::sleep_for(memoryFlags.minReachSleep);
                            sleepFlag = currentFlag + 1;
                        }
                        queue->pop(tid);
                        random_additional_work(consumerAdditionalWork);
                    }
                }
            }
        };
    
        queue = new Q<UserData>(ringSize, producers + consumers);
        //customize signal mask
        sigset_t mask,oldmask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGUSR1);
        sigaddset(&mask, SIGUSR2);
        sigaddset(&mask, SIGALRM);
        if(sigprocmask(SIG_SETMASK, &mask, &oldmask) < 0){  //The child process will inherit this mask (We dont care about sigalarm)
            cerr << "Error setting signal mask" << endl;
            return Results();
        }

        //fork memoryMonitor
        pid_t pid = fork();
        if(pid < 0){
            cerr << "Error forking memoryMonitor" << endl;
            return Results();
        } else if (pid == 0){
            //Child process
#if defined(__linux__)
            return memoryMonitor(getppid(),runDuration,granularity,fileName,memoryFlags.max_memory,memoryFlags.min_memory);
#else
#error "Memory Monitor not implemented for this OS"
#endif
        } 
        //Parent process
        ThreadGroup threads{};
        //Schedule threads accordingly to MemoryControl level

        if(memoryFlags.level | MemoryControl::PRODS){
            for(int iProd = 0; iProd < producers; iProd++)
                threads.thread(prod_sync);
        } else {
            for(int iProd = 0; iProd < producers; iProd++)
                threads.thread(prod);
        }
        
        if(memoryFlags.level | MemoryControl::CONS){
            for(int iCons = 0; iCons < consumers; iCons++)
                threads.thread(cons_sync);
        } else {
            for(int iCons = 0; iCons < consumers; iCons++)
                threads.thread(cons);
        }
        
        barrier.arrive_and_wait(); //init
        int sig;    //Signal exchange between parent and child
        kill(pid,SIGUSR1);  //start measuring
        puts("STARTING MEASURING");
        do{
            sigwait(&mask,&sig);
            if(sig == SIGUSR1)
                producerFlag.fetch_add(1);
            else if(sig == SIGUSR2)
                consumerFlag.fetch_add(1);
        }while(sig != SIGALRM); //end of measuration;
        stopFlag.store(true);   //signal to stop all threads
        sigprocmask(SIG_SETMASK, &oldmask, NULL); //restore old mask
        puts("WAITING FOR JOIN");
        threads.join();
        puts("JOINED");
        delete (Q<UserData>*) queue;    //automatically drains the queue and deallocates it
        int status;
        waitpid(pid,&status,0); //wait for memoryMonitor to finish
        return WIFEXITED(status) && WEXITSTATUS(status) == 0? computeMetrics(fileName) : Results();
    }

public:

    template<template<typename> typename Q>
    Results MemoryRun(seconds runDuration,milliseconds granularity,std::string fileName=""){
        bool deleteAfter = false;
        if(fileName == ""){
            fileName = "./"+ Q<UserData>::className(false) + "_Mem" + ".tmp";
            deleteAfter = true;
        }
        printHeader(fileName);
        Results retval = __MemoryBenchmark<Q>(runDuration,granularity,fileName);
        // if(deleteAfter) 
        //     remove(fileName.c_str());
        return retval;
    }

};  //MemoryBenchmark

} //namespace bench