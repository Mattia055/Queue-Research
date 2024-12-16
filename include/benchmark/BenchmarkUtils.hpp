#pragma once
#include <cstdlib>
#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#include <chrono>


#include "Stats.hpp"

#ifndef NSEC_SEC
#define NSEC_SEC 1'000'000'000ULL
#endif

namespace bench {

using namespace std;
using namespace chrono;

// ===== MEMORY CONTROL =====

/**
 * Returns the total memor
 */

size_t getTotalMemory(bool includeSwap);

/**
 * Data that will be pushed into the queue;
 * 
 * All queues we're considering are queues
 * of pointer (or references) at UserData
 * data types
 */
struct UserData {

    int value = 0;
    int seq   = 0;

    UserData(int val, int seq): value{val},seq{seq}{};
    
    UserData(const UserData& other){
        value   = other.value;
        seq     = other.seq;
    }

    UserData(){};

    ~UserData(){};

    bool operator==(const UserData& other) const {
        return other.value == value && other.seq == seq;
    }

};


struct Arguments {
    bool _stdout            = true;
    bool _progress          = true;
    bool _overwrite         = false;
    bool _check_write       = true;
    bool _clear_terminal    = true;

    Arguments(bool f_stdout,bool f_progress,bool f_overwrite,bool f_check_write,bool f_clear):
    _stdout(f_stdout),_progress(f_progress),_overwrite(f_overwrite),
    _check_write(f_check_write),_clear_terminal(f_clear){} 

    Arguments(){} //default init;

    ~Arguments(){}
};

/**
 * Memory Arguments that can be given in order control the 
 * behaviour of threads like:
 * - sleeping if overproducing
 * - sleeping if overconsuming
 * 
 * - synchronized sleep of producers
 * - synchronized sleep of consumers
 * 
 */
struct MemoryArguments {

    size_t max_memory;
    size_t min_memory;
    milliseconds supSleep;  //producers sleep if max_memory is reached
    milliseconds infSleep;  //consumers sleep if min_memory is reached
    // synchronized sleep
    size_t produceBeforeSleep;
    size_t consumeBeforeSleep;
    milliseconds producerSleep;     //sleep after producing i items
    milliseconds consumerSleep;     //sleep after consuming i items

    
    MemoryArguments(milliseconds supSleepProducer = 100ms,
                    milliseconds infSleepConsumer = 100ms,
                    size_t max_memory = getTotalMemory(true)*(2/3),
                    size_t min_memory = 0
                    ):  max_memory{max_memory},min_memory{min_memory},
                        supSleep{supSleepProducer},infSleep{infSleepConsumer}
    {};

};


uint32_t __GCD(size_t a, size_t b);

uint32_t GCD(size_t a, size_t b);

// ===== CSV & FORMAT =====
void ThroughputCSVHeader(std::ostream& stream);

void ThroughputCSVData( std::ostream& stream,
                        std::string_view benchmark,
                        std::string_view queueType,
                        size_t threads,
                        double additionalWork,
                        size_t ring_size,
                        uint64_t duration,
                        size_t iterations,
                        const Stats<long double> stats);

string formatDigits(uint64_t n);

string formatTime(int totalSeconds);

template<typename V>
inline void printBenchmarkResults(string title, string resultLabel, const V& mean, const V& stddev) {
    size_t boxWidth     = 40; // Total width of the box
    size_t labelWidth   = 20; // Width for labels (Ops/Sec, Stddev)
    size_t valueWidth   = 20; // Width for values

    // Print the top line of # symbols
    std::cout << std::string(boxWidth, '#') << std::endl;

    // Print the title centered within the box
    size_t spacesBeforeTitle = (boxWidth - title.length()) / 2;
    std::cout << std::setw(spacesBeforeTitle) << "" << title << std::endl;

    // Print the same line of # symbols below the title
    std::cout << std::string(boxWidth, '#') << std::endl;

    // Print results inside the box
    std::cout   << std::left
                << std::setw(labelWidth) << resultLabel  // Left-aligned label for Ops/Sec
                << std::right << std::setw(valueWidth) << std::fixed << std::setprecision(0)
                << formatDigits(static_cast<uint64_t>(mean))  // Right-aligned value for Ops/Sec
                << std::endl
                << std::left
                << std::setw(labelWidth) << "Stddev"  // Left-aligned label for Stddev
                << std::right << std::setw(valueWidth) << formatDigits(static_cast<uint64_t>(stddev))  // Right-aligned value for Stddev
                << std::endl;

    // Print the bottom line of # symbols (to close the box)
    std::cout << std::string(boxWidth, '#') << std::endl;
}


// ===== FILE SYSTEM =====
bool notFile(const string filename);

void clearTerminal();

}

