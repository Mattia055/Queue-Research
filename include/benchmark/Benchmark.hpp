#pragma once
#include <cstdint>
#include <chrono>
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include "Stats.hpp"
#include <type_traits>
#include <iomanip>


#define __delim_1 ':'
#define __delim_2 ','
#define __NULL_ARG string("NULL")


namespace bench{

using namespace std;
using namespace chrono;

class Benchmark{
protected:
    static constexpr uint64_t NSEC_SEC = 1'000'000'000ULL;
    static constexpr uint64_t WARMUP = 1'000'000UL;
    static constexpr uint64_t RINGSIZE = 4096;
public:
struct Arguments {
    bool _stdout            = true;
    bool _progress          = true;
    bool _overwrite         = false;

    Arguments(bool f_stdout,bool f_progress,bool f_overwrite):
    _stdout(f_stdout),_progress(f_progress),_overwrite(f_overwrite)
    {}
    Arguments(){};
    ~Arguments(){};
};

struct MemoryArguments {
    private:
    size_t max_memory;
    size_t min_memory;
    milliseconds supSleep;  //producers sleep if max_memory is reached
    milliseconds infSleep;  //consumers sleep if min_memory is reached
    short synchro;      //Bitmask for synchro
    //If synchro = 0 then all the following are ignored [default]
    milliseconds producerInitialDelay;
    milliseconds consumerInitialDelay;
    milliseconds producerSleep; //should be multiple of granularity
    milliseconds producerUptime;
    milliseconds consumerSleep;
    milliseconds consumerUptime;

    static constexpr short SYNC_NONE = 0;
    static constexpr short SYNC_PRODS = 1;
    static constexpr short SYNC_CONS = 2;

    public:
    MemoryArguments(){
        max_memory  = std::numeric_limits<size_t>::max();
        min_memory  = 0;
        supSleep    = 0ms;
        infSleep    = 0ms;
        synchro     = SYNC_NONE;
    };

    void setMaxMonitor(size_t maxMemory,milliseconds supSlp){
        if(supSlp.count() == 0)
            throw std::invalid_argument("SupSleep must be greater than 0");
        max_memory = maxMemory;
        supSleep = supSlp;
    }
    void setMinMonitor(size_t minMemory,milliseconds infSlp){
        if(infSlp.count() == 0)
            throw std::invalid_argument("InfSleep must be greater than 0");
        min_memory = minMemory;
        infSleep = infSlp;
    }
    void setProducerSync(milliseconds initialDelay,milliseconds uptime,milliseconds sleep){
        synchro = synchro | SYNC_PRODS;
        producerInitialDelay = initialDelay;
        producerUptime = uptime;
        producerSleep = sleep;
    }

    void setConsumerSync(milliseconds initialDelay,milliseconds uptime,milliseconds sleep){
        synchro = synchro | SYNC_CONS;
        consumerInitialDelay = initialDelay;
        consumerUptime = uptime;
        consumerSleep = sleep;
    }

    std::string printFormat(string lineStart = string()){
        using namespace string_literals;

        std::stringstream ss;
        string endline = "\n"+lineStart;

        if(supSleep.count() + infSleep.count() > 0){
            ss  << "Max allowed memory: " << max_memory << " kB" << endline
                << "Min allowed memory: " << min_memory << " kB" << endline
                << "Wait above max: " << supSleep.count() << " ms" << endline
                << "Wait below min: " << infSleep.count() << " ms" << endline;
        }
        if(synchro & SYNC_PRODS != 0){
            //ss << "Producers synchronized" << endline
            ss  << "====== Producers Synchro ======" << endline
                << "Init delay: " << producerInitialDelay.count() << " ms" << endline
                << "Uptime: " << producerUptime.count() << " ms" << endline
                << "Sleep: " << producerSleep.count() << " ms" << endline;
        }
        if(synchro & SYNC_CONS != 0){
            ss  << "====== Consumers Synchro ======" << endline
                << "Init delay: " << consumerInitialDelay.count() << " ms" << endline
                << "Uptime: " << consumerUptime.count() << " ms" << endline
                << "Sleep: " << consumerSleep.count() << " ms";
        }
        
        string result = ss.str();
        string start = lineStart + "====== Memory Parameters ======"s + endline;
        return result.length() == 0? "" : start + result;
    }

};

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
    stream  << "Benchmark,QueueType,Threads,AdditionalWork,RingSize,"
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
            static_cast<uint64_t>(stats.mean) << "," << static_cast<long double>(stats.stddev) << endl;  
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

    // Create the formatted string without leading zeros for hours, minutes, and seconds
    std::stringstream result;
    if (hours > 0)
        result << hours << "hh:";
    if (minutes > 0)
        result << std::setw(2) << std::setfill('0') << minutes << "mm:";
    if (seconds > 0)
        result << std::setw(2) << std::setfill('0') << seconds << "ss";
    
    return result.str();
}

public:
static bool fileExists(const string filename) {
    // Check if file exists
    std::ifstream file(filename);
    if (file) {
        return true;  // File doesn't exist or cannot be opened for reading
    }

    // Use stat to check the file size (for any file, including empty ones)
    struct stat fileStat;
    if (stat(filename.c_str(), &fileStat) == 0) {
        return true;  // Error checking file status
    }

    // If the file size is 0, it is empty
    return fileStat.st_size != 0;
}
static inline size_t getTotalMemory(bool includeSwap) {
        using namespace std;
#ifndef __linux__
        cerr << "Can't get total memory : only linux systems" << endl;
#ifdef SYS_CAUTIOUS
        exit(0);
#else
        return 0;
#endif
#endif

        ifstream memInfo("/proc/meminfo");
        if (!memInfo.is_open()) {
            std::cerr << "Failed to open /proc/meminfo" << std::endl;
#ifdef SYS_CAUTIOUS
            exit(-1);
#endif
            return 0;
        }

        size_t totalMemory = 0;
        size_t totalSwap = 0;
        string line;
        
        while (getline(memInfo, line)) {
            istringstream iss(line);
            string key;
            size_t value;

            // Read the first word (key) and the second number (value)
            if (iss >> key >> value) {
                if (key == "MemTotal:") {
                    totalMemory = value;  // In KB
                    if(!includeSwap) break;
                } else if (key == "SwapTotal:") {
                    totalSwap = value;  // In KB
                }
            }
        }

        // Close the file
        memInfo.close();

        // Return total memory considering the swap flag
        return includeSwap? totalMemory + totalSwap : totalMemory;
    }

};

} //namespace bench