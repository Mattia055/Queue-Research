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

using namespace std;

template<class T, class Segment>
class LinkedRingQueue{
private:
    static constexpr size_t MAX_THREADS     = 128;
    static constexpr size_t RING_SIZE       = 128; //setta la lunghezza per i segmenti
    static constexpr int kHpTail = 0;
    static constexpr int kHpHead = 1;
    const int maxThreads;

    alignas(CACHE_LINE) std::atomic<Segment*> head;
    alignas(CACHE_LINE) std::atomic<Segment*> tail;
    
    HazardPointers<Segment> HP;

    //Deprecated function
    inline T* dequeueAfterNextLinked(Segment* lhead, int tid) {
        return lhead->dequeue(tid);
    }

public:

    size_t Ring_Size; //variabile d'istanza

    LinkedRingQueue(int maxThreads=MAX_THREADS, size_t Segment_Length=RING_SIZE):
    Ring_Size{Segment_Length},
    maxThreads{maxThreads},
    HP(2,maxThreads)
    {
#ifndef DISABLE_HAZARD
    assert(maxThreads <= HazardPointers<Segment*>::MAX_THREADS); //assertion to assure no SIGSEGV
#endif
        Segment* sentinel = new Segment(Segment_Length);
        head.store(sentinel,std::memory_order_relaxed);
        tail.store(sentinel,std::memory_order_relaxed);

    }

    ~LinkedRingQueue() {
        while(dequeue(0) != nullptr); //Svuota la coda
        delete head.load(); //elimina il primo Segment;
    }

    static std::string className(bool padding = true){
        return "Linked" + Segment::className(padding);
    }

    __attribute__((used,always_inline)) void enqueue(T* item, int tid){
        if(item == nullptr)
            throw std::invalid_argument("item cannot be null pointer");
        
        Segment *ltail = HP.protect(kHpTail,tail.load(),tid);
        while(1) {

#ifndef DISABLE_HAZARD //Se Hazard Pointers abilitati
            Segment *ltail2 = tail.load();
            if(ltail2 != ltail){
                ltail = HP.protect(kHpTail,ltail2,tid); //cambia la coda
                continue;
            }
#endif  
            Segment *lnext = ltail->next.load();
            if(lnext != nullptr) { //se esiste una nuova coda
                if(tail.compare_exchange_strong(ltail, lnext))
                    ltail = HP.protect(kHpTail, lnext,tid); //aggiorna il puntatore coda protetto dal thread
                else
                    ltail = HP.protect(kHpTail,tail.load(),tid); //qualcun altro thread ha cambiato la coda [aggiorno il puntatore]
                
                continue; //provo l'inserimento nella nuova coda
            }

            if(ltail->enqueue(item,tid)) {
                HP.clear(kHpTail,tid); //se l'inserimento ha successo elimino il puntatore dal thread;
                break;
            }

            //se l'inserimento non ha successo allora il Segmento è pieno, ne serve un altro
            Segment* newTail = new Segment(Ring_Size);
            newTail->setStartIndex(ltail->getNextSegmentStartIndex());
            newTail->enqueue(item,tid);

            Segment* nullSegment = nullptr;
            if(ltail->next.compare_exchange_strong(nullSegment,newTail)){ //se scambio il segmento modifico anche la nuova coda
                tail.compare_exchange_strong(ltail,newTail);
                HP.clear(kHpTail,tid); //elimino il puntatore alla vecchia coda dall'Hazard vector;
                break;
            } 
            else 
                delete newTail;

            ltail = HP.protect(kHpTail,nullSegment,tid);

        }
    }

    __attribute__((used,always_inline)) T* dequeue(int tid) {
        Segment* lhead = HP.protect(kHpHead,head.load(),tid);
        while(true){
#ifndef DISABLE_HAZARD
            Segment *lhead2 = head.load();
            if(lhead2 != lhead){
                lhead = HP.protect(kHpHead,lhead2,tid);
                continue;
            }           
#endif
            T* item = lhead->dequeue(tid); //dequeue dal segmento corrente
            if (item == nullptr) {
                Segment* lnext = lhead->next.load(); //carica il next
                if (lnext != nullptr) { //se next == nullptr allora non c'è nulla da estrarre
                    item = lhead->dequeue(tid); //DequeueAfterNextLinked(lnext)
                    if (item == nullptr) {
                        if (head.compare_exchange_strong(lhead, lnext)) {
                            HP.retire(lhead, tid); //elimino il vecchio puntatore a testa
                            lhead = HP.protect(kHpHead, lnext, tid); //proteggo il nuovo puntatore a testa
                        } else {
                            lhead = HP.protect(kHpHead, lhead, tid);
                        }
                        continue;
                    }
                }
            }

            HP.clear(kHpHead,tid); //dopo l'estrazione elimino dalla tabella del thread il puntatore alla testa corrente
            return item;
        }
    }

    size_t size(int tid) {
        Segment *lhead = HP.protect(kHpHead,head,tid);
        Segment *ltail = HP.protect(kHpTail,tail,tid);
        uint64_t t = ltail->getTailIndex();
        uint64_t h = lhead->getHeadIndex();
        HP.clear(tid);
        return t > h ? t - h : 0;
    }

};

template <class T,class Segment>
struct QueueSegmentBase {
protected:

    static constexpr size_t     RING_SIZE = 128;
    static constexpr size_t     TRY_CLOSE = 10;

    alignas(CACHE_LINE) std::atomic<uint64_t> head{0};
    alignas(CACHE_LINE) std::atomic<uint64_t> tail{0};
    alignas(CACHE_LINE) std::atomic<Segment*> next{nullptr};
    alignas(CACHE_LINE) std::atomic<uint64_t> cluster{};   //most operations should be on the same cluster

    static inline uint64_t tailIndex(uint64_t t) {
        return (t & ~(1ull << 63));
    }

    static inline bool isClosed(uint64_t t) {
        return (t & (1ull<<63)) != 0;
    }

    inline void setStartIndex(uint64_t i){
        head.store(i,std::memory_order_relaxed);
        tail.store(i,std::memory_order_relaxed); 
    }

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

    size_t size() const {
        long size = tail.load() - head.load();
        return size > 0 ? size : 0;
    }

    inline bool closeSegment(const uint64_t tailTicket, bool force) {
        if(!force) {
            uint64_t tmp = tailTicket + 1;
            return tail.compare_exchange_strong(tmp,(tailTicket + 1) | (1ull << 63));
        } else 
            return BIT_TEST_AND_SET63(&tail);
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

#ifndef DISABLE_NUMA
    __attribute__((used,always_inline)) bool safeCluster(){
        do{
            uint64_t c = cluster.load();
            if(c != getNumaNode()){
                this_thread::sleep_for(std::chrono::microseconds(CLUSTER_TIMEOUT));
                if(!cluster.compare_exchange_strong(c,getNumaNode()))
                    continue;
            } 
            return true;

        }while(true);
    }
#else
    inline bool safeCluster(){return true;}
#endif


public:
    friend class LinkedRingQueue<T,Segment>;   
};