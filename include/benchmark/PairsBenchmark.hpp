#pragma once
#include <fstream>
#include <iostream>
#include <barrier>

#include "Benchmark.hpp"
#include "ThreadGroup.hpp"
#include "AdditionalWork.hpp"
#include "Stats.hpp"
#include "QueueTypeSet.hpp"
#include "Format.hpp"

using namespace std;
using namespace chrono;

namespace bench {

class PairsBenchmark: public Benchmark{
public:
    size_t producers, consumers;
    size_t warmup=WARMUP;
    size_t ringSize=RINGSIZE; 
    double additionalWork;
    double producerAdditionalWork;
    double consumerAdditionalWork;
    bool balancedLoad;
    Arguments   flags = Arguments();

public:
    PairsBenchmark(     size_t numProducers,
                        size_t numConsumers,
                        double additionalWork,
                        bool balancedLoad,
                        size_t ringSize=RINGSIZE,
                        size_t warmup=WARMUP,
                        Arguments args=Arguments()
                    ):
    producers{numProducers},
    consumers{numConsumers},
    additionalWork{additionalWork},
    ringSize{ringSize},
    balancedLoad{balancedLoad},
    warmup{warmup},
    flags{args}{

        if(balancedLoad){
            const size_t total  = producers + consumers;
            const double ref    = additionalWork * 2 / total;
            producerAdditionalWork = producers * ref;
            consumerAdditionalWork = consumers * ref;
        } else {
            producerAdditionalWork = additionalWork;
            consumerAdditionalWork = additionalWork;
        }
    }

    ~PairsBenchmark(){};

    template<template<typename> typename Q, typename Duration>
    void ProducerConsumer(const Duration runDuration, const size_t numRuns,std::string fileName){
        auto res = __ProducerConsumer<Q>(runDuration,numRuns);
        Stats<long double> sts = stats(res.begin(),res.end());
        if(flags._stdout){
            printBenchmarkResults(Q<UserData>::className(),"Transf/Sec",sts.mean,sts.stddev);
        }
        if(fileName != ""){
            bool header = flags._overwrite || notFile(fileName);
            ofstream csvFile(fileName, header? ios::trunc : ios::app);
            if(header){
                ThroughputCSVHeader(csvFile);
        }

            ThroughputCSVData(  csvFile,
                                toString(),
                                Q<UserData>::className(),
                                (producers + consumers),
                                additionalWork,
                                ringSize,
                                static_cast<uint64_t>(runDuration.count()),
                                numRuns,
                                sts);
            csvFile.close();
        }
    }

    string toString(){
        ostringstream benchmark;
        uint32_t prodConsGcd = GCD(producers,consumers);
        uint32_t prodRatio = producers / prodConsGcd;
        uint32_t consRatio = consumers / prodConsGcd;
        benchmark << "producerConsumer[" << prodRatio << "/" << consRatio << (balancedLoad? "|balanced":"") << "]";
        return benchmark.str();
    }

private:
template<template<typename> typename Q>
    vector<long double> __ProducerConsumer(seconds runDuration, size_t numRuns){
        Q<UserData>* queue = nullptr;
        barrier<> barrier(producers + consumers + 1);
        std::atomic<bool> stopFlag{false};
        pair<uint64_t,uint64_t> transferredCount[consumers][numRuns];

        const auto prod_lambda = [this,&stopFlag,&queue,&barrier](const int tid){
            UserData ud{};

            //Warmup Iterations
            barrier.arrive_and_wait();
            for(size_t iter = 0; iter < warmup; ++iter)
                queue->enqueue(&ud,tid);
            
            barrier.arrive_and_wait();
            //Queue Draining
            barrier.arrive_and_wait();
            uint64_t iter = 0;

            while(!stopFlag.load()){
                //If balance load we "slow down" producers every few iterations
                if constexpr((BoundedQueues::Append<LinkedMuxQueue>::template Contains<Q>))
                {  //BoundedQueues is problematic
                    if((iter &((1ull << 5)-1)) != 0 ||//every 31 iterations
                    !balancedLoad) {
                        queue->enqueue(&ud,tid);
                        ++iter;
                    }
                } 
                else{
                    if((iter &((1ull << 5)-1)) != 0 ||//every 31 iterations
                    !balancedLoad                   ||
                    (queue->size(tid) < queue->Ring_Size * 7 / 10)) {
                        queue->enqueue(&ud,tid);
                        ++iter;
                    }
                }
                random_additional_work(producerAdditionalWork);
                //random additional work
            }
        };

        const auto cons_lambda = [this,&stopFlag,&queue,&barrier](const int tid){
            UserData placeholder;
            uint64_t successfulDeqCount = 0;
            uint64_t failedDeqCount = 0;

            barrier.arrive_and_wait();
            //Warmup    Iterations
            for(size_t iter = 0; iter < warmup; ++iter){
                queue->dequeue(tid);
            }
            barrier.arrive_and_wait();
            //Drain the queue
            while(queue->dequeue(tid) != nullptr){}
            barrier.arrive_and_wait();
            while(!stopFlag.load()){
                UserData *d = queue->dequeue(tid);
                if(d != nullptr) {
                    ++successfulDeqCount;
                } else ++failedDeqCount;
                //random additional work;
                random_additional_work(consumerAdditionalWork);
            }
            return pair{successfulDeqCount, failedDeqCount};

        };

        nanoseconds deltas[numRuns];

        for(size_t iRun = 0; iRun < numRuns; iRun++){
            //Make a new Queue for every run
            queue = new Q<UserData>(producers + consumers, ringSize);
            ThreadGroup threads{};
            for(size_t iProd = 0; iProd < producers; iProd++){
                threads.thread(prod_lambda);
            }
            for(size_t iCons = 0; iCons < consumers; iCons++){
                threads.threadWithResult(cons_lambda,transferredCount[iCons][iRun]);
            }

            stopFlag.store(false);
            barrier.arrive_and_wait();  //warmup
            barrier.arrive_and_wait();  //drain queue
            barrier.arrive_and_wait();  //measurement

            auto startBeat = steady_clock::now();
            std::this_thread::sleep_for(runDuration);
            stopFlag.store(true);
            auto stopBeat = steady_clock::now();

            deltas[iRun] = duration_cast<nanoseconds>(stopBeat - startBeat);
            threads.join();
            while(queue->dequeue(0) != nullptr);
            delete (Q<UserData>*) queue;
        }

        //return value
        vector<long double> transfersPerSec(numRuns);

            for(size_t iRun = 0; iRun < numRuns; iRun++){
                uint64_t totalTransfersCount = 0;
                uint64_t totalFailedDeqCount = 0;
                for(size_t i = 0; i< consumers; ++i){
                    totalTransfersCount += transferredCount[i][iRun].first;
                    totalFailedDeqCount += transferredCount[i][iRun].second;
                }

                transfersPerSec[iRun] = static_cast<long double>(totalTransfersCount * NSEC_SEC) / deltas[iRun].count();

            }

        return transfersPerSec;
    }



public:
    template<template<typename> typename Q>
    static void runSeries  (std::string csvFileName,
                            const vector<size_t> producerSet,
                            const vector<size_t> consumerSet,
                                    vector<size_t> sizeSet,
                                    vector<double> workSet,
                                    vector<size_t> warmupSet,
                            const seconds runDuration,
                            const size_t numRuns,
                            const bool balancedLoad,
                            const Arguments args=Arguments())
    {
        if(sizeSet.empty()) sizeSet     = {RINGSIZE};
        if(workSet.empty()) workSet     = {0.0};
        if(warmupSet.empty()) warmupSet = {WARMUP};
        
        //CSV-HEADER
        bool header = args._overwrite || notFile(csvFileName);
        ofstream csvFile(csvFileName,header? ios::trunc : ios::app);
        if(header)
            ThroughputCSVHeader(csvFile);
        
        const int totalTests = producerSet.size() * consumerSet.size() * sizeSet.size() * workSet.size() * warmupSet.size();
        uint64_t runTime_sec = runDuration.count();
        uint64_t totalTimeInSec = totalTests * numRuns * runTime_sec;
        int iTest = 0;
        if(args._progress)
            cout    << "Time Remaining " << formatTime(totalTimeInSec) << "\n";
        
        for(double additionalWork : workSet) {
            for(size_t nProd: producerSet) {
                for(size_t nCons: consumerSet) {
                    for(size_t queueSize : sizeSet) {
                        for(size_t warmup : warmupSet){
                            PairsBenchmark bench(nProd,nCons,additionalWork,balancedLoad,queueSize,warmup,args);
                            std::vector<long double> result = bench.__ProducerConsumer<Q>(runDuration,numRuns);
                            Stats sts = stats(result.begin(),result.end());
                            ThroughputCSVData(  csvFile,
                                                bench.toString(),
                                                Q<UserData>::className(),
                                                nProd+nCons,
                                                additionalWork,
                                                queueSize,
                                                static_cast<uint64_t>(runTime_sec),
                                                numRuns,
                                                sts
                                                );
                            iTest++;
                            if((args._progress || args._stdout)){
                                if(args._progress){
                                    totalTimeInSec -= (runTime_sec*numRuns);
                                    cout    << "Executed " << iTest << " of " << totalTests << " runs\n";
                                    if(totalTimeInSec > 0)
                                    cout << "Time Remaining " << formatTime(totalTimeInSec) << "\n";
                                }
                                if(args._stdout)
                                    printBenchmarkResults(Q<UserData>::className(),"Transf/Sec",sts.mean,sts.stddev);
                            }
                        }
                    }
                }
            }
        }
       
    }


    template<template<typename> typename Q>
    static void runSeries  (std::string csvFileName,
                            const vector<size_t> threadSet,
                                    vector<int> ratioSet,
                                    vector<size_t> sizeSet,
                                    vector<double> workSet,
                                    vector<size_t> warmupSet,
                            const seconds runDuration,
                            const size_t numRuns,
                            const bool balancedLoad,
                            const Arguments args=Arguments())
    {

        if(sizeSet.empty()) sizeSet     = {RINGSIZE};
        if(workSet.empty()) workSet     = {0.0};
        if(warmupSet.empty()) warmupSet = {WARMUP};

        assert(ratioSet.size() == 2);
        
        //CSV-HEADER
        bool header = args._overwrite || notFile(csvFileName);
        ofstream csvFile(csvFileName,header? ios::trunc : ios::app);
        if(header)
            ThroughputCSVHeader(csvFile);
        
        const int totalTests = threadSet.size() * sizeSet.size() * workSet.size() * warmupSet.size();
        uint64_t runTime_sec = runDuration.count();
        uint64_t totalTimeInSec = totalTests * numRuns * runTime_sec;
        int iTest = 0;
        if(args._progress)
            cout    << "Time Remaining " << formatTime(totalTimeInSec) << "\n";

        size_t totalRatio = ratioSet[0] + ratioSet[1];
        
        for(double additionalWork : workSet) {
            for(size_t nThreads: threadSet) {
                for(size_t queueSize : sizeSet) {
                    for(size_t warmup : warmupSet){
                        nThreads /= totalRatio;
                        assert(nThreads != 0);
                        size_t nProd = nThreads * ratioSet[0];
                        size_t nCons = nThreads * ratioSet[1];

                        PairsBenchmark bench(nProd,nCons,additionalWork,balancedLoad,queueSize,warmup,args);
                        std::vector<long double> result = bench.__ProducerConsumer<Q>(runDuration,numRuns);
                        Stats sts = stats(result.begin(),result.end());
                        ThroughputCSVData(  csvFile,
                                            bench.toString(),
                                            Q<UserData>::className(),
                                            nProd+nCons,
                                            additionalWork,
                                            queueSize,
                                            static_cast<uint64_t>(runTime_sec),
                                            numRuns,
                                            sts
                                            );
                        iTest++;
                        if((args._progress || args._stdout)){
                            if(args._progress){
                                totalTimeInSec -= (runTime_sec*numRuns);
                                cout    << "Executed " << iTest << " of " << totalTests << " runs\n";
                                if(totalTimeInSec > 0)
                                cout << "Time Remaining " << formatTime(totalTimeInSec) << "\n";
                            }
                            if(args._stdout)
                                printBenchmarkResults(Q<UserData>::className(),"Transf/Sec",sts.mean,sts.stddev);
                        }
                    }
                }   
            }
        }
       
    }

    static void runSeries(Format format){
        for(string q : format.queueFilter){
            Queues::foreach([&q,&format]<template <typename> typename Q>() {
                string queue = Q<int>::className(false);
                if(q == queue)
                    if(format.ratio.size() == 2){
                        runSeries<Q>(   format.path,
                                        format.threads,format.ratio,
                                        format.sizes,format.additionalWork,
                                        format.warmup,seconds(format.iterations),
                                        format.runs,format.balanced,format.args);
                    }
                    else{
                    runSeries<Q>(   format.path,
                                    format.producers,format.consumers,
                                    format.sizes,format.additionalWork,
                                    format.warmup,seconds(format.iterations),
                                    format.runs,format.balanced,format.args);
                    }
            });
        }  
    }
};

}