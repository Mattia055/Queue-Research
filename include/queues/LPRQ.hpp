#pragma once

#include <atomic>
#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"

template<typename T,bool padded_cells>
class PRQueue : public QueueSegmentBase<T, PRQueue<T,padded_cells>> {
private:
    using Base = QueueSegmentBase<T,PRQueue<T,padded_cells>>;
    using Cell = detail::CRQCell<void*,padded_cells>;
    
    Cell* array; 

    static constexpr size_t RING_SIZE = 128;
    static constexpr size_t TRY_CLOSE = 10;

    const size_t Ring_Size;

    inline uint64_t nodeIndex(uint64_t i) const {
        return (i & ~(1ull << 63));
    }

    inline uint64_t setUnsafe(uint64_t i) const {
        return (i | (1ull << 63));
    }

    inline uint64_t nodeUnsafe(uint64_t i) const {
        return (i & (1ull << 63));
    }

    inline bool isBottom(void* const value) const {
        return (reinterpret_cast<uintptr_t>(value) & 1) != 0;
    }

    inline void* threadLocalBottom(const int tid) const {
        return reinterpret_cast<void*>(static_cast<uintptr_t>((tid << 1) | 1));
    }

public:
    PRQueue(uint64_t start, size_t Ring_Size = RING_SIZE):
    Base(),
    Ring_Size{Ring_Size}
    {
        array = new Cell[Ring_Size];
        for(uint64_t i = start; i < start + Ring_Size; i++) { 
            uint64_t j = i % Ring_Size;
            array[j].val.store(nullptr,std::memory_order_relaxed);
            array[j].idx.store(i,std::memory_order_relaxed);
        }

        Base::head.store(start,std::memory_order_relaxed);
        Base::tail.store(start,std::memory_order_relaxed);
    }
    ~PRQueue(){
        delete[] array;
    }

    static std::string className() {
        using namespace std::string_literals;
        return "PRQueue"s + (padded_cells ? "/padded":"");
    }

    bool enqueue(T* item,[[maybe_unused]] const int tid) {
        int try_close = 0;
    
        while(true) {
            uint64_t tailTicket = Base::tail.fetch_add(1);
            if(Base::isClosed(tailTicket)) {
                return false;
            }

            Cell& cell = array[tailTicket % Ring_Size];
            uint64_t idx = cell.idx.load();
            void* val = cell.val.load();

            if( val == nullptr
                && nodeIndex(idx) <= tailTicket
                && (!nodeUnsafe(idx) || Base::head.load() <= tailTicket)) 
            {
                void* bottom = threadLocalBottom(tid);
                if(cell.val.compare_exchange_strong(val,bottom)) {
                    if(cell.idx.compare_exchange_strong(idx,tailTicket + Ring_Size)) {
                        if(cell.val.compare_exchange_strong(bottom, item)) {
                            return true;
                        }
                    } else {
                        cell.val.compare_exchange_strong(bottom, nullptr);
                    }
                }
            }

            if(tailTicket >= Base::head.load() + Ring_Size){
                if(Base::closeSegment(tailTicket, ++try_close > TRY_CLOSE))
                    return false;
            }  

        }
    }

    T* dequeue([[maybe_unused]] const int tid) {
#ifdef CAUTIOUS_DEQUEUE
        if(Base::isEmpty())
            return nullptr;
#endif
        while(1) {
            uint64_t headTicket = Base::head.fetch_add(1);
            Cell& cell = array[headTicket % Ring_Size];

            int r = 0;
            uint64_t tt = 0;

            while(1) {
                uint64_t cell_idx   = cell.idx.load();
                uint64_t unsafe     = nodeUnsafe(cell_idx);
                uint64_t idx        = nodeIndex(cell_idx);

                void* val           = cell.val.load();

                if(val != nullptr && !isBottom(val)){
                    if(idx == headTicket + Ring_Size){
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

                    int closed = Base::isClosed(tt);
                    uint64_t t = Base::tailIndex(tt);
                    if(unsafe || t < headTicket + 1 || closed || r > 4 * 1024) {
                        if(isBottom(val) && !cell.val.compare_exchange_strong(val,nullptr))
                            continue;
                        if(cell.idx.compare_exchange_strong(cell_idx, unsafe | (headTicket + Ring_Size)))
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
  
};

#ifndef NO_PADDING
template<typename T, bool padded_cells=true>
#else
template<typename T, bool padded_cells=false>
#endif
using LPRQueue = LinkedRingQueue<T,PRQueue<T,padded_cells>>;