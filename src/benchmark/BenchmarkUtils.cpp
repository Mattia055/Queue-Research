#include <cstdint> 
#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <sys/stat.h>

#include "BenchmarkUtils.hpp"
#include "Stats.hpp"

using namespace std;

namespace bench{

uint32_t __GCD(size_t a, size_t b){
    return (b==0)? a : __GCD(b,a % b);
}


uint32_t GCD(size_t a, size_t b){
    return (a > b)? __GCD(a,b) : __GCD(b,a);
}

void ThroughputCSVHeader(std::ostream& stream){
    stream << "Benchmark,\"Param: queueType\",Threads,\"Param: additionalWork\",\"Param: ringSize\","
           << "Duration\",\"Iterations\",\"Score,\"Score Error\"" << endl;
} 

void ThroughputCSVData( std::ostream& stream,
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
string formatDigits(uint64_t n){
    std::string numStr = std::to_string(n);  // Convert integer to string
    int length = numStr.length();
    
    // Start from the second-to-last digit and move backward
    for (int i = length - 3; i > 0; i -= 3) {
        numStr.insert(i, ".");  // Insert a dot every 3 digits
    }
    return numStr;
}


string formatTime(int totalSeconds) {
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

    void clearTerminal(){
#ifdef _WIN_32
    system("cls");
#else
    system("clear");
#endif
}

bool notFile(const string filename) {
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

size_t getTotalMemory(bool includeSwap) {
#ifndef __linux__
    cerr << "Can't get total memory : only linux systems" << endl;
#ifdef SYS_CAUTIOUS
    exit(0);
#else
    return 0;
#endif
#endif

    std::ifstream memInfo("/proc/meminfo");
    if (!memInfo.is_open()) {
        std::cerr << "Failed to open /proc/meminfo" << std::endl;
#ifdef SYS_CAUTIOUS
        exit(-1);
#endif
        return 0;
    }

    size_t totalMemory = 0;
    size_t totalSwap = 0;
    std::string line;
    
    while (getline(memInfo, line)) {
        std::istringstream iss(line);
        std::string key;
        size_t value;

        // Read the first word (key) and the second number (value)
        if (iss >> key >> value) {
            if (key == "MemTotal:") {
                totalMemory = value;  // In KB
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




}