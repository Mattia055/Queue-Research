#include <iostream>
#include <vector>
#include <chrono>
#include <unistd.h>
#include <string>
#include <array>

#include "FAArray.hpp"
#include "LPRQ.hpp"
#include "LCRQ.hpp"
#include "SymmetricBenchmark.hpp"
#include "MuxQueue.hpp"
#include "ProdConsBenchmark.hpp"
#include "MemoryBenchmark.hpp"
#include "QueueTypeSet.hpp"

#define relativePath "../result" // from build to project root

using namespace std;
using namespace chrono;
using namespace bench;

int main(void)
{
    // SymmetricBenchmark sym1(2, 0, 4096);

    // // Iterate through each template in the set
    // UnboundedQueues::foreach([&sym1]<template <typename> typename Queue>() {
    //     sym1.EnqueueDequeue<Queue>(10, 10, "EnqDeq.csv");
    // });

    // return 0;

    string fileEnqDeq = "../result/EnqDeq.csv";
    string fileEnqDeqAdditional = "../result/EnqDeqAdditional.csv";
    string fileProdCons = "../result/ProdCons[1:1].csv";
    string fileProdConsAdditional = "../result/ProdCons[1:1]Additional.csv";
    string fileProdCons2 = "../result/ProdCons[1:2].csv";
    string fileProdCons2Additional = "../result/ProdCons[1:2]Additional.csv";
    string fileProdCons3 = "../result/ProdCons[2:1].csv";
    string fileProdCons3Additional = "../result/ProdCons[2:1]Additional.csv";
    string fileMemory = "../result/MemoryBenchmark.csv";
    string fileMemory1 = "../result/MemoryBenchmarkAdditional.csv";

    array<size_t, 8> threadSet{1, 2, 4, 8, 16, 32, 64, 128};
    array<size_t, 7> threadSetUnbalanced{3, 6, 12, 24, 48, 96, 126};
    size_t length = 4096 * 2;
    size_t iter = 10'000'000;
    size_t runs = 50;

    for (size_t thread : threadSet)
    {
        SymmetricBenchmark sym1(thread, 2, length);
        sym1.EnqueueDequeue<BoundedMuxQueue>(iter, runs, fileEnqDeqAdditional);
        sym1.EnqueueDequeue<LinkedMuxQueue>(iter, runs, fileEnqDeqAdditional);
        sym1.EnqueueDequeue<FAAQueue>(iter, runs, fileEnqDeqAdditional);
        sym1.EnqueueDequeue<LPRQueue>(iter, runs, fileEnqDeqAdditional);
    }

    // Producer Consumer 1:1
    runs = 10;
    milliseconds duration = duration_cast<milliseconds>(seconds{10});
    for (size_t i = 0; i < threadSet.size() - 1; i++)
    {
        ProdConsBenchmark pc1(threadSet[i], threadSet[i], 0, false, length);
        pc1.ProducerConsumer<LCRQueue>(duration, runs, fileProdCons);
        pc1.ProducerConsumer<FAAQueue>(duration, runs, fileProdCons);
        pc1.ProducerConsumer<LPRQueue>(duration, runs, fileProdCons);
    }

    for (size_t i = 0; i < threadSet.size() - 1; i++)
    {
        ProdConsBenchmark pc1(threadSet[i], threadSet[i], 2, false, length);
        pc1.ProducerConsumer<LPRQueue>(duration, runs, fileProdConsAdditional);
        pc1.ProducerConsumer<BoundedPRQueue>(duration, runs, fileProdConsAdditional);
        pc1.ProducerConsumer<LPRQueue>(duration, runs, fileProdConsAdditional);
    }

    // Producer Consumer 1:2
    for (size_t thread : threadSetUnbalanced)
    {
        size_t producer = thread / 3;
        size_t consumer = thread - producer;
        ProdConsBenchmark pc1(producer, consumer, 0, false, length);
        pc1.ProducerConsumer<BoundedMuxQueue>(duration, runs, fileProdCons2);
        pc1.ProducerConsumer<LCRQueue>(duration, runs, fileProdCons2);
        pc1.ProducerConsumer<FAAQueue>(duration, runs, fileProdCons2);
        pc1.ProducerConsumer<LPRQueue>(duration, runs, fileProdCons2);
    }

    for (size_t thread : threadSetUnbalanced)
    {
        size_t producer = thread / 3;
        size_t consumer = thread - producer;
        ProdConsBenchmark pc1(producer, consumer, 2, false, length);
        pc1.ProducerConsumer<BoundedMuxQueue>(duration, runs, fileProdCons2Additional);
        pc1.ProducerConsumer<LCRQueue>(duration, runs, fileProdCons2Additional);
        pc1.ProducerConsumer<FAAQueue>(duration, runs, fileProdCons2Additional);
        pc1.ProducerConsumer<LPRQueue>(duration, runs, fileProdCons2Additional);
    }

    // Producer Consumer 2:1
    for (size_t thread : threadSetUnbalanced)
    {
        size_t producer = thread * 2 / 3;
        size_t consumer = thread - producer;
        ProdConsBenchmark pc1(producer, consumer, 0, false, length);
        pc1.ProducerConsumer<BoundedMuxQueue>(duration, runs, fileProdCons3);
        pc1.ProducerConsumer<LCRQueue>(duration, runs, fileProdCons3);
        pc1.ProducerConsumer<FAAQueue>(duration, runs, fileProdCons3);
        pc1.ProducerConsumer<LPRQueue>(duration, runs, fileProdCons3);
    }

    for (size_t thread : threadSetUnbalanced)
    {
        size_t producer = thread * 2 / 3;
        size_t consumer = thread - producer;
        ProdConsBenchmark pc1(producer, consumer, 2, false, length);
        pc1.ProducerConsumer<BoundedMuxQueue>(duration, runs, fileProdCons3Additional);
        pc1.ProducerConsumer<LCRQueue>(duration, runs, fileProdCons3Additional);
        pc1.ProducerConsumer<FAAQueue>(duration, runs, fileProdCons3Additional);
        pc1.ProducerConsumer<LPRQueue>(duration, runs, fileProdCons3Additional);
    }

    // Memory Benchmark
    MemoryBenchmark::MemoryArguments args;
    args.synchro = MemoryBenchmark::SYNC_PRODS | MemoryBenchmark::SYNC_CONS;
    args.producerInitialDelay = 10000ms;
    args.consumerInitialDelay = 0ms;
    args.producerSleep = 10000ms;
    args.consumerSleep = 10000ms;
    args.consumerUptime = 10000ms;
    args.producerUptime = 10000ms;

    MemoryBenchmark mem1(5, 5, 0, false, 4096 * 4, args);
    mem1.MemoryRun<LCRQueue>(seconds{360}, milliseconds{500}, fileMemory);

    return 0;
}
