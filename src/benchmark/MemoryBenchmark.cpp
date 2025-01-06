#include <chrono>
#include <string>
#include <signal.h>
#include <array>
#include <fstream>
#include <thread>
#include <iostream>
#include <cstdlib>

#include "MemoryBenchmark.hpp"

namespace bench{

/**
 * RESULTS STRUCT
 */

// Implementation of MemoryBenchmark::Results::Stats constructor
MemoryBenchmark::Results::Stats::Stats(size_t max, size_t min, size_t start, size_t end,
                                        double mean, double stddev)
    : max(max), min(min), start(start), end(end), mean(mean), stddev(stddev) {}

// Implementation of MemoryBenchmark::Results constructor
MemoryBenchmark::Results::Results(size_t rssMax, size_t rssMin, size_t rssStart, size_t rssEnd,
                                   double rssMean, double rssStddev,
                                   size_t vmMax, size_t vmMin, size_t vmStart, size_t vmEnd,
                                   double vmMean, double vmStddev)
    : RSS(rssMax, rssMin, rssStart, rssEnd, rssMean, rssStddev),
      VM(vmMax, vmMin, vmStart, vmEnd, vmMean, vmStddev) {}

// Implementation of MemoryBenchmark::Results::setRSS method
void MemoryBenchmark::Results::setRSS(size_t max, size_t min, size_t start, size_t end,
                                       double mean, double stddev) {
    RSS = Stats(max, min, start, end, mean, stddev);
}

// Implementation of MemoryBenchmark::Results::setVM method
void MemoryBenchmark::Results::setVM(size_t max, size_t min, size_t start, size_t end,
                                      double mean, double stddev) {
    VM = Stats(max, min, start, end, mean, stddev);
}

// Implementation of MemoryBenchmark::Results::setResults method
void MemoryBenchmark::Results::setResults(size_t rssMax, size_t rssMin, size_t rssStart, size_t rssEnd,
                                          double rssMean, double rssStddev,
                                          size_t vmMax, size_t vmMin, size_t vmStart, size_t vmEnd,
                                          double vmMean, double vmStddev) {
    setRSS(rssMax, rssMin, rssStart, rssEnd, rssMean, rssStddev);
    setVM(vmMax, vmMin, vmStart, vmEnd, vmMean, vmStddev);
}

/**
 * MEMORY CONTROL STRUCT
 */
MemoryBenchmark::MemoryControl::ThreadControl::ThreadControl(std::chrono::milliseconds init,
                                                            std::chrono::milliseconds sleep,
                                                            std::chrono::milliseconds uptime)
    : initUptime(init), sleep(sleep), uptime(uptime) {}

MemoryBenchmark::MemoryControl::MemoryControl(size_t max, std::chrono::milliseconds maxSleep,
                                              size_t min, std::chrono::milliseconds minSleep)
    : max_memory(max), min_memory(min), maxReachSleep(maxSleep), minReachSleep(minSleep), level(NONE) {}

void MemoryBenchmark::MemoryControl::producers(std::chrono::milliseconds init,
                                               std::chrono::milliseconds sleep,
                                               std::chrono::milliseconds uptime)
    {
        producer = ThreadControl(init, sleep, uptime);
        level |= PRODS;
    }

void MemoryBenchmark::MemoryControl::consumers(std::chrono::milliseconds init,
                                               std::chrono::milliseconds sleep,
                                               std::chrono::milliseconds uptime)
    {
        consumer = ThreadControl(init, sleep, uptime);
        level |= CONS;
    }

/**
 * MEMORY BENCHMARK CLASS
 */

// Constructor for MemoryBenchmark
MemoryBenchmark::MemoryBenchmark(size_t prodCount, size_t consCount, double additionalWork,
                                 bool balancedLoad, size_t ringSize, Arguments args, MemoryControl memoryFlags)
    : producers(prodCount), consumers(consCount), ringSize(ringSize), balancedLoad(balancedLoad), 
    flags{args}, memoryFlags{memoryFlags}
    {
        if (producers == 0 || consumers == 0)
            throw std::invalid_argument("Threads count must be greater than 0");
        else if (ringSize == 0)
            throw std::invalid_argument("Ring Size must be greater than 0");
        else if (additionalWork < 0)
            throw std::invalid_argument("Additional Work must be greater than 0");

        if (balancedLoad) {
            const size_t total = producers + consumers;
            const double ref = additionalWork * 2 / total;
            producerAdditionalWork = producers * ref;
            consumerAdditionalWork = consumers * ref;
        } else {
            producerAdditionalWork = additionalWork;
            consumerAdditionalWork = additionalWork;
        }
    }

void MemoryBenchmark::printHeader(std::string filePath){
    std::ofstream file(filePath);
    file << "RSS,VM,Time" << std::endl;
    file.close();
}

int MemoryBenchmark::memoryMonitor(pid_t proc, seconds runDuration, milliseconds granularity, std::string fileName, size_t maxMemory, size_t minMemory){
    using namespace std;
    using namespace chrono;

    // Open file
    ofstream file(fileName,ios::out);    //automatically truncates the file
    if(!file.is_open()){
        cerr << "MEMORY MONITOR: Failed to open file: " << fileName << "\n";
        kill(proc, SIGALRM);    //SIGALARM to parent to terminate
        return 1;
    }

    //Dont have to set the signal mask since child inherits from main
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);  //we just use this mask to wait for SIGUSR1

    // Append header if file is empty
    if (fileExists(fileName)) file << "RSS,VM,Time\n";

    array<MemoryInfo, BATCH_SIZE> memorySnapshots;
    MemoryInfo current;

    // Monitor the memory before warmup
    memorySnapshots[0] = getProcessMemoryInfo(proc);

    //Initialize useful variables
    size_t granularity_count = static_cast<size_t>(granularity.count());
    size_t iterations = static_cast<size_t>(duration_cast<milliseconds>(runDuration).count()) / granularity_count;

    int sig;
    do {
        sigwait(&mask, &sig);
    } while (sig != SIGUSR1);   // Wait for signal to start measure

    int curr_idx = 0;   //index for the element
    
    for(int i = 1; i<= iterations; i++){
        if(i % BATCH_SIZE == 0){  //Write batched data to file
            for(MemoryInfo mem : memorySnapshots)
                file << mem.rssSize << "," << mem.vmSize << "," << (curr_idx++) * granularity_count << "\n";
        }

        current = getProcessMemoryInfo(proc);
        if(current.rssSize > maxMemory)
            kill(proc, SIGUSR1);
        else if(current.rssSize < minMemory)
            kill(proc, SIGUSR2);

        memorySnapshots[i % BATCH_SIZE] = current;
        this_thread::sleep_for(granularity);
    }

    //write the remaining
    int remaining = (iterations + 1) % BATCH_SIZE;
    for(int i = 0; i < remaining; i++){
        file << memorySnapshots[i].rssSize << "," << memorySnapshots[i].vmSize << "," << (curr_idx++) * granularity_count << "\n";
    }

    file.close(); //close file flushing the buffer just at the end;
    kill(proc, SIGALRM);    //SIGALARM to parent to terminate
    return 0;
}

MemoryBenchmark::Results MemoryBenchmark::computeMetrics(std::string fileName){
    using namespace std;

    ifstream file(fileName);
    if(!file.is_open()){
        cerr << "Failed to open file: " << fileName << "\n";
        return Results();
    }
    string line;
    double RSS_mean, VM_mean;
    double RSS_stddev, VM_stddev;
    size_t RSS_max, VM_max;
    size_t RSS_min, VM_min;
    size_t RSS_start, VM_start;
    size_t RSS_end, VM_end;

    //Using 2 step formula to compute stddev without mean [1/n(sum(x^2)-sum(x)^2)]
    long RSS_curr, VM_curr;
    size_t count = 0;   //also used to check if file is empty
    getline(file, line);    //skip header
    getline(file,line); //get first line and initialize values
    if(sscanf(line.c_str(), "%ld,%ld", &RSS_curr, &VM_curr) == 2){
        RSS_mean = RSS_max = RSS_min = RSS_start = RSS_stddev = RSS_curr;
        VM_mean = VM_max = VM_min = VM_start = RSS_stddev = VM_curr;
        RSS_stddev *= RSS_stddev;   VM_stddev *= VM_stddev;
        count++;
    }

    //keep parsing the file
    while(getline(file,line)){//keep parsing the file
        if(sscanf(line.c_str(), "%ld,%ld", &RSS_curr, &VM_curr) != 2) continue;
        RSS_mean += RSS_curr;   VM_mean += VM_curr;
        if(RSS_curr > RSS_max) RSS_max = RSS_curr;
        if(RSS_curr < RSS_min) RSS_min = RSS_curr;
        if(VM_curr > VM_max) VM_max = VM_curr;
        if(VM_curr < VM_min) VM_min = VM_curr;
        RSS_stddev += RSS_curr * RSS_curr;
        VM_stddev += VM_curr * VM_curr;
        count++;
    }
    file.close();

    //finish computing mean and stddev
    if(count != 0){
        RSS_end = RSS_curr; //compute last values if they exist
        VM_end = VM_curr;
        if(count == 1){
            RSS_stddev = VM_stddev = 0;
        }
        else{
            RSS_stddev  = sqrt((1/(count-1)) * (RSS_stddev - (RSS_mean * RSS_mean)));
            VM_stddev   = sqrt((1/(count-1)) * (VM_stddev - (VM_mean * VM_mean)));
            RSS_mean   /= count;
            VM_mean    /= count;
        }
        return Results(RSS_max, RSS_min, RSS_start, RSS_end, RSS_mean, RSS_stddev,
                    VM_max, VM_min, VM_start, VM_end, VM_mean, VM_stddev);
    }
    
    return Results();
}



} //namespace bench

// const size_t BATCH_SIZE = 100;

// int main(int argc, char **argv) {
//     using MemBench = bench::MemoryBenchmark;
//     using namespace std;
//     using namespace chrono;

//     if (argc != 8) {
//         cerr << "Usage: " << argv[0] << " <pid> <runDuration> <granularity> <file_name> <max_memory> <min_memory> <monitor>\n";
//         return 1;
//     }

//     // Cast parameters
//     pid_t pid = static_cast<pid_t>(stoi(argv[1]));
//     seconds runDuration = seconds(stoi(argv[2]));
//     milliseconds granularity = milliseconds(stoi(argv[3]));
//     string file_name = argv[4];
//     size_t max_memory = (stol(argv[5]));
//     size_t min_memory = (stol(argv[6]));
//     bool monitor = (stoi(argv[7]));

//     // Open file
//     ofstream file(file_name, ios::app);
//     if (!file.is_open()) {
//         cerr << "Failed to open file: " << file_name << "\n";
//         return 1;
//     }

//     // Append header if file is empty
//     if (MemBench::fileExists(file_name)) {
//         file << "vmSize,rssSize,time\n";
//     }

//     array<MemBench::MemoryInfo, BATCH_SIZE> memorySnapshots;
//     MemBench::MemoryInfo current;

//     // Monitor the memory before warmup
//     memorySnapshots[0] = MemBench::getProcessMemoryInfo(pid);

//     // Set signal mask
//     sigset_t mask;
//     sigemptyset(&mask);
//     sigaddset(&mask, SIGUSR1);
//     if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
//         cerr << "Failed to set signal mask\n";
//         return 1;
//     }

//     // Wait for signal with sigwait
//     int sig;
//     do {
//         sigwait(&mask, &sig);
//     } while (sig != SIGUSR1);

    

//     // Calculate number of iterations based on granularity
//     size_t granularity_count = static_cast<size_t>(granularity.count());
//     size_t iterations = static_cast<size_t>(duration_cast<milliseconds>(runDuration).count()) / granularity_count;

//     int iFile = 0;
//     for (size_t i = 0; i <= iterations; i++, iFile++) {
//         if ((i + 1) % BATCH_SIZE == 0) {   // Write to file
//             for (size_t j = 0; j < BATCH_SIZE; j++) {
//                 MemBench::MemoryInfo mem = memorySnapshots[j];
//                 file << mem.vmSize << "," 
//                      << mem.rssSize << "," 
//                      << ((i + 1) - BATCH_SIZE + j) * granularity_count << "\n";
//             }
//         }
//         current = MemBench::getProcessMemoryInfo(pid);
//         if (current.rssSize > max_memory)
//             kill(pid, SIGUSR1);
//         else if (current.rssSize < min_memory)
//             kill(pid, SIGUSR2);
            
//         memorySnapshots[i % BATCH_SIZE] = current;
//         std::this_thread::sleep_for(granularity);
//     }

//     // Write the remaining
//     int remaining = iFile % BATCH_SIZE;
//     for (int i = 0; i < remaining; i++) {
//         file << memorySnapshots[i].vmSize << "," 
//              << memorySnapshots[i].rssSize << "," 
//              << (iFile - remaining + i) * granularity_count << "\n";
//     }

//     // Flushes the buffer just at the end
//     file.close();

//     // Send SIGALRM to parent
//     kill(pid, SIGALRM);

//     return 0;
// }
