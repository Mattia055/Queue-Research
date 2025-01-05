#pragma once
#include <cstddef>  // For alignas
#include <mutex>
#include <queue>
#include <atomic>
#include <string>


template<typename T, bool bounded>
class MuxQueue {
private:
    std::queue<T*>   queue;
    mutex            mux;   
    size_t           size;


public:
    //tid parameter to be consistent with linked queues
    MuxQueue(size_t size_par, [[maybe_unused]] const int tid = 0):size{size_par}{};
    ~MuxQueue(){};

    size_t length([[maybe_unused]] const int tid = 0){ 
        lock_guard<mutex> lock(mux);
        return queue.size();
    }

    static inline std::string className([[maybe_unused]] bool padding=true){
        using namespace std::string_literals;
        return (bounded? "Bounded"s : ""s) + "MuxQueue"s;
    }

    __attribute__((used,always_inline)) bool push(T* item,[[maybe_unused]] const int tid = 0){
        std::lock_guard<std::mutex> lock(mux);
        if constexpr (bounded){
            if(queue.size() >= size) return false;
        }
        queue.push(item);
        return true;
    }

    __attribute__((used,always_inline)) T* pop([[maybe_unused]] const int tid = 0){
        T* item;
        lock_guard<mutex> lock(mux);
        if(!queue.empty()){
            item = queue.front();  //still need to know waht happens if queue it's empty
            queue.pop();
        } else item = nullptr;
        return item;
    }

};

template<typename T,bool bounded=true>
using BoundedMuxQueue = MuxQueue<T,bounded>;
template<typename T,bool bounded=false>
using LinkedMuxQueue = MuxQueue<T,bounded>;