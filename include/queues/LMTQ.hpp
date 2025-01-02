#pragma once

#include <atomic>
#include <iostream>
#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"
#include "x86Atomics.hpp"

#include <chrono>

#define BACKOFF_MIN 128UL
#define BACKOFF_MAX 1024UL

//POWER_OF_TWO RING_SIZE

template <typename T,bool padded_cells, bool bounded>
class MTQueue : public QueueSegmentBase<T, MTQueue<T,padded_cells,bounded>>{
private:
    using Base = QueueSegmentBase<T, MTQueue<T,padded_cells,bounded>>;
    using Cell = detail::CRQCell<T*,padded_cells>;

    Cell *array;
    const size_t Ring_Size;
#ifndef DISABLE_POWTWO_SIZE
    const size_t mask;  //Mask to execute the modulo operation
#endif

public:
    MTQueue(size_t RingSize=Base::RING_SIZE): 
    Base(),
#ifndef DISABLE_POWTWO_SIZE
    Ring_Size{detail::nextPowTwo(RingSize)},
    mask{Ring_Size - 1}
#else
    Ring_Size{RingSize}
#endif
    {
        if(Ring_Size == 0)
            throw std::invalid_argument("Ring Size must be greater than 0");
        array = new Cell[Ring_Size];
        for (uint64_t i = 0; i < Ring_Size; i++){
#ifndef DISABLE_POWTWO_SIZE
            uint64_t j = i & mask;
#else
            uint64_t j = i % Ring_Size;
#endif
            array[j].val.store(nullptr,std::memory_order_relaxed);
            array[j].idx.store(i,std::memory_order_relaxed);

            Base::head.store(0,std::memory_order_relaxed);
            Base::tail.store(0,std::memory_order_relaxed);
        }
    }

    MTQueue(size_t threads,size_t Ring_Size): MTQueue(Ring_Size){}

    ~MTQueue(){
        delete[] array;
    }

    static std::string className(bool padding = true) {
        using namespace std::string_literals;
        return (bounded? "Bounded"s : ""s ) + "MTQueue"s + ((padded_cells && padding)? "/padded":"");
    }

    __attribute__((used,always_inline)) bool enqueue(T *item,[[maybe_unused]] const int tid){
        uint64_t tailTicket;
        uint64_t idx;
        size_t bk = BACKOFF_MAX;
        size_t try_close = 0;

        while(true){
            tailTicket = Base::tail.load(std::memory_order_relaxed);
            if constexpr (bounded == false){    //Tantrum queue if unbounded
                if(Base::isClosed(tailTicket))
                    return false;  
            }
#ifndef DISABLE_POWTWO_SIZE
            Cell &cell = array[tailTicket & mask];
#else
            Cell &cell = array[tailTicket % Ring_Size];
#endif
            idx = cell.idx.load(std::memory_order_acquire);
            if(tailTicket == idx){
                if(Base::tail.compare_exchange_weak(tailTicket,tailTicket+1,std::memory_order_relaxed)){
                    cell.val.store(item,std::memory_order_relaxed);
                    cell.idx.store(idx+1,std::memory_order_release);
                    return true;
                }
                
                    for(unsigned int i = 0; i < bk; ++i){}
                    bk <<= 1;
                    bk &= BACKOFF_MAX;
                
            } else{
                if (tailTicket > idx){
                    if constexpr (bounded){
                        return false;
                    }
                    else{
                        if (Base::closeSegment(tailTicket, ++try_close > Base::TRY_CLOSE))
                            return false;
                    }
                }
            }
        }
    }
    __attribute__((used,always_inline)) T *dequeue([[maybe_unused]] const int tid){
        uint64_t headTicket;
        uint64_t idx;
        size_t bk = BACKOFF_MAX;
        while(true){
            headTicket = Base::head.load(std::memory_order_relaxed);
#ifndef DISABLE_POWTWO_SIZE
            Cell &cell = array[headTicket & mask];
#else
            Cell &cell = array[headTicket % Ring_Size];
#endif
            idx = cell.idx.load(std::memory_order_acquire);
            int diff =  idx - (headTicket+1);
            if(diff == 0){
                if(Base::head.compare_exchange_weak(headTicket,headTicket+1,std::memory_order_relaxed)){
                    T *val = cell.val.load(std::memory_order_relaxed);
                    cell.idx.store((headTicket+Ring_Size),std::memory_order_release);
                    return val;
                }

                for(unsigned int i = 0; i < bk; ++i){}
                bk <<= 1;
                bk &= BACKOFF_MAX;
            } else {
                if(diff < 0) return nullptr;
            }
        }

    }

    inline size_t RingSize() const { 
        return Ring_Size; 
    }

    inline size_t size() const {
        if constexpr (bounded){
            uint64_t length = Base::tail.load() - Base::head.load();
            return length < 0 ? 0 : length > Ring_Size ? Ring_Size : length;
        } else {
            return Base::size();
        }
    }

    friend class LinkedRingQueue<T,MTQueue<T,padded_cells,bounded>>;   

};

#ifndef NO_PADDING
template<typename T,bool padded_cells=true,bool bounded=false>
#else
template<typename T,bool padded_cells=true,bool bounded=false>
#endif
using LMTQueue = LinkedRingQueue<T,MTQueue<T,padded_cells,bounded>>;

#ifndef NO_PADDING
template<typename T,bool padded_cells=true,bool bounded=true>
#else
template<typename T,bool padded_cells=false,bool bounded=true>
#endif
using BoundedMTQueue = MTQueue<T,padded_cells,bounded>;