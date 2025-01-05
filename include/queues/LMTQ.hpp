#pragma once

#include <atomic>
#include <iostream>
#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"
#include "x86Atomics.hpp"

#include <chrono>

#define BACKOFF_MIN 128UL
#define BACKOFF_MAX 1024UL

/**
 * MACROS:  DISABLE_POW2
 *          DISABLE_DECAY 
 */

template <typename T,bool padded_cells, bool bounded>
class MTQueue : public QueueSegmentBase<T, MTQueue<T,padded_cells,bounded>>{
private:
    using Base = QueueSegmentBase<T, MTQueue<T,padded_cells,bounded>>;
    using Cell = detail::CRQCell<T*,padded_cells>;

    static constexpr size_t TRY_CLOSE = 10;

    Cell *array;
    const size_t size;
#ifndef DISABLE_POW2
    const size_t mask;  //Mask to execute the modulo operation
#endif

private:
    MTQueue(size_t size_param,[[maybe_unused]] const int tid, uint64_t start): 
    Base(),
#ifndef DISABLE_POW2
    size{detail::nextPowTwo(size_param)},
    mask{size - 1}
#else
    size{size_param}
#endif
    {
        if(size == 0)
            throw std::invalid_argument("Ring Size must be greater than 0");
        array = new Cell[size];
        for (uint64_t i = start; i < start + size; i++){
            array[i % size].val.store(nullptr,std::memory_order_relaxed);
            array[i % size].idx.store(i,std::memory_order_relaxed);

            Base::head.store(start,std::memory_order_relaxed);
            Base::tail.store(start,std::memory_order_relaxed);
        }
    }


public:
    MTQueue(size_t size,[[maybe_unused]] const int tid = 0): MTQueue(size,tid,0){} 

    ~MTQueue(){
        while(pop(0) != nullptr);
        delete[] array;
    }

    static std::string className(bool padding = true) {
        using namespace std::string_literals;
        return (bounded? "Bounded"s : ""s ) + "MTQueue"s + ((padded_cells && padding)? "/padded":"");
    }

    /*
        Nonblocking push operation.

        Operation fails if queue is full (if unbounded queue sets the segment as closed)
        1. load the current tail ticket
        2. check if the current segment is closed (if bounded dont do it)
        3. load the cell at the tail ticket index (modulo) and compare the index with tailticket
        4. if index is the same then try to push the item
        5. try to advance the index
        6. if cas successful then fill the cell with the item
        6.1. if cas fails then retry (wait for exponential decay)
        7. if index is not the same then check if the queue is full (if so close the queue and return false)
        8. else retry
    
    */
    __attribute__((used,always_inline)) bool push(T *item,[[maybe_unused]] const int tid){
        //puts("PUSHING");
        size_t tailTicket,idx;
        Cell *node;
        size_t bk = BACKOFF_MIN;
        size_t try_close = 0;
        while(true){
            tailTicket = Base::tail.load(std::memory_order_relaxed);
            if constexpr (!bounded){ //check if queue is closed if unbounded
                if(Base::isClosed(tailTicket)) {
                    return false;
                } 
            }
#ifndef DISABLE_POW2
            node = &(array[tailTicket & mask]);
#else
            node = &(array[tailTicket % size]);
#endif
            idx = node->idx.load(std::memory_order_acquire);
            if(tailTicket == idx){
                if(Base::tail.compare_exchange_strong(tailTicket,tailTicket + 1)) //try to advance the index
                    break;
                for (size_t i = 0; i < bk; ++i){asm("");} //so the loop isnt optimized out
                bk <<= 1;
                bk &= BACKOFF_MAX;
            } else {
                if(tailTicket > idx){
                    if constexpr (bounded){ //if queue is bounded then never closes the segment
                        return false;
                    }
                    else{
                        /*
                            The Base::closeSegment function is designed to close segments for fetch_add queues
                            to compensate we use tailTicket - 1 to close the current segment
                        */
                        if (Base::closeSegment(tailTicket-1, ++try_close > TRY_CLOSE)){
                            return false;
                        }
                    }
                }
            }
        }
        node->val = item;
        node->idx.store((idx + 1),std::memory_order_release);
        return true;
    }

    /*
        Nonblocking pop operation.
        Operation fails if queue is empty
        1. load the current head ticket
        2. load the cell at the head ticket index (modulo) and compare the index with headticket
        3. if index is headticket plus 1 then try extraction
        4. if index is less than headticket plus 1 then queue is empty
        5. try to advance the index (if cas fails then retry: wait exponential decay)
        6. if extraction successful then update the cell with the next index
        7. return the element (or nullptr if queue is empty)
    */
    __attribute__((used,always_inline)) T *pop([[maybe_unused]] const int tid){
        //puts("POPPING");
        size_t headTicket,idx;
        Cell *node;
        size_t bk = BACKOFF_MIN;
        T* item;    //item to return;

        while(true){
            headTicket = Base::head.load(std::memory_order_relaxed);
#ifndef DISABLE_POW2
            node = &array[headTicket & mask];
#else
            node = &array[headTicket % size];
#endif
            idx = node->idx.load(std::memory_order_acquire);
            long diff = idx - (headTicket + 1);
            if(diff == 0){
                if(Base::head.compare_exchange_strong(headTicket,headTicket + 1)) //try to advance the head
                    break;
                for (size_t i = 0; i < bk; ++i){asm("");} //so the loop isnt optimized out
                bk <<= 1;
                bk &= BACKOFF_MAX;
            } 
            // else if(diff < 0){//queue is empty [if segment is closed switch to next]
            //     if constexpr (bounded){
            //         if(diff < 0){
            //             return nullptr;
            //         }
            //     } else {
            //         if(Base::tailIndex(Base::tail) <= headTicket){
            //             return nullptr;
            //         }
            //     }
            // }   
            else if (diff < 0){
                if constexpr (bounded){
                    if(Base::tail.load() <= headTicket){
                        return nullptr;
                    }
                }
                else{
                    if(Base::tailIndex(Base::tail.load()) <= headTicket)
                        return nullptr;
                }
            }
        }

        //std::cerr << "Pushing " << item << " with " << headTicket << " at "<< headTicket % size << std::endl;
        item = node->val;
        node->idx.store(headTicket + size,std::memory_order_release);
        return item;
    }

    inline size_t length([[maybe_unused]] const int tid = 0) const {
        if constexpr (bounded){
            uint64_t length = Base::tail.load() - Base::head.load();
            return length < 0 ? 0 : length > size ? size : length;
        } else {
            return Base::length();
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