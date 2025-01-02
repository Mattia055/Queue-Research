#pragma once

#include <string>
#include <vector>
#include <string>
#include <unordered_map>
#include "Benchmark.hpp"

using namespace std;
using namespace chrono;
using namespace bench;

class Format {
public:
    string name;
    string path;
    vector<size_t> threads;
    vector<size_t> producers;
    vector<size_t> consumers;
    vector<size_t> sizes;
    vector<int> ratio;
    size_t iterations;
    size_t runs;
    seconds duration;
    milliseconds granularity;
    vector<size_t> warmup;
    vector<double> additionalWork;
    vector<string> queueFilter;
    Benchmark::Arguments args;
    Benchmark::MemoryArguments memoryArgs;
    bool balanced;

    //Dont use the first 2 arguments
    Format(char **argv, int argc){
        unordered_map<string,string> formatMap;
        for(int i = 0; i < argc; i++){
            //check if string has format "substr:val"
            string str(argv[i]);
            size_t pos = str.find(__delim_1);
            if(pos == string::npos) continue; //skip if no delimiter
            string key = str.substr(0,pos);
            string val = str.substr(pos+1);
            formatMap[key] = val;   
        }
        parseFormat(formatMap);
        formatMap.clear();
    };

    struct FormatKeys{
        string name             = "benchmark";
        string path             = "path";
        string ratio            = "ratio";   
        string sizes            = "sizes";
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

private:
    void parseFormat(unordered_map<string,string> formatMap){
        FormatKeys keys;
        //Mandatory Arguments
        name        = formatMap.find(keys.name) != formatMap.end() ? formatMap[keys.name] : "";
        path        = formatMap.find(keys.path) != formatMap.end() ? path = formatMap[keys.path] : "";
        threads     = formatMap.find(keys.threads) != formatMap.end() ? split<size_t>(formatMap[keys.threads]) : vector<size_t>{};
        producers   = formatMap.find(keys.producers) != formatMap.end() ? split<size_t>(formatMap[keys.producers]) : vector<size_t>{};
        consumers   = formatMap.find(keys.consumers) != formatMap.end() ? split<size_t>(formatMap[keys.consumers]) : vector<size_t>{};
        iterations  = formatMap.find(keys.iterations) != formatMap.end() ? static_cast<size_t>(std::stoull(formatMap[keys.iterations])) : 0;
        runs        = formatMap.find(keys.runs) != formatMap.end() ? static_cast<size_t>(std::stoull(formatMap[keys.runs])) : 0;
        duration    = formatMap.find(keys.duration) != formatMap.end()? seconds(std::stoull(formatMap[keys.duration])) : seconds{0};
        granularity = formatMap.find(keys.granularity) != formatMap.end() ? milliseconds(std::stoull(formatMap[keys.granularity])) : milliseconds{0};
        queueFilter = formatMap.find(keys.queueFilter) != formatMap.end() ? split<string>(formatMap[keys.queueFilter]) : vector<string>{};
        ratio       = formatMap.find(keys.ratio) != formatMap.end() ? split<int>(formatMap[keys.ratio]) : vector<int>{};
        //Optional Arguments
        warmup          = formatMap.find(keys.warmup) != formatMap.end() ? split<size_t>(formatMap[keys.warmup]) : vector<size_t>{};
        additionalWork  = formatMap.find(keys.additionalWork) != formatMap.end() ? split<double>(formatMap[keys.additionalWork]) : vector<double>{};
        balanced        = formatMap.find(keys.balanced) != formatMap.end() ? stoi(formatMap[keys.balanced]) : false;
        sizes           = formatMap.find(keys.sizes) != formatMap.end() ? split<size_t>(formatMap[keys.sizes]) : vector<size_t>{};
        //Special parsing for Arguments and MemoryArguments;
        if(formatMap.find(keys.arguments) != formatMap.end()){
            vector<bool> fls = split<bool>((formatMap[keys.arguments]));
            args = parseFlags(fls);
        } else args = Benchmark::Arguments();

        memoryArgs = Benchmark::MemoryArguments();
        if(formatMap.find(keys.memoryArgs) != formatMap.end()){
            vector<int> memArgs = split<int>((formatMap[keys.memoryArgs]));
            bool setThresh = false;
            memoryArgs = parseMemArgs(memArgs);
        } else memoryArgs = Benchmark::MemoryArguments();
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

    Benchmark::Arguments parseFlags(vector<bool> values){
        Benchmark::Arguments retval;
        if(values.size() == 3){
            retval._stdout      = values[0];
            retval._progress    = values[1];
            retval._overwrite   = values[2];
        }
        return retval;
    }

    Benchmark::MemoryArguments parseMemArgs(const std::vector<int>& values) {
        Benchmark::MemoryArguments retval;
        if (values.size() != 10) return retval;
        // First four values for memory-related parameters
        if(values[0] > 0 && values[2] > 0)
            retval.setMaxMonitor(values[0], std::chrono::milliseconds(values[2]));
        if(values[1] > 0 && values[3] > 0)
            retval.setMinMonitor(values[1], std::chrono::milliseconds(values[3]));

        // Second set of three values for producer flags and related settings
        bool setter = true;
        for(int i = 4 ; i < 8; ++i) if(values[i] < 0) setter = false;
        retval.setProducerSync(milliseconds(values[5]),milliseconds(values[7]),milliseconds(values[6]));
        setter = true;
        for(int i = 8; i< 11; ++i) if(values[i] < 0) setter = false;
        retval.setConsumerSync(milliseconds(values[9]),milliseconds(values[11]),milliseconds(values[10]));
        return retval;
    } 

};

