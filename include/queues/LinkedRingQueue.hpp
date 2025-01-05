#pragma once
#include "x86Atomics.hpp"
#include "HazardPointers.hpp"
#include <stdexcept>
#include <cstddef>  // For alignas
#include <cassert>
#include <atomic>
#include <chrono>   //For NUMA Timeout
#include "numa_support.hpp"

#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif

#ifndef CLUSTER_TIMEOUT //microseconds
#define CLUSTER_TIMEOUT 100
#endif

template<class T, class Segment>
class LinkedRingQueue{
private:
    static constexpr size_t MAX_THREADS = HazardPointers<Segment*>::MAX_THREADS;
    static constexpr int kHpTail = 0;
    static constexpr int kHpHead = 1;
    const size_t maxThreads;
    const size_t size;

    alignas(CACHE_LINE) std::atomic<Segment*> head;
    alignas(CACHE_LINE) std::atomic<Segment*> tail;
    
    HazardPointers<Segment> HP; //Hazard Pointer matrix to ensure no memory leaks on concurrent allocations and deletions

    //Deprecated function
    inline T* dequeueAfterNextLinked(Segment* lhead, int tid) {
        return lhead->pop(tid);
    }

public:

    LinkedRingQueue(size_t SegmentLength, size_t threads = MAX_THREADS):
    size{SegmentLength},
    maxThreads{threads},
    HP(2,maxThreads)    
    {
#ifndef DISABLE_HAZARD
    assert(maxThreads <= MAX_THREADS); //assertion to assure no SIGSEGV when accessing the HP matrix
#endif
        Segment* sentinel = new Segment(SegmentLength);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    /*
        When the queue goes out of scope:
        1. empty the current segment (ensures no memory leaks on elements)
        2. delete the current segment

        The destructor cascades on the next segments
    */
    ~LinkedRingQueue() {
        while(pop(0) != nullptr);
        delete head.load();
    }

    static string className(bool padding = true){
        return "Linked" + Segment::className(padding);
    }

    /*
        pushes a new element into the queue. The operation always succeds
        (if current segment is full then allocates another one)

        Note: It uses Hazard Pointer to protect access to segments (ensures no memory leaks on concurrent eliminations)
    */
    __attribute__((used,always_inline)) void push(T* item, int tid){
        if(item == nullptr)
            throw invalid_argument(className(false) + "ERROR push(): item cannot be null");
        
        Segment *ltail = HP.protect(kHpTail,tail.load(),tid);
        while(true) {
#ifndef DISABLE_HAZARD
            Segment *ltail2 = tail.load();
            if(ltail2 != ltail){
                ltail = HP.protect(kHpTail,ltail2,tid); //if current segment has been updated then changes
                continue;
            }
#endif  
            Segment *lnext = ltail->next.load();
            if(lnext != nullptr) { //If a new segment exists
                tail.compare_exchange_strong(ltail, lnext)?
                    ltail = HP.protect(kHpTail, lnext,tid) //update protection on the new Segment
                : 
                    ltail = HP.protect(kHpTail,tail.load(),tid); //someone else already updated the shared queeu
                continue; //try push on the new segment
            }

            if(ltail->push(item,tid)) {
                HP.clear(kHpTail,tid); //if succesful insertion then exits updating the HP matrix
                break;
            }

            //if failed insertion then current segment is full (allocate a new one)
            Segment* newTail = new Segment(size,0,ltail->getTailIndex());
            newTail->push(item,tid);

            Segment* nullSegment = nullptr;
            if(ltail->next.compare_exchange_strong(nullSegment,newTail)){ //if CAS succesful then the queue has ben updated
                tail.compare_exchange_strong(ltail,newTail);
                HP.clear(kHpTail,tid); //clear protection on the tail
                break;
            } 
            else 
                delete newTail; //delete the segment since the modification has been unsuccesful

            ltail = HP.protect(kHpTail,nullSegment,tid);    //update protection on hte current new segment
        }
    }

    /*
    Pop operation tries to dequeue from the linked queue, from the current segment. If segment is empty
    then tries to load the next segment. Fails if pop unsuccesful and no next segment.

    NOTE:   uses protect / clear (Hazard Pointers operation) to ensure no other thread deallocates the
            segment while another thread is using it (prevents memory leaks and Segmentation Faults)

    return: pointer to next element or nullptr
     */
    __attribute__((used,always_inline)) T* pop(int tid) {
        Segment* lhead = HP.protect(kHpHead,head.load(),tid);   //protect the current segment
        while(true){
#ifndef DISABLE_HAZARD
            Segment *lhead2 = head.load();
            if(lhead2 != lhead){
                lhead = HP.protect(kHpHead,lhead2,tid);
                continue;
            }           
#endif
            T* item = lhead->pop(tid); //pop on the current segment
            if (item == nullptr) {
                Segment* lnext = lhead->next.load(); //if unsuccesful pop then try to load next semgnet
                if (lnext != nullptr) { //if next segments exist
                    item = lhead->pop(tid); //DequeueAfterNextLinked(lnext)
                    if (item == nullptr) {
                        if (head.compare_exchange_strong(lhead, lnext)) {   //changes shared head pointer
                            HP.retire(lhead, tid); //tries to deallocate current segment
                            lhead = HP.protect(kHpHead, lnext, tid); //protect new segment
                        } else {
                            lhead = HP.protect(kHpHead, lhead, tid);
                        }
                        continue;
                    }
                }
            }

            HP.clear(kHpHead,tid); //after pop removes protection on the current segment
            return item;
        }
    }

    /*
        Returns current size of the queue
    */
    size_t length(int tid) {
        Segment *lhead = HP.protect(kHpHead,head,tid);
        Segment *ltail = HP.protect(kHpTail,tail,tid);
        uint64_t t = ltail->getTailIndex();
        uint64_t h = lhead->getHeadIndex();
        HP.clear(tid);
        return t > h ? t - h : 0;
    }

};

/**
 * Superclass for queue segments
 */
template <class T,class Segment>
struct QueueSegmentBase {
protected:

    alignas(CACHE_LINE) std::atomic<uint64_t> head{0};
    alignas(CACHE_LINE) std::atomic<uint64_t> tail{0};
    alignas(CACHE_LINE) std::atomic<Segment*> next{nullptr};
    alignas(CACHE_LINE) std::atomic<uint64_t> cluster{};   //most operations should be on the same cluster

    /*
        The tail is:
        MSB: closed bit
        63 LSB: tail index
    */

    //Returns the index given the tail
    static inline uint64_t tailIndex(uint64_t tail) {return (tail & ~(1ull << 63));}

    //returns the current state of the queue
    static inline bool isClosed(uint64_t tail) {return (tail & (1ull<<63)) != 0;}

    /*
    Sets the start index when allocating new segments (uses the index of the last segment - or 0)
    If the startIndex isn't set correctly then the queue length becomes irrelevant
    */
    inline void setStartIndex(uint64_t i){
        head.store(i,std::memory_order_relaxed);
        tail.store(i,std::memory_order_relaxed); 
    }

    /*
    push and pop operation can make the indexes inconsistent (head greater than tail)
    */
    void fixState() {
        while(true) {
            uint64_t t = tail.load();
            uint64_t h = head.load();
            if(tail.load() != t) continue;
            if(h>t) {
                uint64_t tmp = t;
                if(tail.compare_exchange_strong(tmp,h))
                    break;
                continue;
            }
            break;
        }
    }

    //returns the current size of the queue considering all segments
    size_t length() const {
        long size = tail.load() - head.load();
        return size > 0 ? size : 0;
    }

    /*
    Uses CAS to try to close the queue, if "force" uses asm TAS on the MSB of the tail
    */
    inline bool closeSegment(const uint64_t tailTicket, bool force) {
        if(!force) {
            uint64_t tmp = tailTicket +1;
            return tail.compare_exchange_strong(tmp,(tailTicket + 1) | (1ull << 63));
        } else {
            return BIT_TEST_AND_SET63(&(tail));
        }
    }

    inline bool isEmpty() const {
        return head.load() >= tailIndex(tail.load());
    }

    inline uint64_t getHeadIndex() const { 
        return head.load(); 
    }

    inline uint64_t getTailIndex() const {
        return tailIndex(tail.load());
    }

    inline uint64_t getNextSegmentStartIndex() const { 
        return getTailIndex() - 1;
    }

    /*
    DEBUG: numa optimization to keep most operations in-cluster
    */
#ifndef DISABLE_NUMA
    __attribute__((used,always_inline)) bool safeCluster(){
        while(true){
            uint64_t c = cluster.load();
            if(c != getNumaNode()){
                this_thread::sleep_for(std::chrono::microseconds(CLUSTER_TIMEOUT));
                if(cluster.compare_exchange_strong(c,getNumaNode()))
                    return true;
            }
        }
    }
#else
    inline bool safeCluster(){return true;}
#endif


public:
    friend class LinkedRingQueue<T,Segment>;   
};