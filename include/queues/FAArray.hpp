#pragma once
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstddef>  //for alignas
#include "RQCell.hpp"
#include "HazardPointers.hpp"

#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif

template<typename T, bool padded_cells>
class FAAArrayQueue {
private:

    struct Node;
    using Cell = detail::PlainCell<T*,padded_cells>;
    const size_t maxThreads;

    HazardPointers<Node> HP;
    const int kHpTail = 0;
    const int kHpHead = 1;
    const size_t size;

    alignas(CACHE_LINE) std::atomic<Node*> head;
    alignas(CACHE_LINE) std::atomic<Node*> tail;
    T* taken = (T*)new int(); //alloca un puntatore a intero e lo casta a T

    struct Node {

        alignas(CACHE_LINE) std::atomic<int>    deqidx;
        alignas(CACHE_LINE) std::atomic<int>    enqidx;
        alignas(CACHE_LINE) std::atomic<Node*>  next;
        Cell *items;
        const uint64_t startIndexOffset;

        //Inizia con la prima entry prefilled e enqidx a 1
        Node(T* item, uint64_t startIndexOffset,size_t Buffer_Size=128):
        deqidx{0}, enqidx{1},
        next{nullptr}, startIndexOffset(startIndexOffset)
        {
            items = new Cell[Buffer_Size];
            std::memset(items,0,sizeof(Cell) * Buffer_Size);
            items[0].val.store(item,std::memory_order_relaxed);
        }

        ~Node(){
            delete[] items;
        }

        inline bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp,val);
        }
    };

    inline bool casTail(Node *cmp, Node *val) {
        return tail.compare_exchange_strong(cmp,val);
    }

    inline bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp,val);
    }

public:
    FAAArrayQueue(size_t Buffer_Size, size_t maxThreads):
    maxThreads{maxThreads},size{Buffer_Size},
    HP(2,maxThreads)
    {
        assert(Buffer_Size > 0);
        Node* sentinelNode = new Node(nullptr,0,Buffer_Size);
        sentinelNode->enqidx.store(0,std::memory_order_relaxed);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }

    ~FAAArrayQueue() {
        while(pop(0) != nullptr);
        delete head.load(); //elimina l'ultimo nodo
        delete (int*)taken;
    }

    static std::string className(bool padding = true) {
        using namespace std::string_literals;
        return "FAAArrayQueue"s + ((padded_cells && padding)? "/padded" : "");
    }

    size_t length(int tid) {
        Node* lhead = HP.protect(kHpHead,head,tid);
        Node* ltail = HP.protect(kHpTail,tail,tid);

        uint64_t t = std::min((uint64_t) size, ((uint64_t) ltail->enqidx.load())) + ltail->startIndexOffset;
        uint64_t h = std::min((uint64_t) size, ((uint64_t) lhead->deqidx.load())) + lhead->startIndexOffset;
        HP.clear(tid);
        return t > h ? t - h : 0;
    }

    __attribute__((used,always_inline)) void push(T* item, const int tid) {
        if(item == nullptr)
            throw std::invalid_argument("item cannot be null pointer");

        while(1){
            Node *ltail = HP.protect(kHpTail,tail,tid);
            const int idx = ltail->enqidx.fetch_add(1);
            if(idx >(size-1)) { 
                if(ltail != tail.load()) continue;
                Node* lnext = ltail->next.load(); 
                if(lnext == nullptr) {
                    Node* newNode = new Node(item,ltail->startIndexOffset + size, size);
                    if(ltail->casNext(nullptr,newNode)) {
                        casTail(ltail, newNode);
                        HP.clear(kHpTail,tid);
                        return;
                    }
                    delete newNode;
                } else {
                    casTail(ltail,lnext);
                }
                continue;
            }

            T* itemNull = nullptr;
            if(ltail->items[idx].val.compare_exchange_strong(itemNull,item)) {
                HP.clear(kHpTail,tid);
                return;
            }
        }
    }

    __attribute__((used,always_inline)) T* pop(const int tid) {
        T* item = nullptr;
        Node* lhead = HP.protect(kHpHead, head, tid);

#ifdef CAUTIOUS_DEQUEUE
        if (lhead->deqidx.load() >= lhead->enqidx.load() && lhead->next.load() == nullptr)
            return nullptr;
#endif

        while (true) {
            const int idx = lhead->deqidx.fetch_add(1);
            if (idx > size-1) { // This node has been drained, check if there is another one
                Node* lnext = lhead->next.load();
                if (lnext == nullptr) {
                    break;  // No more nodes in the queue
                }
                if (casHead(lhead, lnext))
                    HP.retire(lhead, tid);

                lhead = HP.protect(kHpHead, head, tid);
                continue;
            }
            Cell& cell = lhead->items[idx];
            if (cell.val.load() == nullptr && idx < lhead->enqidx.load()) {
                for (size_t i = 0; i < 4*1024; ++i) {
                    if (cell.val.load() != nullptr)
                        break;
                }
            }
            item = cell.val.exchange(taken);
            if (item != nullptr)
                break;

            int t = lhead->enqidx.load();
            if (idx + 1 >= t) {
                if (lhead->next.load() != nullptr)
                    continue;
                lhead->enqidx.compare_exchange_strong(t, idx + 1);
                break;
            }
        }
        HP.clear(kHpHead, tid);
        return item;
    }

};

// Type alias for FAAQueue
#ifndef NO_PADDING
template<typename T, bool padding=true>
#else
template<typename T, bool padding=false>
#endif
using FAAQueue = FAAArrayQueue<T,padding>;
