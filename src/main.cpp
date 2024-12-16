#include <iostream>
#include <vector>
#include <chrono>
#include <unistd.h>
#include <string>

#include "FAArray.hpp"
#include "LPRQ.hpp"
#include "LCRQ.hpp"
#include "SymmetricBenchmark.hpp"
#include "MuxQueue.hpp"
#include "ProdConsBenchmark.hpp"

using namespace std;
using namespace chrono;
using namespace bench;

// // Define the global variables
// size_t __totalMemory = 0;
// // Using the custom hash function for void* (PointerHash struct)
// std::unordered_map<size_t, size_t> __allocationSizes;
// std::mutex memory_mutex;

int main(void){
    //bench::SymmetricBenchmark test1(128,1280,1);
    //test1.flags._stdout = true;
    //std::cout << LCRQueue<bench::UserData>::className() << endl;
    vector<size_t> producerSet{1,2,4};  //Quando finiscono devono fare il test i consumer set
    vector<size_t> consumerSet{1,2,4,8,16,32,64,128};
    vector<size_t> sizeSet{32,64,128,256,512,1024,2048,4096,4096*2};
    vector<double> workSet{0.5};
    vector<size_t> warmupSet{1'000'000UL};
    //bench::MemoryBenchmark::MemoryArguments args();
    //mem1.MemoryRun<LCRQueue>("LPRQ.mem",2048,milliseconds{15000},milliseconds{100});
    string fileName = "./results/Symmetric.csv";
    string fileName1= "./results/Prod[1:1].csv";

    size_t iter = 3'000'000;
    size_t runs = 25;

    double additional = 0;

    for(size_t size : sizeSet)
    for(size_t thread : producerSet){
        SymmetricBenchmark test1(thread,additional,size);
        test1.EnqueueDequeue<MutexBasedQueue>(iter,runs,fileName);
        test1.EnqueueDequeue<LCRQueue>(iter,runs,fileName);
        test1.EnqueueDequeue<LPRQueue>(iter,runs,fileName);
        test1.EnqueueDequeue<FAAQueue>(iter,runs,fileName);
    }

    milliseconds iterations = 4000ms;
    runs = 5;

    for(size_t size : sizeSet)
    for(size_t thread : consumerSet){
        ProdConsBenchmark test1(thread,thread,0,true,size);
        test1.ProducerConsumer<MutexBasedQueue>(iterations,runs,fileName1);
        test1.ProducerConsumer<LCRQueue>(iterations,runs,fileName1);
        test1.ProducerConsumer<LPRQueue>(iterations,runs,fileName1);
        test1.ProducerConsumer<FAAQueue>(iterations,runs,fileName1);
    }
    
    cout << "FINISHED" <<endl;

    return 0;

}

