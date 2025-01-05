#pragma once

#include <atomic>
#include <cassert>

#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"
#include "x86Atomics.hpp"
#include "numa_support.hpp"

using namespace std;

/*
    Macros:
    DISABLE_PADDING: disables padding for cells
    DISABLE_POW2: Disables power of 2 modulo ops
    CAUTIOUS_DEQUEUE: check that the queue is empty before attempt pop
*/


template <typename T, bool padded_cells, bool bounded>
class CRQueue : public QueueSegmentBase<T, CRQueue<T, padded_cells, bounded>> {
private:

    using Base = QueueSegmentBase<T, CRQueue<T, padded_cells, bounded>>;
    using Cell = detail::CRQCell<T *, padded_cells>;

    static constexpr size_t TRY_CLOSE = 10;

    size_t size;
#ifndef DISABLE_POW2
    size_t mask;  //Mask to execute the modulo operation
#endif
    Cell *array;

    inline uint64_t nodeIndex(uint64_t i)   const {return (i & ~(1ull << 63));}
    inline uint64_t nodeUnsafe(uint64_t i)  const {return i & (1ull << 63);}
    inline uint64_t setUnsafe(uint64_t i)   const {return (i | (1ull << 63));}

private:
    CRQueue(size_t size_par,[[maybe_unused]] const int tid, const uint64_t start): Base(), 
#ifndef DISABLE_POW2
    size{detail::nextPowTwo(size_par)},
    mask{size - 1}
#else
    size{size_par}
#endif
    {
        assert(size_par > 0);
        array = new Cell[size];

        for(uint64_t i = start; i < start + size; ++i){
            array[i % size].val.store(nullptr,memory_order_relaxed);
            array[i % size].idx.store(i,memory_order_relaxed);           
        }

        Base::head.store(start,memory_order_relaxed);
        Base::tail.store(start,memory_order_relaxed);
        //Numa optimization
        Base::cluster.store((isNumaAvailable() ? getNumaNode() : 0), memory_order_relaxed );
    }


public: 
    //uses the tid argument to be consistent with linked queues
    CRQueue(size_t size_par,[[maybe_unused]] const int tid = 0): CRQueue(size_par,tid,0){}

    ~CRQueue() { 
        while(pop(0) != nullptr);
        delete[] array; 
    }

    static std::string className(bool padding = true){
        using namespace std::string_literals;
        return (bounded? "Bounded"s : ""s) + "CRQueue"s + ((padded_cells && padding)? "/padded":"");
    }

    /*
        Takes an additional tid parameter to keep the interface compatible with
        LinkedRingQueue->push
        The parameter has a default value so that it can be omitted
    */
    __attribute__((used,always_inline)) bool push(T *item,[[maybe_unused]] const int tid = 0)
    {
        int try_close = 0;

        while (true)
        {
            Base::safeCluster();
            uint64_t tailTicket = Base::tail.fetch_add(1);

            if constexpr (bounded == false){    //if LinkedRingQueue Segment then checks if the segment is closed
                if(Base::isClosed(tailTicket)){
                    return false;
                }
            }
#ifndef DISABLE_POW2
            Cell &cell = array[tailTicket & mask];
#else
            Cell &cell = array[tailTicket % size];
#endif
            uint64_t idx = cell.idx.load();
            if (cell.val.load() == nullptr)
            {
                if (nodeIndex(idx) <= tailTicket)
                {
                    if ((!nodeUnsafe(idx) || Base::head.load() < tailTicket))
                    {
                        if (CAS2((void **)&cell, nullptr, idx, item, tailTicket))
                            return true;
                    }
                }
            }

            if (tailTicket >= Base::head.load() + size)
            {   
                if constexpr (bounded){ //if queue is bounded then never closes the segment
                    return false;
                }
                else{
                    if (Base::closeSegment(tailTicket, ++try_close > TRY_CLOSE)){
                        return false;
                    }
                }
            }
        }
    }

     /*
        Takes an additional tid parameter to keep the interface compatible with
        LinkedRingQueue->pop
        The parameter has a default value so that it can be omitted
    */
    __attribute__((used,always_inline)) T *pop([[maybe_unused]] const int tid = 0)
    {
#ifdef CAUTIOUS_DEQUEUE //checks if the queue is empty before trying operations
        if (Base::isEmpty()) return nullptr;
#endif
        while (true)
        {
            Base::safeCluster();
            uint64_t headTicket = Base::head.fetch_add(1);
#ifndef DISABLE_POW2
            Cell &cell = array[headTicket & mask];
#else
            Cell &cell = array[headTicket % size];
#endif

            int r = 0;
            uint64_t tt = 0;

            while (true)
            {
                uint64_t cell_idx = cell.idx.load();
                uint64_t unsafe = nodeUnsafe(cell_idx);
                uint64_t idx = nodeIndex(cell_idx);
                T *val = static_cast<T *>(cell.val.load());

                if (idx > headTicket)
                    break;

                if (val != nullptr)
                { //
                    if (idx == headTicket)
                    {
                        if (CAS2((void **)&cell, val, cell_idx, nullptr, unsafe | (headTicket + size)))
                            return val;
                    }
                    else
                    { // Unsafe Transition
                        if (CAS2((void **)&cell, val, cell_idx, val, setUnsafe(idx)))
                            break;
                    }
                }
                else
                { // Void Transition
                    if ((r & ((1ull << 8)) - 1) == 0)
                        tt = Base::tail.load();

                    int closed = Base::isClosed(tt);
                    uint64_t t = Base::tailIndex(tt);
                    if (unsafe || t < headTicket + 1 || closed || r > 4 * 1024 )
                    {
                        if (CAS2((void **)&cell, val, cell_idx, val, unsafe | (headTicket + size)))
                            break;
                    }
                    ++r;
                }
            }

            if (Base::tailIndex(Base::tail.load()) <= headTicket)
            {
                Base::fixState();
                return nullptr; // coda vuota;
            }
        }
    }

    inline size_t length([[maybe_unused]] const int tid = 0) const {
        if constexpr (bounded){
            uint64_t length = Base::tail.load() - Base::head.load();
            return length < 0 ? 0 : length > size ? size : length;
        } else {
            return Base::length();
        }
    }

    friend class LinkedRingQueue<T,CRQueue<T,padded_cells,bounded>>;   //LinkedRingQueue can access private class members 
};

/*
    Declare aliases for Unbounded and Bounded Queues
*/
#ifndef DISABLE_PADDING
template<typename T,bool padded_cells=true,bool bounded=false>
#else
template<typename T,bool padded_cells=true,bool bounded=false>
#endif
using LCRQueue = LinkedRingQueue<T,CRQueue<T,padded_cells,bounded>>;

#ifndef DISABLE_PADDING
template<typename T,bool padded_cells=true,bool bounded=true>
#else
template<typename T,bool padded_cells=false,bool bounded=true>
#endif
using BoundedCRQueue = CRQueue<T,padded_cells,bounded>;