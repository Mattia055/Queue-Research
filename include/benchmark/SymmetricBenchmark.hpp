#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <barrier>
#include <iostream>
#include <fstream>
#include <cassert>

#include "Benchmark.hpp"        //  for benchmarking Base Class
#include "ThreadGroup.hpp"      //  for thread management
#include "Stats.hpp"            //  for average and stddev computation
#include "AdditionalWork.hpp"   //  Additional Work by threads
#include "Format.hpp"
#include "QueueTypeSet.hpp"

using namespace std;
using namespace chrono;

namespace bench {

class SymmetricBenchmark: public Benchmark {
public:
    
    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result(){}
        Result(const Result& other){
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator<(const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
        bool operator>(const Result& other) const {
            return totOpsSec > other.totOpsSec;
        }
        bool operator==(const Result& other) const {
            return  (   (nsEnq   == other.nsEnq)        &&
                        (nsDeq   == other.nsDeq)        &&
                        (numEnq  == other.numEnq)       &&
                        (numDeq  == other.numDeq)       &&
                        (totOpsSec == other.totOpsSec)
                    );
        }
    };

// ===== CLASS ATTRIBUTES ===== //

public:
    Arguments flags;
    size_t threads;
    size_t ringSize = RINGSIZE;
    size_t warmup = WARMUP;
    double additionalWork;

    SymmetricBenchmark( size_t threads, 
                        double additionalWork, 
                        size_t ringSz,
                        size_t warmup, 
                        Arguments flags):
    threads{threads}, additionalWork{additionalWork}, ringSize{ringSz}, warmup{warmup}, flags{flags} {
        assert(threads > 0);
        assert(ringSize > 0);
    }

    SymmetricBenchmark( size_t threads, double additionalWork){
        SymmetricBenchmark(threads,additionalWork,RINGSIZE,WARMUP,Arguments());
    };
    ~SymmetricBenchmark(){};

    static string toString(){
        return "Symmetric";
    }

    template<template<typename> typename Q>
    void EnqueueDequeue(const size_t IterNum, const size_t numRuns, const std::string fileName = ""){
        auto res = __EnqDeqBenchmark<Q>(IterNum,numRuns);
        Stats<long double> sts = stats(res.begin(),res.end());

        if(flags._stdout){ 
            printBenchmarkResults(Q<UserData>::className(),"Ops/Sec",sts.mean,sts.stddev);
        }

        if(fileName != ""){
            bool header = flags._overwrite || notFile(fileName);

            ofstream csv(fileName,header? ios::trunc : ios::app);
            if(header){
                ThroughputCSVHeader(csv);
            }

            ThroughputCSVData(  csv,"EnqDec",
                                Q<UserData>::className(),
                                threads,additionalWork,
                                ringSize,IterNum,numRuns,
                                sts
                            );
            csv.close();
        }

    }

private:
    template<template<typename> typename Q>
    vector<long double>__EnqDeqBenchmark(const size_t IterNum, const size_t numRuns){
        nanoseconds deltas[threads][numRuns];
        barrier<> barrier(threads + 1);
        Q<UserData>* queue = nullptr;
        size_t IterPerThread = IterNum / threads;
        size_t WarmupPerThread = warmup / threads;

        bool constexpr bounded = BoundedQueues::Contains<Q>;

        //Threads Routine
        const auto lambda = [this,IterPerThread,WarmupPerThread, &queue,&barrier](const int tid) {
            UserData ud{};
            barrier.arrive_and_wait();
            //Warmup
            for(size_t iW = 0; iW <WarmupPerThread;++iW){
                if constexpr(bounded){
                    while(!queue->enqueue(&ud,tid));
                    while(queue->dequeue(tid) == nullptr);
                }
                else{
                    queue->enqueue(&ud,tid);
                    queue->dequeue(tid);
                }
            }

            barrier.arrive_and_wait();
            
            //Measuration
            auto startBeat = steady_clock::now();
            for(size_t iter = 0 ; iter < IterPerThread; ++iter) {
                if constexpr(bounded){
                    while(!queue->enqueue(&ud,tid));
                }
                else{
                    queue->enqueue(&ud,tid);
                }
                random_additional_work(additionalWork);
                if(queue->dequeue(tid) == nullptr)
                cout << "Error at measuration iter " << iter << endl;
            }
            auto stopBeat = steady_clock::now();

            return stopBeat - startBeat;
        };

        for(size_t iRun = 0; iRun < numRuns; iRun++){
            queue = new Q<UserData>(threads,ringSize);
            ThreadGroup threadSet{};
            for(size_t iThread = 0; iThread < threads; ++iThread)
                threadSet.threadWithResult(lambda,deltas[iThread][iRun]);
            barrier.arrive_and_wait();  //unlocks   Warmup
            barrier.arrive_and_wait();  //unlocks   Measurement
            threadSet.join();

            delete (Q<UserData>*) queue;
        }

        //finished compiling the delta matrix
        vector<long double> opsPerSec(numRuns);
        for(size_t iRun = 0; iRun < numRuns; ++iRun){
            auto agg = 0ns;
            for(size_t iThread = 0; iThread< threads; ++iThread){
                agg += deltas[iThread][iRun];
                opsPerSec[iRun] = static_cast<long double>(IterNum * 2 * (NSEC_SEC) * threads) / agg.count();
            }
        }
        return opsPerSec;

    }

public:
    template<template<typename> typename Q>
    static void runSeries  (const std::string csvFileName,
                            const vector<size_t> threadSet,
                                  vector<size_t> sizeSet,
                                  vector<double> workSet,
                                  vector<size_t> warmupSet,
                            const size_t IterNum,
                            const size_t numRuns,
                            const Arguments args=Arguments())
    {
        //CHECK ARGS
        if(sizeSet.empty())     sizeSet     = {RINGSIZE};
        if(warmupSet.empty())   warmupSet   = {WARMUP};
        if(workSet.empty())     workSet     = {0.0};

        //CSV-HEADER
        bool header = args._overwrite || notFile(csvFileName);
        ofstream csvFile(csvFileName,header? ios::trunc : ios::app);
        if(header){
            ThroughputCSVHeader(csvFile);
        }

        const int totalTests = threadSet.size() * sizeSet.size() * workSet.size() * warmupSet.size();
        int iTest = 0;

        for(double additionalWork : workSet) {
            for(size_t nThreads: threadSet) {
                for(size_t queueSize : sizeSet) {
                    for(size_t warmup : warmupSet){
                        SymmetricBenchmark bench(nThreads,additionalWork,queueSize,warmup,args);
                        std::vector<long double> result = bench.__EnqDeqBenchmark<Q>(IterNum,numRuns);
                        Stats<long double> sts = stats(result.begin(),result.end());
                        ThroughputCSVData(  csvFile,
                                            "EnqDec",
                                            Q<UserData>::className(),
                                            nThreads,
                                            additionalWork,
                                            queueSize,
                                            static_cast<uint64_t>(IterNum),
                                            numRuns,
                                            sts
                        );
                        iTest++;
                        if(args._progress)
                            cout << "Executed " << iTest << " of " << totalTests << " runs" << endl;
                        if(args._stdout)
                            printBenchmarkResults(Q<UserData>::className(),"Transf/Sec",sts.mean,sts.stddev); 
                    }
                }
            }
        }
    }

    static void runSeries   (Format format){
        for(string q : format.queueFilter){
            Queues::foreach([&format,&q]<template <typename> typename Q>() {
                string queue = Q<int>::className(false);
                    if(q == queue){
                        runSeries<Q>(   format.path,
                                        format.threads,
                                        format.sizes,format.additionalWork,format.warmup,format.iterations,format.runs,format.args);
                    }
            });
        }
    }


};

}