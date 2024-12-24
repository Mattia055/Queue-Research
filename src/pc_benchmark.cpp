#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "Benchmark.hpp"
#include "SymmetricBenchmark.hpp"
#include "ProdConsBenchmark.hpp"
#include "MemoryBenchmark.hpp"
#include "QueueTypeSet.hpp"
#include <string.h>

using bench::Benchmark;

/**
 * 
 */
template <typename T>
std::vector<T> split_and_cast(const std::string& input, char delimiter);

template <typename T>
void print_vector(const std::vector<T>& vec);

Benchmark::Arguments parseArguments(string args);


int main(int argc, char **argv) {
    using namespace bench;
    if(argc != 11)
        throw std::invalid_argument("Usage: TODO LATER");
    
    std::vector<std::string> queues     = split_and_cast<std::string>(argv[2], ':');
    std::vector<size_t> threadSet       = split_and_cast<size_t>(argv[3], ':');
    std::vector<double> additionalSet   = split_and_cast<double>(argv[4], ':');
    std::string file                    = argv[5];
    std::vector<size_t> iterSet         = split_and_cast<size_t>(argv[6], ':');
    std::vector<size_t> runSet          = split_and_cast<size_t>(argv[7], ':');
    std::vector<size_t> sizeSet         = split_and_cast<size_t>(argv[8], ':');
    std::vector<size_t> warmupSet       = split_and_cast<size_t>(argv[9], ':');


    for(std::string queue : queues){
        for(size_t nThreads : threadSet){
            for(double additionalWork : additionalSet){
                for(size_t iterNum : iterSet){
                    for(size_t numRuns : runSet){
                        for(size_t queueSize : sizeSet){
                            for(size_t warmup : warmupSet){
                                SymmetricBenchmark sym1(nThreads,additionalWork);
                                if(queueSize != 0)
                                sym1.ringSize = queueSize;
                                if(warmup != 0)
                                sym1.warmup = warmup;
                                if(strcmp(argv[10],"NULL")!=0)
                                sym1.flags = parseArguments(argv[10]);

                                // Iterate through each template in the set
                                Queues::foreach([&sym1,file,queue,iterNum,numRuns]<template <typename> typename Queue>() {
                                    if(queue == (Queue<int>::className())){
                                        sym1.EnqueueDequeue<Queue>(iterNum,numRuns,file);
                                    }
                                });
                            }
                        }
                    }
                }
            }
        }
    }
    

    return 0;
}

// Template function to split the string and cast elements to the given type
template <typename T>
std::vector<T> split_and_cast(const std::string& input, char delimiter) {
    std::istringstream stream(input);
    std::string item;
    std::vector<T> result;
    
    while (std::getline(stream, item, delimiter)) {
        // Cast the item to the specified type
        std::istringstream item_stream(item);
        T value;
        item_stream >> value;
        
        if (item_stream.fail()) {
            throw std::invalid_argument("Failed to cast item: " + item);
        }

        result.push_back(value);
    }

    return result;
}

template <typename T>
void print_vector(const std::vector<T>& vec) {
    std::cout << "[ ";
    for (const auto& item : vec) {
        std::cout << item << " ";
    }
    std::cout << "]" << std::endl;
}

Benchmark::Arguments parseArguments(string args){
    Benchmark::Arguments flags;
    std::vector<bool> arguments = split_and_cast<bool>(args, ':');
    flags._stdout = arguments[0];
    flags._progress = arguments[1];
    flags._overwrite = arguments[2];
    return flags;
}
                