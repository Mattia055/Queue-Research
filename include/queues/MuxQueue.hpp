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
template<typename T, bool bounded>
class MuxQueue {
private:
    static constexpr size_t MAX_SIZE = 1024ULL;
    std::queue<T*>   muxQueue;
    mutex            mux;   
    size_t           max_size;


public:
    MuxQueue(size_t max_size=MAX_SIZE):max_size{max_size}{};
    MuxQueue(size_t maxThreads, size_t max_size):max_size{max_size}{};
    ~MuxQueue(){};

    inline size_t RingSize() const { 
        return max_size; 
    }

    size_t size([[maybe_unused]] const int tid){
        return size();
    }

    size_t size(){ 
        lock_guard<mutex> lock(mux);
        return muxQueue.size();
    }

    static inline std::string className([[maybe_unused]] bool padding=true){
        return "BoundedMuxQueue";
    }

    __attribute__((used,always_inline)) bool enqueue(T* item,[[maybe_unused]] const int tid){
        std::lock_guard<std::mutex> lock(mux);
        if constexpr (bounded){
            if(muxQueue.size() >= max_size) return false;
        }
        muxQueue.push(item);
        return true;
    }

    __attribute__((used,always_inline)) T* dequeue([[maybe_unused]] const int tid){
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
class MuxLinkedAdapter{
private:
    QueueType *m;
public:
    MuxLinkedAdapter(size_t maxThreads, size_t Buffer_size){
        m = new QueueType();
    }
    ~MuxLinkedAdapter(){
        delete m;
    }
    static inline std::string className([[maybe_unused]]bool padding=true){
        return "LinkedMuxQueue";
    }
    inline size_t size(int tid){return m->size(tid);}
    __attribute__((used,always_inline)) void enqueue(T* item, const int tid){m->enqueue(item,tid);}
    __attribute__((used,always_inline)) T* dequeue(const int tid){return m->dequeue(tid);};
};

template<typename T,bool bounded=true>
using BoundedMuxQueue = MuxQueue<T,bounded>;

template<typename T,bool bounded=false>
using LinkedMuxQueue = MuxLinkedAdapter<T,MuxQueue<T,bounded>>;