#pragma once
#include <cstdint>
#include <chrono>
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include "Stats.hpp"


namespace bench{

using namespace std;
using namespace chrono;

class Benchmark{

protected:
    static constexpr uint64_t NSEC_SEC = 1'000'000'000ULL;
    static constexpr uint64_t WARMUP = 1'000'000UL;
    static constexpr uint64_t RINGSIZE = 4096;



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


public:

struct Arguments {
    bool _stdout            = true;
    bool _progress          = true;
    bool _overwrite         = false;

    Arguments(bool f_stdout,bool f_progress,bool f_overwrite,bool f_check_write,bool f_clear):
    _stdout(f_stdout),_progress(f_progress),_overwrite(f_overwrite)
    {}

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

protected:

template<typename V>
static inline void printBenchmarkResults(string title, string resultLabel, const V& mean, const V& stddev) {
    size_t boxWidth     = 40; // Total width of the box
    size_t labelWidth   = 20; // Width for labels (Ops/Sec, Stddev)
    size_t valueWidth   = 20; // Width for values

    using namespace std;

    // Print the top line of # symbols
    cout << string(boxWidth, '#') << "\n";

    size_t spacesBeforeTitle = (boxWidth - title.length()) / 2;
    cout << setw(spacesBeforeTitle) << "" << title << "\n";

    cout << string(boxWidth, '#') << "\n";

    cout    << left
            << setw(labelWidth) << resultLabel
            << right << setw(valueWidth) << fixed << setprecision(0)
            << formatDigits(static_cast<uint64_t>(mean))
            << "\n"
            << left
            << setw(labelWidth) << "Stddev"
            << right << setw(valueWidth) << formatDigits(static_cast<uint64_t>(stddev))
            << "\n";

    // Print the bottom line of # symbols (to close the box)
    cout << string(boxWidth, '#') << endl;
}

private: 
uint32_t __GCD(size_t a, size_t b){
    return (b==0)? a : __GCD(b,a % b);
}

public :

uint32_t GCD(size_t a, size_t b){
    return (a > b)? __GCD(a,b) : __GCD(b,a);
}

protected:

static void ThroughputCSVHeader(std::ostream& stream){
    stream << "Benchmark,QueueType,Threads,AdditionalWork,RingSize,"
           << "Duration,Iterations,Score,ScoreError" << endl;
} 

static void ThroughputCSVData( std::ostream& stream,
                        std::string_view benchmark,
                        std::string_view queueType,
                        size_t threads,
                        double additionalWork,
                        size_t ring_size,
                        uint64_t duration,
                        size_t iterations,
                        const Stats<long double> stats) 
{
    stream  << benchmark << "," << queueType << "," << threads << "," << additionalWork
            << "," << ring_size << "," << duration << "," << iterations << "," <<
            static_cast<uint64_t>(stats.mean) << "," << static_cast<uint64_t>(stats.stddev) << endl;  
}

// Assuming Q<UserData>::className() returns a string and sts.mean and sts.stddev are numeric types
static string formatDigits(uint64_t n){
    std::string numStr = std::to_string(n);  // Convert integer to string
    int length = numStr.length();
    
    // Start from the second-to-last digit and move backward
    for (int i = length - 3; i > 0; i -= 3) {
        numStr.insert(i, ".");  // Insert a dot every 3 digits
    }
    return numStr;
}


static string formatTime(int totalSeconds) {
    int hours = totalSeconds / 3600;            // 1 hour = 3600 seconds
    int minutes = (totalSeconds % 3600) / 60;   // 1 minute = 60 seconds
    int seconds = totalSeconds % 60;            // remaining seconds

    // Create the formatted string with leading zeros for minutes and seconds
    std::stringstream result;
    result << hours << "hh:" 
           << std::setw(2) << std::setfill('0') << minutes << "mm:" 
           << std::setw(2) << std::setfill('0') << seconds << "ss";
    
    return result.str();
}

public:
static inline bool notFile(const string filename) {
    // Check if file exists
    std::ifstream file(filename);
    if (!file) {
        return true;  // File doesn't exist or cannot be opened for reading
    }

    // Use stat to check the file size (for any file, including empty ones)
    struct stat fileStat;
    if (stat(filename.c_str(), &fileStat) != 0) {
        return true;  // Error checking file status
    }

    // If the file size is 0, it is empty
    return fileStat.st_size == 0;
}

};

} //namespace bench