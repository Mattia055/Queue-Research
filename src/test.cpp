#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <atomic>
#include <barrier>
#include <vector>
#include <numeric>
#include <random>

//#include "LinkedRingQueue.hpp"
#include "FAArray.hpp"
#include "LCRQ.hpp"
#include "LPRQ.hpp"
#include "ThreadGroup.hpp"


// Define type aliases for each queue type you want to test
template<typename V>
using UnboundedQueues = ::testing::Types<FAAQueue<V>,LCRQueue<V>, LPRQueue<V>>;
template<typename V>
using BoundedQueues = ::testing::Types<BoundedCRQueue<V>, BoundedPRQueue<V>>;

// Test setup for unbounded queues
template <typename Q>
class Unbounded_Traits : public ::testing::Test {
private:
    static constexpr int Threads = 128;
public:
    static constexpr size_t RING_SIZE = 20;
    size_t RingSize = RING_SIZE;

    Q queue;

    // Default constructor with parameters for different queue types
    Unbounded_Traits() : queue(Threads,RING_SIZE){}
};

using UQueuesOfInts = UnboundedQueues<int>;

// Suite for UnboundedQueues
TYPED_TEST_SUITE(Unbounded_Traits, UQueuesOfInts);

TYPED_TEST(Unbounded_Traits, EnqueueDequeue) {
    TypeParam& queue = this->queue;
    for(int i = 1 ; i < 30; i++){
        queue.enqueue(&i, 0);
        EXPECT_EQ(*queue.dequeue(0), i);
    }

    EXPECT_EQ(queue.dequeue(0), nullptr);
    EXPECT_EQ(queue.size(0),0);
}

TYPED_TEST(Unbounded_Traits, OverflowRing) {
    TypeParam& queue = this->queue;
    // Create an array of integers to enqueue
    int values[30];  // Ensure there are 30 values to enqueue
    for (int i = 0; i < 30; i++) {
        values[i] = i + 1;  // Initialize with values from 1 to 30
        queue.enqueue(&values[i], 0);  // Enqueue the address of each value
    }

    // Dequeue and check that the values match
    for (int i = 0; i < 30; i++) {
        EXPECT_EQ(*reinterpret_cast<int*>(queue.dequeue(0)), values[i]);
    }

    EXPECT_EQ(queue.dequeue(0), nullptr);
    EXPECT_EQ(queue.size(0),0);
}

TYPED_TEST(Unbounded_Traits, EnqueueDequeueStress) {
    TypeParam& queue = this->queue;
    size_t size = 32;
    int values[size];

    for(int i = 0; i< 10; ++i) {
        for(int j = 0; j < 2048; ++j) {
            int* val = &(values[j%size]);
            queue.enqueue(val, 0);
            EXPECT_EQ(j+1,queue.size(0)) << "Failed at insertion " << j << " of run " << i;
        }

        for(int j = 0; j < 2048; ++j){
            int* val = &(values[j%size]);
            EXPECT_EQ(queue.dequeue(0),val) << "Failed at extraction " << j << " of run " << i;
            //EXPECT_EQ(2048 - j - 1,queue.size(0)) << "Failed at iteration " << j << " of run " << i;
        }

        EXPECT_EQ(queue.dequeue(0), nullptr);
        EXPECT_EQ(queue.size(0),0);
    }

}

//Suite for BoundedQueues
template <typename Q>
class Bounded_Traits : public ::testing::Test {
public:
    static constexpr size_t RING_SIZE = 10;
    size_t RingSize = RING_SIZE;

    Q queue;

    // Default constructor with parameters for different queue types
    Bounded_Traits() : queue(static_cast<size_t>(RING_SIZE)){}
};

using BQueuesOfInts = BoundedQueues<int>;

TYPED_TEST_SUITE(Bounded_Traits, BQueuesOfInts);

TYPED_TEST(Bounded_Traits, OverflowRing) {
    TypeParam& queue = this->queue;
    //Queue must be full but not overflown
    int values[32];
    for(int i = 0; i< 32; i++)
        values[i] = i+1;

    int try_overwrite = 0;

    for(int i = 0; i< this->RingSize; i++){
        //init values and ship them
        EXPECT_EQ(queue.enqueue(&(values[i]), 0),true);
    }
    
    for(int i = 0; i< 100; i++)
        EXPECT_EQ(queue.enqueue(&try_overwrite,0),false);
    
    for(int i = 0; i< this->RingSize; i++)
        EXPECT_EQ(*queue.dequeue(0),values[i]);
    

    EXPECT_EQ(queue.dequeue(0), nullptr);
    EXPECT_EQ(queue.size(),0);

}

TYPED_TEST(Bounded_Traits, UnderflowRing){
    TypeParam& queue = this->queue;
    int values[this->RingSize];
    int try_overwrite = 0;
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> dis(this->RingSize + 1,(this->RingSize * 10) + 1);
    
    for(int j = 0; j< 100; j++){
        for(int i = 0; i<this->RingSize; i++){
            values[i] = i+1;
            EXPECT_EQ(queue.enqueue(&(values[i]),0),true);
        }

        int n_enqueue;
        do{
            n_enqueue = dis(gen);
        } while((n_enqueue % this->RingSize) == 0);
        //Try to mess up the order of elements

        for(int i = 0; i< n_enqueue; i++)
            EXPECT_EQ(queue.enqueue(&(try_overwrite),0),false);

        for(int i = 0; i< this->RingSize; i++){
            EXPECT_EQ(*queue.dequeue(0),values[i]);
        }
    }
}

TYPED_TEST(Bounded_Traits, EnqueueDequeueStress){
    TypeParam& queue = this->queue;
    size_t size = 32;
    int values[size];

    //init the values
    for(int i = 0; i< 32; i++){
        values[i] = i+1;
    }

    int try_overwrite = 0;

    for(int i = 0; i< 10; ++i) {
        for(int j = 0; j < this->RingSize; ++j) {
            EXPECT_EQ(queue.enqueue(&(values[j%size]), 0),true);
            EXPECT_EQ(j+1,queue.size()) << "Failed at insertion " << j << " of run " << i;
        }

        for(int i = 0; i< 2048; i++){
            EXPECT_EQ(queue.enqueue(&try_overwrite,0),false);
            EXPECT_EQ(queue.size(),this->RingSize);    //full queue;
        }

        for(int j = 0; j < this->RingSize; ++j){
            EXPECT_EQ((queue.dequeue(0)),&(values[j%size])) << "Failed at extraction " << j << " of run " << i;
            //EXPECT_EQ(this->RingSize - j - 1,queue.size()) << "Failed at iteration " << j << " of run " << i;
        }

        EXPECT_EQ(queue.dequeue(0), nullptr);
        EXPECT_EQ(queue.size(),0);
    }
}

struct UserData {
    int tid;
    size_t id;

    auto operator <=> (const UserData&) const = default;
};

using UQueuesOfUserData = UnboundedQueues<UserData>;

// Test setup for unbounded queues
template <typename Q>
class Unbounded_Concurrent : public ::testing::Test {
private:
    static constexpr int Threads = 128;
public:
    static constexpr size_t RING_SIZE = 1024;
    size_t RingSize = RING_SIZE;

    Q queue;

    // Default constructor with parameters for different queue types
    Unbounded_Concurrent() : queue(Threads,RING_SIZE){}
};


TYPED_TEST_SUITE(Unbounded_Concurrent, UQueuesOfUserData);

/**
 * 2 groups of threads producer and consumer enqueue 
 * and dequeue from the queue: check that all items
 * have been inserted and dequeued from exactely one 
 * thread
 */
TYPED_TEST(Unbounded_Concurrent,TransferAllItems){
    TypeParam& queue = this->queue;
    std::atomic<bool> stopFlag{false};
    const size_t numRuns = 10;
    const size_t iter = 20'000;

    for(int iThread = 1 ; iThread < numRuns; iThread++){
        std::vector<uint64_t> sum(iThread);
        std::barrier<> prodBarrier(iThread + 1);
        ThreadGroup prod, cons;

        const auto producer = [&queue,iter,&prodBarrier](const int tid){
            UserData ud[iter];
            for(size_t i = 1; i<= iter; i++){
                ud[i-1] = {tid,i};
                queue.enqueue(&ud[i-1],tid);
            }
            prodBarrier.arrive_and_wait();
            prodBarrier.arrive_and_wait();
            return;
        };

        const auto consumer = [&queue,&stopFlag](const int tid){
            uint64_t sum = 0;
            UserData* ud;
            while(!stopFlag.load())
                if((ud = queue.dequeue(tid)) != nullptr) sum += ud->id;
            do{
                if((ud = queue.dequeue(tid)) != nullptr) sum += ud->id;
            }while(ud != nullptr);
            return sum;
        };

        for(int jProd = 0; jProd < iThread ; jProd++)
            prod.thread(producer);
        for(int jCons = 0; jCons < iThread ; jCons++)
            cons.threadWithResult(consumer,sum[jCons]);
        /**
         * When producers are done signal consumer to start, producers cannot 
         * return before consumers are done emptying the queue, because we would
         * have stack-use-after-return issues. So we use a barrier to synchronize 
         */
        prodBarrier.arrive_and_wait();
        stopFlag.store(true);
        cons.join();
        stopFlag.store(false);
        prodBarrier.arrive_and_wait();
        prod.join();

        uint64_t total = std::accumulate(sum.begin(),sum.end(),0);

        EXPECT_EQ(total/iThread, iter*(iter+1)/2)   << "Failed at run " << iThread 
                                                    << "Got " << total/iThread << "Expected " 
                                                    << iter*(iter+1)/2;
        EXPECT_EQ(queue.dequeue(0),nullptr);
        EXPECT_EQ(queue.size(0),0);
        
    }
}

/**
 * Schedule 2 groups of threads producer and consumer to enqueue and dequeue
 * test if data is correctly dequeued (older data before newer data)
 */
TYPED_TEST(Unbounded_Concurrent,QueueSemantics){
    TypeParam& queue = this->queue;
    const int numRuns = 4;
    const int iter = 10'000;
    std::atomic<bool> stopFlag{false};
    for(int iThread = 1 ; iThread < numRuns; iThread++){
        barrier<>   barrierProd(iThread + 1);
        barrier<>   barrierCons(iThread + 1);
        ThreadGroup prod, cons;
        std::vector<std::vector<UserData>> producersData(iThread);
        std::vector<std::vector<UserData>> consumersData(iThread);

        //initialize the producers matrix
        for(int i = 0; i < iThread; i++){
            for(size_t j = 0; j < iter; j++)
                producersData[i].push_back(UserData{i,j});
        }

        const auto prod_lambda = [&queue,&barrierProd,&producersData](const int tid){
            for(auto& elem : producersData[tid])
                queue.enqueue(&elem,tid);
            barrierProd.arrive_and_wait();
            barrierProd.arrive_and_wait();
            return;
        };

        const auto cons_lambda = [&queue,&stopFlag,&consumersData](const int tid){
            UserData* ud;
            while(!stopFlag.load())
                if((ud = queue.dequeue(tid)) != nullptr) consumersData[tid].push_back(*ud);
            do{
                if((ud = queue.dequeue(tid)) != nullptr) consumersData[tid].push_back(*ud);
            }while(ud != nullptr);
        };

        for(int jProd = 0; jProd < iThread ; jProd++)
            prod.thread(prod_lambda);

        for(int jCons = 0; jCons < iThread ; jCons++)
            cons.thread(cons_lambda);

        barrierProd.arrive_and_wait();
        stopFlag.store(true);
        cons.join();
        stopFlag.store(false);
        barrierProd.arrive_and_wait();
        prod.join();

        //check if data is correctly dequeued
        for(std::vector<UserData> consData : consumersData){
            //sort the vector
            std::stable_sort(consData.begin(),consData.end(),[](const UserData& a, const UserData& b){return a.tid < b.tid;});

            for(size_t i = 1; i < consData.size(); i++){
                const UserData& deq1 = consData[i-1];
                const UserData& deq2 = consData[i];
                if(deq1.tid == deq2.tid) EXPECT_LT(deq1.id,deq2.id) << "Failed at run " << iThread;  
            }
        }

        std::multiset<UserData> prodsJoined;
        for (const auto& data : producersData)
            prodsJoined.insert(data.begin(), data.end());

        std::multiset<UserData> consJoined;
        for (const auto& data : consumersData)
            consJoined.insert(data.begin(), data.end());

        EXPECT_EQ(prodsJoined.size(), consJoined.size());
        EXPECT_EQ(prodsJoined, consJoined);
    }

}

// Test setup for unbounded queues
template <typename Q>
class Bounded_Concurrent : public ::testing::Test {
public:
    static constexpr size_t RING_SIZE = 4096;
    size_t RingSize = RING_SIZE;

    Q queue;

    // Default constructor with parameters for different queue types
    Bounded_Concurrent() : queue(RING_SIZE){}
};

using BQueuesOfUserData = BoundedQueues<UserData>;

TYPED_TEST_SUITE(Bounded_Concurrent, BQueuesOfUserData);

TYPED_TEST(Bounded_Concurrent,TransferAllItems){
    TypeParam& queue = this->queue;
    std::atomic<bool> stopFlag{false};
    const size_t numRuns = 4;
    const size_t iter = 10'000;

    for(int iThread = 1 ; iThread < numRuns; iThread++){
        std::vector<uint64_t> sum(iThread);
        std::barrier<> prodBarrier(iThread + 1);
        ThreadGroup prod, cons;

        const auto producer = [&queue,iter,&prodBarrier](const int tid){
            UserData ud[iter];
            for(size_t i = 1; i<= iter; i++){
                ud[i-1] = {tid,i};
                while(queue.enqueue(&ud[i-1],tid) == false);
            }
            prodBarrier.arrive_and_wait();
            prodBarrier.arrive_and_wait();
            return;
        };

        const auto consumer = [&queue,&stopFlag](const int tid){
            uint64_t sum = 0;
            UserData* ud;
            while(!stopFlag.load())
                if((ud = queue.dequeue(tid)) != nullptr) sum += ud->id;
            do{
                if((ud = queue.dequeue(tid)) != nullptr) sum += ud->id;
            }while(ud != nullptr);
            return sum;
        };

        for(int jProd = 0; jProd < iThread ; jProd++)
            prod.thread(producer);
        for(int jCons = 0; jCons < iThread ; jCons++)
            cons.threadWithResult(consumer,sum[jCons]);
        /**
         * When producers are done signal consumer to start, producers cannot 
         * return before consumers are done emptying the queue, because we would
         * have stack-use-after-return issues. So we use a barrier to synchronize 
         */
        prodBarrier.arrive_and_wait();
        stopFlag.store(true);
        cons.join();
        stopFlag.store(false);
        prodBarrier.arrive_and_wait();
        prod.join();

        uint64_t total = std::accumulate(sum.begin(),sum.end(),0);

        EXPECT_EQ(total/iThread, iter*(iter+1)/2)   << "Failed at run " << iThread 
                                                    << "Got " << total/iThread << "Expected " 
                                                    << iter*(iter+1)/2;
        EXPECT_EQ(queue.dequeue(0),nullptr); 
    }
}

/**
 * Schedule 2 groups of threads producer and consumer to enqueue and dequeue
 * test if data is correctly dequeued (older data before newer data)
 */
TYPED_TEST(Bounded_Concurrent,QueueSemantics){
    TypeParam& queue = this->queue;
    const int numRuns = 4;
    const int iter = 10'000;
    std::atomic<bool> stopFlag{false};
    for(int iThread = 1 ; iThread < numRuns; iThread++){
        barrier<>   barrierProd(iThread + 1);
        barrier<>   barrierCons(iThread + 1);
        ThreadGroup prod, cons;
        std::vector<std::vector<UserData>> producersData(iThread);
        std::vector<std::vector<UserData>> consumersData(iThread);

        //initialize the producers matrix
        for(int i = 0; i < iThread; i++){
            for(size_t j = 0; j < iter; j++)
                producersData[i].push_back(UserData{i,j});
        }

        const auto prod_lambda = [&queue,&barrierProd,&producersData](const int tid){
            for(auto& elem : producersData[tid])
                while(queue.enqueue(&elem,tid) == false);
        };

        const auto cons_lambda = [&queue,&stopFlag,&consumersData](const int tid){
            UserData* ud;
            while(!stopFlag.load())
                if((ud = queue.dequeue(tid)) != nullptr) consumersData[tid].push_back(*ud);
            do{
                if((ud = queue.dequeue(tid)) != nullptr) consumersData[tid].push_back(*ud);
            }while(ud != nullptr);
        };

        for(int jProd = 0; jProd < iThread ; jProd++)
            prod.thread(prod_lambda);

        for(int jCons = 0; jCons < iThread ; jCons++)
            cons.thread(cons_lambda);

        prod.join();
        stopFlag.store(true);
        cons.join();
        stopFlag.store(false);

        //check if data is correctly dequeued
        for(std::vector<UserData> consData : consumersData){
            //sort the vector
            std::stable_sort(consData.begin(),consData.end(),[](const UserData& a, const UserData& b){return a.tid < b.tid;});

            for(size_t i = 1; i < consData.size(); i++){
                const UserData& deq1 = consData[i-1]; 
                const UserData& deq2 = consData[i];
                if(deq1.tid == deq2.tid) EXPECT_LT(deq1.id,deq2.id) << "Failed at run " << iThread;  
            }
        }

        std::multiset<UserData> prodsJoined;
        for (const auto& data : producersData)
            prodsJoined.insert(data.begin(), data.end());

        std::multiset<UserData> consJoined;
        for (const auto& data : consumersData)
            consJoined.insert(data.begin(), data.end());

        EXPECT_EQ(prodsJoined.size(), consJoined.size());
        EXPECT_EQ(prodsJoined, consJoined);
    }

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();  // This runs all tests

    
}

