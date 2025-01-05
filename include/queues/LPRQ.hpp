#pragma once

#include <atomic>
#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"

/*
    Macros:
    DISABLE_PADDING: disables padding for cells
    DISABLE_POW2: Disables power of 2 modulo ops
    CAUTIOUS_DEQUEUE: check that the queue is empty before attempt pop
*/

template<typename T,bool padded_cells, bool bounded>
class PRQueue : public QueueSegmentBase<T, PRQueue<T,padded_cells,bounded>> {
private:
    using Base = QueueSegmentBase<T,PRQueue<T,padded_cells,bounded>>;
    using Cell = detail::CRQCell<void*,padded_cells>;

    static constexpr size_t TRY_CLOSE = 10;
    
    Cell* array; 
    const size_t size;
#ifndef DISABLE_POW2
    const size_t mask;  //Mask to execute the modulo operation
#endif

    /* Private class methods */
    inline uint64_t nodeIndex(uint64_t i) const {return (i & ~(1ull << 63));}
    inline uint64_t setUnsafe(uint64_t i) const {return (i | (1ull << 63));}
    inline uint64_t nodeUnsafe(uint64_t i) const {return (i & (1ull << 63));}
    inline bool isBottom(void* const value) const {return (reinterpret_cast<uintptr_t>(value) & 1) != 0;}
    inline void* threadLocalBottom(const int tid) const {
        return reinterpret_cast<void*>(static_cast<uintptr_t>((tid << 1) | 1));
    }

private:
    //uses the tid argument to be consistent with linked queues
    PRQueue(size_t size_par, [[maybe_unused]] const int tid, const uint64_t start): Base(),
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
    PRQueue(size_t size_par, [[maybe_unused]] const int tid = 0): PRQueue(size_par,tid,0){}

    ~PRQueue(){
        while(pop(0) != nullptr);
        delete[] array;
    }

    static std::string className(bool padding = true) {
        using namespace std::string_literals;
        return (bounded? "Bounded"s : ""s ) + "PRQueue"s + ((padded_cells && padding)? "/padded":"");
    }

    /*
        Takes an additional tid parameter to keep the interface compatible with
        LinkedRingQueue->push
        The parameter has a default value so that it can be omitted
    */
    __attribute__((used,always_inline)) bool push(T* item,[[maybe_unused]] const int tid = 0) {
        int try_close = 0;
    
        while(true) {

            Base::safeCluster();

            uint64_t tailTicket = Base::tail.fetch_add(1);
            
            if constexpr (bounded == false){
                if(Base::isClosed(tailTicket)) {
                    return false;
                }
            }
#ifndef DISABLE_POW2
            Cell& cell = array[tailTicket & mask];
#else
            Cell& cell = array[tailTicket % size];
#endif
            uint64_t idx = cell.idx.load();
            void* val = cell.val.load();

            if( val == nullptr
                && nodeIndex(idx) <= tailTicket
                && (!nodeUnsafe(idx) || Base::head.load() <= tailTicket)) 
            {
                void* bottom = threadLocalBottom(tid);
                if(cell.val.compare_exchange_strong(val,bottom)) {
                    if(cell.idx.compare_exchange_strong(idx,tailTicket + size)) {
                        if(cell.val.compare_exchange_strong(bottom, item)) {
                            return true;
                        }
                    } else {
                        cell.val.compare_exchange_strong(bottom, nullptr);
                    }
                }
            }

            if(tailTicket >= Base::head.load() + size){
                if constexpr (bounded){
                    return false;
                }
                else{
                    if (Base::closeSegment(tailTicket, ++try_close > TRY_CLOSE))
                        return false;
                }
            }  

        }
    }

    /*
        Takes an additional tid parameter to keep the interface compatible with
        LinkedRingQueue->push
        The parameter has a default value so that it can be omitted
    */
    __attribute__((used,always_inline)) T* pop([[maybe_unused]] const int tid = 0) {
#ifdef CAUTIOUS_DEQUEUE
        if(Base::isEmpty())
            return nullptr;
#endif
        while(true) {

            Base::safeCluster();

            uint64_t headTicket = Base::head.fetch_add(1);
#ifndef DISABLE_POW2
            Cell& cell = array[headTicket & mask];
#else
            Cell& cell = array[headTicket % size];
#endif

            int r = 0;
            uint64_t tt = 0;

            while(1) {
                uint64_t cell_idx   = cell.idx.load();
                uint64_t unsafe     = nodeUnsafe(cell_idx);
                uint64_t idx        = nodeIndex(cell_idx);

                void* val           = cell.val.load();

                if(val != nullptr && !isBottom(val)){
                    if(idx == headTicket + size){
                        cell.val.store(nullptr);
                        return static_cast<T*>(val);
                    } else {
                        if(unsafe) {
                            if(cell.idx.load() == cell_idx)
                                break;
                        } else {
                            if(cell.idx.compare_exchange_strong(cell_idx,setUnsafe(idx)))
                                break;
                        }
                    }
                } else {
                    if((r & ((1ull << 8 ) -1 )) == 0)
                        tt = Base::tail.load();

                    int closed = Base::isClosed(tt);    //in case "bounded" it's always false
                    uint64_t t = Base::tailIndex(tt);
                    if(unsafe || t < headTicket + 1  || r > 4 * 1024 || closed) {
                        if(isBottom(val) && !cell.val.compare_exchange_strong(val,nullptr))
                            continue;
                        if(cell.idx.compare_exchange_strong(cell_idx, unsafe | (headTicket + size)))
                            break;
                    }
                    ++r;
                }
            }

            if(Base::tailIndex(Base::tail.load()) <= headTicket + 1){
                Base::fixState();
                return nullptr;
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

public: 
    friend class LinkedRingQueue<T,PRQueue<T,padded_cells,bounded>>;   
  
};

/*
    Declare aliases for Unbounded and Bounded Queues
*/
#ifndef NO_PADDING
template<typename T,bool padded_cells=true,bool bounded=false>
#else
template<typename T,bool padded_cells=true,bool bounded=false>
#endif
using LPRQueue = LinkedRingQueue<T,PRQueue<T,true,false>>;

#ifndef NO_PADDING
template<typename T,bool padded_cells=true,bool bounded=true>
using BoundedPRQueue = PRQueue<T,true,true>;
#else
template<typename T,bool padded_cells=false,bool bounded=true>
using BoundedPRQueue = PRQueue<T,false,true>;
#endif
