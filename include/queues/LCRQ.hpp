#pragma once

#include <atomic>
#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"
#include "x86Atomics.hpp"


template <typename T, bool padded_cells, bool bounded>
class CRQueue : public QueueSegmentBase<T, CRQueue<T, padded_cells, bounded>>
{
private:

    const size_t Ring_Size;

    using Base = QueueSegmentBase<T, CRQueue<T, padded_cells, bounded>>;
    using Cell = detail::CRQCell<T *, padded_cells>;

    Cell *array;

    inline uint64_t node_index(uint64_t i) const
    {
        return (i & ~(1ull << 63));
    }

    inline uint64_t set_unsafe(uint64_t i) const
    {
        return (i | (1ull << 63));
    }

    inline uint64_t node_unsafe(uint64_t i) const
    {
        return i & (1ull << 63);
    }

public:
    CRQueue(size_t Ring_Size=Base::RING_SIZE): Base(), Ring_Size{Ring_Size}
    {
        array = new Cell[Ring_Size];
        for (uint64_t i = 0; i < Ring_Size; i++)
        {
            uint64_t j = i % Ring_Size;
            array[j].val.store(nullptr, std::memory_order_relaxed);
            array[j].idx.store(i, std::memory_order_relaxed);
        }

        Base::head.store(0, std::memory_order_relaxed);
        Base::tail.store(0, std::memory_order_relaxed);
        
        Base::cluster.store(isNumaAvailable() ? getNumaNode() : 0, std::memory_order_relaxed);
    }

    //Constructor to ensure protability with LinkedRing Variants
    CRQueue(size_t threads, size_t Ring_Size): CRQueue(Ring_Size) {}

    ~CRQueue() {
        delete[] array;
    }

    static std::string className(bool padding = true)
    {
        using namespace std::string_literals;
        return (bounded? "Bounded"s : ""s ) + "CRQueue"s + ((padded_cells && padding)? "/padded" : "");
    }

    bool enqueue(T *item,[[maybe_unused]] const int tid)
    {
        int try_close = 0;

        while (true)
        {
            while(!Base::safeCluster(false));
            
            uint64_t tailTicket = Base::tail.fetch_add(1);

            if constexpr (bounded == false){
                if(Base::isClosed(tailTicket)){
                    return false;
                }
            }
            Cell &cell = array[tailTicket % Ring_Size];
            uint64_t idx = cell.idx.load();
            if (cell.val.load() == nullptr)
            {
                if (node_index(idx) <= tailTicket)
                {
                    if ((!node_unsafe(idx) || Base::head.load() < tailTicket))
                    {
                        if ((!node_unsafe(idx) || Base::head.load() < tailTicket))
                        {
                            if (CAS2((void **)&cell, nullptr, idx, item, tailTicket))
                                return true;
                        }
                    }
                }
            }

            if (tailTicket >= Base::head.load() + Ring_Size)
            {   
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

    T *dequeue([[maybe_unused]] const int tid)
    {
#ifdef CAUTIOUS_DEQUEUE
        if (Base::isEmpty())
            return nullptr;
#endif
        while (true)
        {
            while(!Base::safeCluster(false)); //

            uint64_t headTicket = Base::head.fetch_add(1);
            Cell &cell = array[headTicket % Ring_Size];

            int r = 0;
            uint64_t tt = 0;

            while (true)
            {
                uint64_t cell_idx = cell.idx.load();
                uint64_t unsafe = node_unsafe(cell_idx);
                uint64_t idx = node_index(cell_idx);
                T *val = static_cast<T *>(cell.val.load());

                if (idx > headTicket)
                    break;

                if (val != nullptr)
                { // Transizione
                    if (idx == headTicket)
                    {
                        if (CAS2((void **)&cell, val, cell_idx, nullptr, unsafe | (headTicket + Ring_Size)))
                            return val;
                    }
                    else
                    { // Unsafe Transition
                        if (CAS2((void **)&cell, val, cell_idx, val, set_unsafe(idx)))
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
                        if (CAS2((void **)&cell, val, cell_idx, val, unsafe | (headTicket + Ring_Size)))
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

    inline size_t size() const {
        if constexpr (bounded){
            uint64_t length = Base::tail.load() - Base::head.load();
            return length < 0 ? 0 : length > Ring_Size ? Ring_Size : length;
        } else {
            return Base::size();
        }
    }

    friend class LinkedRingQueue<T,CRQueue<T,padded_cells,bounded>>;   
};


#ifndef NO_PADDING
template<typename T,bool padded_cells=true,bool bounded=false>
#else
template<typename T,bool padded_cells=true,bool bounded=false>
#endif
using LCRQueue = LinkedRingQueue<T,CRQueue<T,padded_cells,bounded>>;

#ifndef NO_PADDING
template<typename T,bool padded_cells=true,bool bounded=true>
#else
template<typename T,bool padded_cells=false,bool bounded=true>
#endif
using BoundedCRQueue = CRQueue<T,padded_cells,bounded>;