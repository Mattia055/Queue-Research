#pragma once
#include <string>

template<typename T,typename Adaptee>
class LinkedAdapter {
private:
    Adaptee queue;
public: 
    LinkedAdapter(size_t size, size_t threads = 0):queue{Adaptee(size,threads)}{};
    LinkedAdapter(){};

    static inline std::string className(bool padding = true){
        using namespace std::string_literals;
        return "Linked"s + Adaptee::className(padding);
    }

    __attribute__((used,always_inline)) void push(T* item,[[maybe_unused]] const int tid = 0){
        queue.push(item,tid);   //this operation always succeds
    }
    __attribute__((used,always_inline)) T* pop([[maybe_unused]] const int tid  = 0){
        return queue.pop(tid);
    }
    __attribute__((used,always_inline)) size_t length([[maybe_unused]] const int tid = 0){
        return queue.length(tid);
    }
};