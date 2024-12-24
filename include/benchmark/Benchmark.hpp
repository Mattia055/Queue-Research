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
#include "MemoryBenchmark.hpp"
#include <type_traits>


#define __delim_1 ':'
#define __delim_2 ','
#define __NULL_ARG string("NULL")


namespace bench{

using namespace std;
using namespace chrono;

class Benchmark{
public:

struct Arguments {
    bool _stdout            = false;
    bool _progress          = false;
    bool _overwrite         = false;

    Arguments(bool f_stdout,bool f_progress,bool f_overwrite):
    _stdout(f_stdout),_progress(f_progress),_overwrite(f_overwrite)
    {}
    Arguments(){} //default init;
    ~Arguments(){}
};

struct MemoryArguments {
    bool memoryThreshold = false;
    size_t max_memory = MemoryBenchmark::getTotalMemory(false) / 3 * 2;
    size_t min_memory = 0;
    milliseconds supSleep = 100ms;  //producers sleep if max_memory is reached
    milliseconds infSleep = 100ms;  //consumers sleep if min_memory is reached
    short synchro = 0;      //Bitmask for synchro
    //If synchro = 0 then all the following are ignored [default]
    milliseconds producerInitialDelay;
    milliseconds consumerInitialDelay;
    milliseconds producerSleep; //should be multiple of granularity
    milliseconds producerUptime;
    milliseconds consumerSleep;
    milliseconds consumerUptime;

    MemoryArguments(){
        memoryThreshold = false;
        max_memory = MemoryBenchmark::getTotalMemory(false) / 3 * 2;
        min_memory = 0;
        supSleep = 100ms;
        infSleep = 100ms;
        synchro = 0;
    }

};

protected:
    static constexpr uint64_t NSEC_SEC = 1'000'000'000ULL;
    static constexpr uint64_t WARMUP = 1'000'000UL;
    static constexpr uint64_t RINGSIZE = 4096;

struct Arguments;

struct Format {
    struct FormatKeys{
        string file             = "file";
        string threads          = "threads";
        string producers        = "producers";
        string consumers        = "consumers";
        string iterations       = "iterations";
        string runs             = "runs";
        string duration         = "duration_sec";
        string granularity      = "granularity_msec";
        string warmup           = "warmup";
        string additionalWork   = "additionalWork";
        string queueFilter      = "queues";
        string arguments        = "flags";
        string memoryArgs       = "memoryArgs";
        string balanced         = "balanced";
        
    };
    //create the format map
    unordered_map<string,string> formatMap;
    string file;
    vector<size_t> threads;
    vector<size_t> producers;
    vector<size_t> consumers;
    vector<size_t> iterations;
    vector<size_t> runs;
    vector<seconds> duration;
    vector<milliseconds> granularity;
    vector<size_t> warmup;
    vector<double> additionalWork;
    vector<string> queueFilter;
    Arguments args;
    MemoryArguments memoryArgs;
    bool balanced;

    Format(char **argv, int argc){
        for(int i = 0; i < argc; i++){
            //check if string has format "substr:val"
            if(argv[i][0] == __delim_1){
                string str(argv[i]);
                size_t pos = str.find(__delim_2);
                if(pos == string::npos) continue; //skip if no delimiter
                string key = str.substr(1,pos-1);
                string val = str.substr(pos+1);
                formatMap[key] = val;
            }
        }
        parseFormat();
        formatMap.clear();
    };

private:
    void parseFormat(){
        FormatKeys keys;
        //Mandatory Arguments
        file        = formatMap.find(keys.file) != formatMap.end() ? file = formatMap[keys.file] : "";
        threads     = formatMap.find(keys.threads) != formatMap.end() ? split<size_t>(formatMap[keys.threads]) : vector<size_t>{};
        producers   = formatMap.find(keys.producers) != formatMap.end() ? split<size_t>(formatMap[keys.producers]) : vector<size_t>{};
        consumers   = formatMap.find(keys.consumers) != formatMap.end() ? split<size_t>(formatMap[keys.consumers]) : vector<size_t>{};
        iterations  = formatMap.find(keys.iterations) != formatMap.end() ? split<size_t>(formatMap[keys.iterations]) : vector<size_t>{};
        runs        = formatMap.find(keys.runs) != formatMap.end() ? split<size_t>(formatMap[keys.runs]) : vector<size_t>{};
        duration    = formatMap.find(keys.duration) != formatMap.end() ? split<seconds>(formatMap[keys.duration]) : vector<seconds>{};
        granularity = formatMap.find(keys.granularity) != formatMap.end() ? split<milliseconds>(formatMap[keys.granularity]) : vector<milliseconds>{};
        queueFilter = formatMap.find(keys.queueFilter) != formatMap.end() ? split<string>(formatMap[keys.queueFilter]) : vector<string>{};
        //Optional Arguments
        warmup          = formatMap.find(keys.warmup) != formatMap.end() ? split<size_t>(formatMap[keys.warmup]) : vector<size_t>{};
        additionalWork  = formatMap.find(keys.additionalWork) != formatMap.end() ? split<double>(formatMap[keys.additionalWork]) : vector<double>{};
        balanced        = formatMap.find(keys.balanced) != formatMap.end() ? stoi(formatMap[keys.balanced]) : false;
        //Special parsing for Arguments and MemoryArguments;
        if(formatMap.find(keys.arguments) != formatMap.end()){
            vector<bool> fls = split<bool>((formatMap[keys.arguments]));
            args = parseFlags(fls);
        } else args = Arguments();

        memoryArgs = MemoryArguments();
        if(formatMap.find(keys.memoryArgs) != formatMap.end()){
            vector<int> memArgs = split<int>((formatMap[keys.memoryArgs]));
            bool setThresh = false;
            memoryArgs = parseMemArgs(memArgs);
        } else memoryArgs = MemoryArguments();
    }
    // Template function to split the string and cast elements to the given type
    template <typename T>
    static std::vector<T> split(const std::string& input, char delimiter = ',') {
        if (input.empty() || input == __NULL_ARG) return std::vector<T>{}; // Handle empty input string
        
        std::istringstream stream(input);
        std::string item;
        std::vector<T> result;

        while (std::getline(stream, item, delimiter)) {
            std::istringstream item_stream(item);
            
            if constexpr (std::is_same<T, std::chrono::milliseconds>::value) {
                // Handle milliseconds
                try {
                    long long milliseconds = std::stoll(item);  // Use std::stoll to handle large values
                    result.push_back(std::chrono::milliseconds(milliseconds));
                } catch (const std::invalid_argument&) {
                    throw std::invalid_argument("Failed to convert to milliseconds: " + item);
                }
            } 
            else if constexpr (std::is_same<T, std::chrono::seconds>::value) {
                // Handle seconds
                try {
                    long long seconds = std::stoll(item);  // Use std::stoll to handle large values
                    result.push_back(std::chrono::seconds(seconds));
                } catch (const std::invalid_argument&) {
                    throw std::invalid_argument("Failed to convert to seconds: " + item);
                }
            }
            else {
                // Handle other types (e.g., int, double)
                T value;
                item_stream >> value;
                if (item_stream.fail()) {
                    throw std::invalid_argument("Failed to cast item: " + item);
                }
                result.push_back(value);
            }
        }
        
        return result;
    }

    Arguments parseFlags(vector<bool> values){
        Arguments retval;
        if(values.size() == 3){
            retval._stdout      = values[0];
            retval._progress    = values[1];
            retval._overwrite   = values[2];
        }
        return retval;
    }

    MemoryArguments parseMemArgs(const std::vector<int>& values) {
        MemoryArguments retval;
        if (values.size() != 10) return retval;
        // First four values for memory-related parameters
        for (int i = 0; i < 4; ++i) {
            if (values[i] > 0) retval.memoryThreshold = true;
            else continue;
            switch (i) {
                case 0: retval.max_memory = values[i]; break;
                case 1: retval.min_memory = values[i]; break;
                case 2: retval.supSleep = std::chrono::milliseconds(values[i]); break;
                case 3: retval.infSleep = std::chrono::milliseconds(values[i]); break;
            }
        }
        // Second set of three values for producer flags and related settings
        if (values[4] > 0) {
            retval.synchro |= MemoryBenchmark::SYNC_PRODS;
            for (int i = 5; i < 8; ++i){
                switch (i) {
                    case 5: retval.producerInitialDelay = std::chrono::milliseconds(values[i]); break;
                    case 6: retval.producerSleep = std::chrono::milliseconds(values[i]); break;
                    case 7: retval.producerUptime = std::chrono::milliseconds(values[i]); break;
                }
            }
        }
        // Third set of values for consumer flags and related settings
        if (values[8] > 0) {
            retval.synchro |= MemoryBenchmark::SYNC_CONS;
            for (int i = 9; i < 12; ++i) {
                    switch (i) {
                        case 9: retval.consumerInitialDelay = std::chrono::milliseconds(values[i]); break;
                        case 10: retval.consumerSleep = std::chrono::milliseconds(values[i]); break;
                        case 11: retval.consumerUptime = std::chrono::milliseconds(values[i]); break;
                    }
                
            }
        }
        return retval;
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