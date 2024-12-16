#pragma once
#include <iostream>
#include <array>
#include <atomic>
#include <thread>
#include <barrier>
#include <vector>
#include <chrono>
#include <cstdlib>  // For system()
#include <fstream>  // for file system operations
#include <string>
#include <iomanip>
#include <algorithm>
#include <sys/stat.h>
#include <type_traits>
#include <typeinfo>
#include <random>
#include <filesystem>   //for filesystem::remove
#include <tuple>        //for return
#include <unistd.h> 
#include <cstdlib>

#include "MuxQueue.hpp"
#include "ThreadGroup.hpp"
#include "Stats.hpp"
#include "AdditionalWork.hpp"

#define NSEC_IN_SEC     1'000'000'000LL
#define PRECISION       15


using namespace std;
using namespace chrono;

/**==== DEBUG ====
 * Added output to CSV (both Symmetric and ProdCons)
 * Added MuxQueue adapter to be available for testing
 */

namespace bench {

size_t get_total_memory();
int pid = getpid();

int PAGE_SIZE=0; //getpagesize();
size_t MEMORY_SIZE = 0;//get_total_memory(); 
constexpr size_t WARMUP_CONST = 1'000'000LL;
std::string RSS_FILE_PATH=""; //= "/proc/self/status";


std::string randomNumberString(size_t length) {
    // Random number generator
    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_int_distribution<> dis(0, 9); // Uniform distribution for digits 0-9

    std::string randomStr;
    for (size_t i = 0; i < length; ++i) {
        randomStr += std::to_string(dis(gen));  // Append a random digit to the string
    }
    return randomStr;
}

size_t get_total_memory() {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    size_t total_memory = 0;

    if (meminfo.is_open()) {
        while (getline(meminfo, line)) {
            if (line.substr(0, 9) == "MemTotal:") {
                total_memory = std::stoul(line.substr(9));  // Extract memory size in kB
                break;
            }
        }
        meminfo.close();
    }
    return total_memory;
}

template<typename Duration>
std::string TimeUnit(Duration d, bool minimize=true){
    using Period = typename Duration::period;
    std::string result;
    minimize = !minimize;   //
    // Print the unit of the duration as a string based on Period
    if (std::is_same<Period,seconds::period>::value) 
        result = minimize? "seconds":"s";
    else if (std::is_same<Period, milliseconds::period>::value)
        result = minimize? "milliseconds":"ms";
    else if (std::is_same<Period, microseconds::period>::value)
        result = minimize? "microseconds":"micros";
    else if (std::is_same<Period, nanoseconds::period>::value)
        result = minimize? "nanoseconds":"nanos";
    else if (std::is_same<Period, minutes::period>::value) 
        result = minimize? "minutes":"mm";
    else if (std::is_same<Period, hours::period>::value) 
        result = minimize?"hours":"hh";
    else
        result = "N/A";
    return result;

}





// Assuming Q<UserData>::className() returns a string and sts.mean and sts.stddev are numeric types
std::string formatDigits(uint64_t n){
    std::string numStr = std::to_string(n);  // Convert integer to string
    int length = numStr.length();
    
    // Start from the second-to-last digit and move backward
    for (int i = length - 3; i > 0; i -= 3) {
        numStr.insert(i, ".");  // Insert a dot every 3 digits
    }
    return numStr;
}


std::string formatTime(int totalSeconds) {
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

template<typename V>
void printBenchmarkResults(std::string title, std::string resultLabel, const V& mean, const V& stddev) {
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



class MemoryBenchmark {
private: 
    bench::Arguments flags;

    static constexpr int BATCH_SIZE =  10000;    //10mila bytes di buffer

    static inline size_t getRss(bool pages=false){
#ifndef __linux__
        cerr << "Only implemented in LinuxBased systems" << endl;
        exit(0);
#endif
#if true
        std::ifstream file(bench::RSS_FILE_PATH);
        std::string line;
        size_t rss = 0;

        // Read the file line by line
        while (std::getline(file, line)) {
            // Check for the VmRSS line which contains the RSS value
            if (line.find("VmRSS:") == 0) {
                // Extract the number (RSS in kB) from the line
                std::istringstream(line.substr(7)) >> rss;
                break;
            }
        }
        
        file.close();

        //page_count = rss_in_bytes / PAGE_SIZE
        return pages? rss * (1024) / PAGE_SIZE : rss;  // Return the RSS in kilobytes
#else
    std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
    if (!status_file) {
        std::cerr << "Error opening /proc/" << pid << "/status" << std::endl;
        return -1;
    }

    std::string line;
    long vsz = -1;

    // Look for the line that starts with "VmSize" to get the virtual memory size
    while (std::getline(status_file, line)) {
        if (line.rfind("VmSize:", 0) == 0) {
            // Extract the VSZ value
            std::sscanf(line.c_str(), "VmSize: %ld kB", &vsz);
            break;
        }
    }

    return vsz;  // Return VSZ in kilobytes

#endif
    }

public:

    struct MemoryArguments {
        //max mamory allowed is half of the ram
        size_t max_mem = MEMORY_SIZE / 2; //main can block producers
        size_t min_mem = 0; //main can block consumer
        milliseconds producerStop = milliseconds{100};  //sleep for producers
        milliseconds consumerStop = milliseconds{100};  //sleep for consumers
        size_t consumerWaitIndex = 0;
        milliseconds consumerSleep = milliseconds{0};
        size_t producerWaitIndex = 0;
        milliseconds producerSleep = milliseconds{0};

        MemoryArguments(size_t max=(get_total_memory()/2),
                        size_t min=0,
                        milliseconds prodStop=milliseconds{100}, 
                        milliseconds consStop=milliseconds{100},
                        size_t producerWaitIdx = 0,
                        size_t consumerWaitIdx = 0,
                        milliseconds producerSlp = milliseconds{0},
                        milliseconds consumerSlp = milliseconds{0}
                        )
        {
            assert(max_mem >= min_mem);
            max_mem = max;
            min_mem = min;
            producerStop = prodStop;
            consumerStop = consStop;
            producerWaitIndex = producerWaitIdx;
            producerSleep = producerSlp;
            consumerSleep = consumerSlp;
        };
        ~MemoryArguments(){}
    };

    struct Results{
        size_t mem_init =0;
        size_t mem_start=0;
        size_t mem_max=0;
        size_t mem_min=0;
        size_t mem_end=0;
        size_t mem_dealloc = 0;
        double mem_avg=0;
        double mem_stddev=0;

        //double constructor
        Results(){};

        void set(size_t m_init,size_t m_start,size_t m_max, size_t m_min,size_t m_end,long double m_avg,long double m_stddev)
        {   mem_init = m_init;
            mem_start = m_start;
            mem_max = m_max;
            mem_min = m_min;
            mem_end = m_end;
            mem_avg = m_avg;
        }

        Results(size_t m_init,size_t m_start,size_t m_max, size_t m_min,size_t m_end,long double m_avg,long double m_stddev)
        {   mem_init = m_init;
            mem_start = m_start;
            mem_max = m_max;
            mem_min = m_min;
            mem_end = m_end;
            mem_avg = m_avg;
            mem_stddev = m_stddev;
        }
        ~Results(){};

        std::string toString(std::string linestart = "", bool pages = false) {
            std::stringstream retval;
            std::string unit = pages ? " pages" : " KB";
            std::string eol = "\n" + linestart;

            if (pages) {
                retval << linestart << "Size of Pages:\t" << PAGE_SIZE << " " << unit << eol;
            }

            // Formatting with std::setw to align the values properly
            retval << linestart << "Base Memory: "  << mem_init    << unit << eol
                << linestart << "Starting Memory: "  << mem_start    << unit << eol
                << linestart << "Ending Memory: "       << mem_end      << unit << eol
                << linestart << "Max Memory: "          << mem_max      << unit << eol
                << linestart << "Min Memory: "          << mem_min      << unit << eol
                << linestart << "Memory Dealloc: "       << mem_dealloc   << unit << eol
                << linestart << "Average Memory: "      << mem_avg      << unit << eol
                << linestart << "Stddev Memory: "       << mem_stddev   << unit << eol;

            return retval.str();
        }
    };


    size_t numProducers, numConsumers;
    double producerAdditionalWork{};
    double consumerAdditionalWork{};
    size_t ringsToFill;
    MemoryArguments memArgs;

    MemoryBenchmark(size_t nProd, size_t nCons, double pWork, double cWork,size_t ringsFiller=0,
                    MemoryArguments memArgsp = MemoryArguments(),
                    bench::Arguments flags=bench::Arguments()):
    numProducers{nProd},
    numConsumers{nCons},
    producerAdditionalWork{pWork},
    consumerAdditionalWork{cWork},
    ringsToFill{ringsFiller},
    flags{flags},
    memArgs{memArgsp}
    {
        /**
         *  int PAGE_SIZE=0; //getpagesize();
            size_t MEMORY_SIZE = 0;//get_total_memory(); 
            constexpr size_t WARMUP_CONST = 1'000'000LL;
            std::string RSS_FILE_PATH=""; //= "/proc/self/status";
         */
        //check if getRSS works
        if(PAGE_SIZE == 0){
            MEMORY_SIZE = get_total_memory();
            RSS_FILE_PATH = "/proc/self/status";
        }
        std::ifstream file(bench::RSS_FILE_PATH);
        if (!file.is_open()) {
            std::cerr   << "Unable to open "<<bench::RSS_FILE_PATH<<"\n"
                        << "You can change the file updating the bench::RSS_FILE_PATH global variable"
                        << std::endl;
            exit(0);
        }
    };

    ~MemoryBenchmark(){};

private:
    template<template<typename> typename Q, typename Duration> // works on any queue
    Results __MemoryRun(std::string filename, size_t queueSize, Duration runDuration, Duration rssSnapshot, bool pages) {
        const size_t MAX_BATCH_SIZE = 1024 * 10; // Max size of the batch buffer (adjust size as needed)

        // Create a std::string with pre-allocated capacity to avoid dynamic allocations
        std::string buffer;
        buffer.reserve(MAX_BATCH_SIZE); // Reserve space upfront to avoid reallocations

        size_t currentBufferSize = 0; // Variable to track the current size of the buffer
        ofstream file = ofstream(filename);

        barrier<> barrier(numProducers + numConsumers + 1);
        std::atomic<bool> stopFlag{false};
        std::atomic<bool> prodWaitFlag{false};
        std::atomic<bool> consWaitFlag{false};
        Q<UserData>* queue = nullptr;
        ThreadGroup threads{};

        //thread monitoring
        bool prodWaitSetted = false;
        bool consWaitSetted = false;
        size_t minMem = memArgs.min_mem;
        size_t maxMem = memArgs.max_mem;

        Results res;

        auto const prod_lambda = [this,queueSize,&barrier,&prodWaitFlag,&stopFlag, &queue](const int tid) {
            barrier.arrive_and_wait();
            UserData ud(0, 0);
            size_t iSleep = 0;
            barrier.arrive_and_wait();
            long long elements = (queueSize * ringsToFill)/(this->numProducers);
            cout << elements * this->numProducers << endl;
            while ((--elements) >= 0) {
                queue->enqueue(&ud, tid);
            }
            barrier.arrive_and_wait();
            //wait for main
            barrier.arrive_and_wait();
            // Start measurement
            return;
            while (true) {
                while(prodWaitFlag){
                    this_thread::sleep_for((this->memArgs).producerStop);
                }
                if(stopFlag.load())break;
                queue->enqueue(&ud, tid);
                random_additional_work(this->producerAdditionalWork);
                iSleep++;
                if(iSleep == ((this->memArgs).producerWaitIndex/(this->numProducers))){
                    this_thread::sleep_for((this->memArgs).producerSleep);
                    iSleep = 0;
                }
            }
        };

        auto const cons_lambda = [this,&barrier,&consWaitFlag,&stopFlag, &queue](const int tid) {
            barrier.arrive_and_wait();
            size_t iSleep = 0;
            UserData* placeholder = nullptr;
            barrier.arrive_and_wait();
            // Wait for queueFill
            barrier.arrive_and_wait();
            //wait for main
            barrier.arrive_and_wait();
            while (true) {
                while(consWaitFlag.load()){
                    this_thread::sleep_for((this->memArgs).consumerStop);
                }
                if(stopFlag.load())break;
                placeholder = queue->dequeue(tid);
                random_additional_work(this->consumerAdditionalWork);
                iSleep++;
                if(iSleep == ((this->memArgs).producerWaitIndex/(this->numProducers))){
                    this_thread::sleep_for((this->memArgs).producerSleep);
                    iSleep = 0;
                }
            }
        };

        for (int i = 0; i < numProducers; i++) {
            threads.thread(prod_lambda);
        }
        for (int i = 0; i < numConsumers; i++) {
            threads.thread(cons_lambda);
        }

        barrier.arrive_and_wait();  //starting barrier
        // Initialize memory tracking
        size_t rss_base = getRss(pages);
        queue = new Q<UserData>(numProducers + numConsumers, queueSize);
        size_t rss_baseQueue = getRss(pages) - rss_base;
        cout << "Base Queue" << rss_baseQueue << endl;
        barrier.arrive_and_wait();  //insertion barrier
        barrier.arrive_and_wait();  //second barrier [main does stuff]

        
        size_t rss_queue = getRss(pages) - rss_base;
        res.set(rss_baseQueue,rss_queue,rss_queue,numeric_limits<size_t>::max(),0,rss_queue,0);
        size_t i = 1; //iteration count used to calculate the average memory consumption
        //Write first result
        std::string newData = std::to_string(rss_queue) + "\n";
        size_t newDataSize = newData.size();
        buffer.append(newData);
        currentBufferSize += newDataSize; // Update the current buffer size
        //[DEBUG]
        cout << res.toString() << endl;
        barrier.arrive_and_wait(); // Start memory monitoring
        auto current_time = steady_clock::now();
        auto end_time = current_time + runDuration - rssSnapshot; //account for initial snapshot

        // Main loop for memory monitoring and file writing
        for(; current_time <= end_time - rssSnapshot; current_time += rssSnapshot) {
            this_thread::sleep_for(rssSnapshot);//just dumped first result so sleep
            i++;    //increment the iteration count
            rss_queue = getRss(pages) - rss_base;
            //malloc_trim(0);

            //memory boundary_inf
            if(rss_queue >= maxMem){
                prodWaitFlag.store(true,memory_order::acquire);
                prodWaitSetted = true;
                
            }
            //memory boundary_inf
            if(rss_queue <= minMem){
                consWaitFlag.store(true,memory_order::acquire);
                consWaitSetted = true;
            }

            if(prodWaitSetted && rss_queue < maxMem){
                prodWaitFlag.store(false);
                prodWaitSetted = false;
            }
            if(consWaitSetted && rss_queue >= minMem){
                consWaitFlag.store(false);
                consWaitSetted = false;
            }

            

            if(rss_queue > res.mem_max) res.mem_max = rss_queue;
            if(rss_queue < res.mem_min) res.mem_min = rss_queue;
            res.mem_avg += rss_queue;   //calculate the sum (for the average);
            
            // New data to append
            newData = std::to_string(rss_queue) + "\n";
            newDataSize = newData.size();

            // Check if adding the new data would overflow the buffer
            if (currentBufferSize + newDataSize > MAX_BATCH_SIZE) {
                // If the buffer will overflow, write the current buffer to the file
                file.write(buffer.data(), currentBufferSize);
                buffer.clear();         // Clear the buffer after writing
                currentBufferSize = 0;  // Reset the buffer size counter
            }

            // Append the new data to the buffer
            buffer.append(newData);
            currentBufferSize += newDataSize;   // Update the current buffer size

            // If buffer is full, write it to the file
            if (currentBufferSize >= MAX_BATCH_SIZE) {
                file.write(buffer.data(), currentBufferSize);
                buffer.clear();         // Reset the buffer after writing
                currentBufferSize = 0;  // Reset the buffer size counter
            }

            cout << "WHILE LOOP " << getRss() << endl;

        }
        prodWaitFlag.store(false);
        consWaitFlag.store(false);
        stopFlag.store(true);
        //record the last result
        while(queue->dequeue(0) != nullptr){
            cout << "Extracting" << endl;
        }
        res.mem_end = rss_queue;
        // Calculate the ratio

        res.mem_avg /= i;

        // Write any remaining data in the buffer to the file
        if (currentBufferSize > 0) {
            file.write(buffer.data(), currentBufferSize);
        }
        cout << "before file close" << getRss() << endl;
        file.close();
        cout << "before thread join" << getRss() << endl;
        threads.join();
        cout << "before queue dealloc" << getRss() << endl;
        delete queue;
        res.mem_dealloc = getRss(pages);
        cout << "before return " << getRss() << endl;
        return res;
    }

public:
    template<template<typename> typename Q, typename Duration>
    Results MemoryRun(std::string filename, size_t queueSize, Duration runDuration,Duration rssSnapshot, bool pages=false){
        //check if file exists and flags: if flags overwrite it
        std::string tempFileName;
        do{
            tempFileName = filename + randomNumberString(5) + ".tmp";
        }while(!notFile(tempFileName));
        
        Results res = __MemoryRun<Q>(tempFileName, queueSize, runDuration, rssSnapshot, pages); //compiles the tempfile
        //create the csvFile; 
        ofstream file(filename,ios::out | ios::trunc);

        // compute standard deviation
        ifstream tempFile(tempFileName);
        std::string line;
        double variance_iter{};
        int iter = 0;
        while(std::getline(tempFile,line)){
            try{
                double variter = stol(line) - res.mem_avg;
                variance_iter += variter * variter;
            } catch(...){
                cerr << "An error occurred while processing the line: "<< iter << "of file " << tempFileName << endl;
                exit(-1);
            } 
            ++iter; 
        }
        //put the standard deviation in the result struct
        res.mem_stddev = iter == 0? 0 : std::sqrt(variance_iter / (iter -1));

        // append comments at file start
        file    << "%\n% Memory Consumption of " << Q<UserData>::className()
                << "\n% Duration: " << runDuration.count() << " " << TimeUnit(runDuration)
                << "\n% Snapshot Intervals: "<< rssSnapshot.count() << " " << TimeUnit(rssSnapshot)
                << "\n% Summary:"
                << "\n" << res.toString("% ",pages)
                << endl;;

        //append header
        file    << "Memory,\"Param: Snapshot\"" << endl;

        //append from tempfile
        tempFile = ifstream(tempFileName);
        int rssSnaphotVal = rssSnapshot.count();
        int index = 0;

        while(std::getline(tempFile,line)){
            try{
                file << stol(line) <<","<<rssSnaphotVal*(index++)<<"\n";
            } catch(...){
                cerr << "An error occurred while processing the line: "<< index-1 << "of file " << tempFileName << endl;
                exit(-1);
            } 
        }
        file.close();

        try{
            if(false && !std::filesystem::remove(tempFileName))
                cerr << "Error deleting file: " << tempFileName << " " << endl;
        }catch( const filesystem::filesystem_error& e){
            cerr << "Error deleting file: " << e.what() << endl;
        }
        return res;
    }
};

}

