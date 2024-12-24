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
#include "Stats.hpp"            //  for average and stddev computation
#include "ThreadGroup.hpp"      //  for thread scheduling
#include "AdditionalWork.hpp"   //  Additional Work by threads

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
    threads{threads},
    additionalWork{additionalWork},
    ringSize{ringSz},
    warmup{warmup},
    flags{flags} {};

    SymmetricBenchmark( size_t threads,
                        double additionalWork):
    threads{threads},additionalWork{additionalWork}{}; 

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
        assert(IterNum / threads > 0);
        nanoseconds deltas[threads][numRuns];
        barrier<> barrier(threads + 1);
        Q<UserData>* queue = nullptr;

        //Threads Routine
        const auto lambda = [this,&IterNum, &queue,&barrier](const int tid) {
            UserData ud{};
            barrier.arrive_and_wait();
            
            //Warmup Iterations
            for(size_t iW = 0; iW < warmup/threads;++iW){
                queue->enqueue(&ud,tid);
                if(queue->dequeue(tid) == nullptr) 
                    cerr << "Error at warmup iteration: "<< iW << endl;
            }

            barrier.arrive_and_wait();
            
            //Measuration
            auto startBeat = steady_clock::now();
            for(size_t iter = 0; iter < IterNum / threads; ++iter) {
                queue->enqueue(&ud,tid);
                random_additional_work(additionalWork);
                if(queue->dequeue(tid) == nullptr)
                    cerr << "Error at measurement iteration: " << iter << endl;
                random_additional_work(additionalWork);
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
    static void ThroughputGrid( std::string csvFileName,
                                const vector<size_t>& threadSet,
                                const vector<size_t>& sizeSet,
                                const vector<double>& workSet,
                                const vector<size_t>& warmupSet,
                                const size_t IterNum,
                                const size_t numRuns,
                                const Arguments args=Arguments())
    {
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
                        SymmetricBenchmark bench(nThreads,queueSize,additionalWork,warmup,args);
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
};

}