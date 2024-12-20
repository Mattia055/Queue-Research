#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <atomic>

#include "LinkedRingQueue.hpp"
#include "FAArray.hpp"
#include "LCRQ.hpp"
#include "LPRQ.hpp"

// Define type aliases for each queue type you want to test
template<typename V>
using QueuesToTest = ::testing::Types<FAAQueue<V>, LCRQueue<V>, LPRQueue<V>>;

// Test fixture for generic Queue types
template <typename Q>
class QueueTest : public ::testing::Test {
protected:
    Q queue;

    // Default constructor with parameters for different queue types
    QueueTest() : queue(128,128){}
};

// Register the test suite with the specific queue types
TYPED_TEST_SUITE(QueueTest, QueuesToTest<int>);

TYPED_TEST(QueueTest, EnqueueDequeue) {
    for(int i = 1 ; i < 10; i++){
        this->queue.enqueue(&i, 0);
        EXPECT_EQ(*this->queue.dequeue(0), i);
    }
}




int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();  // This runs all tests
}

