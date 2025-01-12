#include <chrono>
#include <string>
#include <signal.h>
#include <array>
#include <fstream>
#include <thread>
#include <iostream>
#include <cstdlib>

#include "MemoryBenchmark.hpp"

const size_t BATCH_SIZE = 100;

int main(int argc, char **argv) {
    using MemBench = bench::MemoryBenchmark;
    using namespace std;
    using namespace chrono;

    if (argc != 8) {
        cerr << "Usage: " << argv[0] << " <pid> <runDuration> <granularity> <file_name> <max_memory> <min_memory> <monitor>\n";
        return 1;
    }

    // Cast parameters
    pid_t pid = static_cast<pid_t>(stoi(argv[1]));
    seconds runDuration = seconds(stoi(argv[2]));
    milliseconds granularity = milliseconds(stoi(argv[3]));
    string file_name = argv[4];
    size_t max_memory = (stol(argv[5]));
    size_t min_memory = (stol(argv[6]));
    bool monitor = (stoi(argv[7]));

    // Open file
    ofstream file(file_name, ios::app);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << file_name << "\n";
        return 1;
    }

    // Append header if file is empty
    if (MemBench::notFile(file_name)) {
        file << "vmSize,rssSize,time\n";
    }

    array<MemBench::MemoryInfo, BATCH_SIZE> memorySnapshots;
    MemBench::MemoryInfo current;

    // Monitor the memory before warmup
    memorySnapshots[0] = MemBench::getProcessMemoryInfo(pid);

    // Set signal mask
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
        cerr << "Failed to set signal mask\n";
        return 1;
    }

    // Wait for signal with sigwait
    int sig;
    do {
        sigwait(&mask, &sig);
    } while (sig != SIGUSR1);

    

    // Calculate number of iterations based on granularity
    size_t granularity_count = static_cast<size_t>(granularity.count());
    size_t iterations = static_cast<size_t>(duration_cast<milliseconds>(runDuration).count()) / granularity_count;

    int iFile = 0;
    for (size_t i = 0; i <= iterations; i++, iFile++) {
        if ((i + 1) % BATCH_SIZE == 0) {   // Write to file
            for (size_t j = 0; j < BATCH_SIZE; j++) {
                MemBench::MemoryInfo mem = memorySnapshots[j];
                file << mem.vmSize << "," 
                     << mem.rssSize << "," 
                     << ((i + 1) - BATCH_SIZE + j) * granularity_count << "\n";
            }
        }
        current = MemBench::getProcessMemoryInfo(pid);
        if (current.rssSize > max_memory)
            kill(pid, SIGUSR1);
        else if (current.rssSize < min_memory)
            kill(pid, SIGUSR2);
            
        memorySnapshots[i % BATCH_SIZE] = current;
        std::this_thread::sleep_for(granularity);
    }

    // Write the remaining
    int remaining = iFile % BATCH_SIZE;
    for (int i = 0; i < remaining; i++) {
        file << memorySnapshots[i].vmSize << "," 
             << memorySnapshots[i].rssSize << "," 
             << (iFile - remaining + i) * granularity_count << "\n";
    }

    // Flushes the buffer just at the end
    file.close();

    // Send SIGALRM to parent
    kill(pid, SIGALRM);

    return 0;
}
