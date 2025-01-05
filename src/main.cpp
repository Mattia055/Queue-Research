// #include <LinkedRingQueue.hpp>
// #include <MuxQueue.hpp>
// #include <FAArray.hpp>
// #include <LCRQ.hpp>
// #include <LPRQ.hpp>
#include <LMTQ.hpp>
#include <LCRQ.hpp>
#include <iostream>
#include "SymmetricBenchmark.hpp"
#include <chrono>
#include "PairsBenchmark.hpp"

int main(void){
    //LMTQueue<int> queue(128, 128);
    bench::Benchmark::Arguments args(true,true,false);
    // bench::PairsBenchmark bench(1,1,0.0,false,1024,1000000,args);
    // bench.ProducerConsumer<LCRQueue>(std::chrono::seconds{5}, 10);
    bench::SymmetricBenchmark bench(6,0.0,1024*8,100000,args);
    bench.EnqueueDequeue<BoundedMTQueue>(1000000,10);
    
}



// #include <iostream>
// #include <sstream>
// #include <vector>
// #include <string>
// #include <stdexcept>
// #include <chrono>

// #include "LMTQ.hpp"
// #include "Benchmark.hpp"
// #include "SymmetricBenchmark.hpp"
// #include "PairsBenchmark.hpp"
// #include "QueueTypeSet.hpp"
// #include "Format.hpp"
// #include "numa_support.hpp"

// using bench::Benchmark;

// // int main() {
// //     LMTQueue<uintptr_t> queue(128, 128);

// //     // Enqueue values
// //     for (uintptr_t i = 1; i < 128000; i++) {
// //         queue.enqueue(reinterpret_cast<uintptr_t*>(i), 0);
// //     }

// //     // Dequeue and compare
// //     for (uintptr_t i = 1; i < 128000; i++) {
// //         uintptr_t* value = queue.dequeue(0);
// //         if (reinterpret_cast<uintptr_t>(value) != i) {
// //             std::cout << "Error at " << i << std::endl;
// //         }
// //     }

// //     std::cout << "Test passed" << std::endl;
// //     return 0;
// // }




// template <typename T>
// void print_vector(const std::vector<T>& vec);

// int main(int argc, char **argv){

//     Format format(argv,argc);
//     Format::FormatKeys keys;

//     // Parse
//     // cout << format.path << endl;
//     // cout << keys.threads << "->"; print_vector(format.threads);
//     // cout << keys.producers << "->";print_vector(format.producers);
//     // cout << keys.consumers << "->";print_vector(format.consumers); 
//     // cout << keys.iterations << "->" << (format.iterations) << endl; 
//     // cout << keys.runs << "->" << format.runs << endl;
//     // cout << keys.duration << "->" << format.duration << endl;
//     // cout << keys.granularity << "->" << format.granularity << endl;
//     // cout << keys.warmup << "->"; print_vector(format.warmup);
//     // cout << keys.additionalWork << "->"; print_vector(format.additionalWork);
//     // cout << keys.queueFilter << "->"; print_vector(format.queueFilter);
//     // cout << keys.sizes << "->"; print_vector(format.sizes);
//     // cout << keys.balanced << "->" << format.balanced << endl;
//     // cout << keys.ratio << "->";print_vector(format.ratio);
//     // return 0;

//     if(format.name == "EnqueueDequeue")
//         SymmetricBenchmark::runSeries(format);
//     else if (format.name == "Pairs")
//         PairsBenchmark::runSeries(format);
//     else
//         puts("Benchmark not found");

//     return 0;

// }

// template <typename T>
// void print_vector(const std::vector<T>& vec) {
//     std::cout << "[ ";
//         for (const auto& item : vec) {
//             if constexpr (std::is_same_v<T, std::chrono::milliseconds> || std::is_same_v<T, std::chrono::seconds>) {
//                 std::cout << item.count() << " ";
//             } else
//                 std::cout << item << " "; 
//         }
//     std::cout << "]" << std::endl;
// }