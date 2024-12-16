#pragma once
#include <cstddef>  // For alignas
#include <mutex>
#include <queue>
#include <atomic>
#include <string>
#include <limits>

/**
 * Includes a LinkedAdapter for the mutex queue to make it 
 * usable via a macro (see end of file) to create instances
 * or execute benchmarks as wrote on the file ./include/Benchmarks.hpp 
 */

using namespace std;

/**
 * For now we work on the assumption that the queue is always 
 * unbounded.
 * 
 * The underlying structure of std::queue handles the growing and 
 * shrinking of the queue;
 * 
 * Supposedly this queue is always slower than lock free alternatives
 * it'd be more slow accounting for allocations of different rings
 */
template<typename T, bool bounded>
class Queue {
private:
    std::queue<T*>   muxQueue;
    mutex       mux;         

public:
    Queue(){};
    ~Queue(){};

    size_t estimateSize(int tid){ 
        lock_guard<mutex> lock(mux);
        return muxQueue.size();
    }

    inline void enqueue(T* item, const int tid){
        std::lock_guard<std::mutex> lock(mux);
        muxQueue.push(item);
        return;
    }

    inline T* dequeue(const int tid){
        T* item;
        lock_guard<mutex> lock(mux);
        if(!muxQueue.empty()){
            item = muxQueue.front();  //still need to know waht happens if queue it's empty
            muxQueue.pop();
        } else item = nullptr;
        return item;
    }

};

template<typename T,typename QueueType>
class MutexLinkedAdapter{
private:
    QueueType *m;
public:
    MutexLinkedAdapter(size_t maxThreads, size_t Buffer_size){
        m = new QueueType();
    }
    ~MutexLinkedAdapter(){
        delete m;
    }
    static inline std::string className(){
        return "MutexBasedQueue";
    }
    inline size_t estimateSize(int tid){return m->estimateSize(tid);}
    inline void enqueue(T* item, const int tid){return m->enqueue(item,tid);}
    inline T* dequeue(const int tid){return m->dequeue(tid);};
};
template<typename T,bool bounded=true>
using MutexBasedQueue = MutexLinkedAdapter<T,Queue<T,bounded>>;





    


    
