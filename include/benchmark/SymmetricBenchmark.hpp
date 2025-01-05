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

namespace bench {

class SymmetricBenchmark: public Benchmark {
// ===== CLASS ATTRIBUTES ===== //

public:
    Arguments flags;
    size_t threads;
    size_t ringSize;
    size_t warmup;
    double additionalWork;

    SymmetricBenchmark( size_t threads_par, 
                        double additionalWork_par, 
                        size_t ringSz,
                        size_t warmup_par, 
                        Arguments flags):
    threads{threads_par}, additionalWork{additionalWork_par}, ringSize{ringSz}, warmup{warmup_par}, flags{flags} {
        //parameter check
        if(threads == 0)        throw invalid_argument("Threads must be greater than 0");
        if(ringSize == 0)       throw invalid_argument("Ring Size must be greater than 0");
        if(additionalWork < 0)  throw invalid_argument("Additional Work must be greater than 0");
    }

    SymmetricBenchmark( size_t threads, double additionalWork):
        SymmetricBenchmark(threads,additionalWork,RINGSIZE,WARMUP,Arguments()){};
    ~SymmetricBenchmark(){};

    static string toString(){
        return "Symmetric";
    }

    template<template<typename> typename Q>
    void EnqueueDequeue(const size_t IterNum, const size_t numRuns, const std::string fileName = ""){
        auto res = __EnqDeqBenchmark<Q>(IterNum,numRuns);
        Stats<long double> sts = stats(res.begin(),res.end());  //Average and Stddev over all runs

        if(flags._stdout){ 
            printBenchmarkResults(Q<UserData>::className(),"Ops/Sec",sts.mean,sts.stddev);
        }

        if(fileName != ""){
            bool header = flags._overwrite || !fileExists(fileName);
            ofstream csv(fileName, header? ios::trunc : ios::app);

            if(header)ThroughputCSVHeader(csv);
            

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
        using namespace std;
        using namespace chrono;
        nanoseconds deltas[threads][numRuns];
        barrier<> barrier(threads + 1);    //barrier for threads (used only once)
        Q<UserData>* queue = nullptr;
        size_t IterPerThread    = IterNum / threads;
        assert(IterPerThread > 0);
        size_t WarmupPerThread  = warmup / threads; //Warmup can be equal to 0
        atomic<size_t> warmupCounter{0};

        bool constexpr bounded = BoundedQueues::Contains<Q>;    //checks if the queue is bounded

        //Threads Routine
        const auto lambda = [this,IterPerThread,WarmupPerThread,&queue,&barrier,&warmupCounter](const int tid) {
            UserData ud{};
            barrier.arrive_and_wait();
            //Warmup Iterations
            for( size_t iW = 0; iW < WarmupPerThread; ++iW ){
                if constexpr(bounded){  //if queue is bounded then checks if operation succeed
                    while(!queue->push(&ud,tid));
                    if(queue->pop(tid) == nullptr)
                        cerr << "Error at warmup iter " << iW << "\n";
                }
                else{
                    queue->push(&ud,tid);
                    queue->pop(tid);
                }
            }
            warmupCounter.fetch_add(1);
            while(warmupCounter.load() != threads){} //Spin until the flag is set
            
            //Measuration Iterations
            auto startBeat = steady_clock::now();
            for(size_t iter = 0 ; iter < IterPerThread; ++iter) {
                if constexpr(bounded){
                    while(!queue->push(&ud,tid));
                }
                else{
                    queue->push(&ud,tid);
                }
                random_additional_work(additionalWork);
                if(queue->pop(tid) == nullptr)
                    cerr << "Error at measuration iter " << iter << "\n";
            }
            auto stopBeat = steady_clock::now();

            return stopBeat - startBeat;
        };

        //Iterates over all runs
        for(size_t iRun = 0; iRun < numRuns; iRun++){
            queue = new Q<UserData>(ringSize,threads);
            ThreadGroup threadSet{};
            for(size_t iThread = 0; iThread < threads; ++iThread)
                threadSet.threadWithResult(lambda,deltas[iThread][iRun]);
            barrier.arrive_and_wait();      //unlocks Warmup
            threadSet.join();
            warmupCounter.store(0);         //resets warmupCounter
            delete (Q<UserData>*) queue;
        }

        //Compute the delta Matrix
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
        bool header = args._overwrite || !fileExists(csvFileName);
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

    /*
        I should be able to reduce this interface to a standard format like a std::map
    */
    static void runSeries   (Format format){    //DEBUG: look at earlier comment Do it later
        //here i have to integrate the json object handling for the format
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

    // EXPERIMENTAL_CODE
    // struct Result {
    //     nanoseconds nsEnq = 0ns;
    //     nanoseconds nsDeq = 0ns;
    //     long long numEnq = 0;
    //     long long numDeq = 0;
    //     long long totOpsSec = 0;

    //     Result(){}
    //     Result(const Result& other){
    //         nsEnq = other.nsEnq;
    //         nsDeq = other.nsDeq;
    //         numEnq = other.numEnq;
    //         numDeq = other.numDeq;
    //         totOpsSec = other.totOpsSec;
    //     }

    //     bool operator<(const Result& other) const {
    //         return totOpsSec < other.totOpsSec;
    //     }
    //     bool operator>(const Result& other) const {
    //         return totOpsSec > other.totOpsSec;
    //     }
    //     bool operator==(const Result& other) const {
    //         return  (   (nsEnq   == other.nsEnq)        &&
    //                     (nsDeq   == other.nsDeq)        &&
    //                     (numEnq  == other.numEnq)       &&
    //                     (numDeq  == other.numDeq)       &&
    //                     (totOpsSec == other.totOpsSec)
    //                 );
    //     }
    // };

};

}