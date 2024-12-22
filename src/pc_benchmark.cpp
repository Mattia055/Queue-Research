#include "QueueTypeSet.hpp"
#include "SymmetricBenchmark.hpp"
#include "ProdConsBenchmark.hpp"
#include "MemoryBenchmark.hpp"

/**
 * 
 */
int main(int argc, char **argv) {
    using namespace bench;
    SymmetricBenchmark sym1(2, 0, 4096);

    // Call EnqueueDequeue with templates
    UnboundedQueues::foreach([&sym1]<typename Queue>() {
        sym1.EnqueueDequeue<Queue::template Type>(10, 10, "EnqDeq.csv");
    });

    return 0;
}
                