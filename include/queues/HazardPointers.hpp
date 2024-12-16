#pragma once

#include <atomic>
#include <iostream>
#include <vector>


using namespace std;

#ifndef DISABLE_HAZARD

// #ifndef STATIC_HAZARD
// #define STATIC_HAZARD //Dynamic Hazard doesn't work
// #endif
#ifndef STATIC_HAZARD
#define STATIC_HAZARD
#endif

#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif

template<typename T>
class HazardPointers {
public:
    static const int MAX_THREADS        = 200; 
private:
    static const int MAX_HP_PER_THREAD  = 11;
    static const int CLPAD     = CACHE_LINE / sizeof(std::atomic<T*>);
    static const int THRESHOLD_R        = 0;
    static const int MAX_RETIRED        = MAX_THREADS * MAX_HP_PER_THREAD;

    const int maxHPs;
    const int maxThreads;

    //moltiplica per CACHE_LINE (molte celle vuote) ma no false sharing
#ifdef STATIC_HAZARD
    std::atomic<T*> Hazard [MAX_THREADS * CLPAD][MAX_HP_PER_THREAD];
    std::vector<T*> Retired[MAX_THREADS * CLPAD];
#else
    std::atomic<T*> **Hazard;
    std::vector<T*> *Retired;
#endif

public:
    //constructor
    HazardPointers(int maxHPs=MAX_HP_PER_THREAD, int maxThreads=MAX_THREADS):
    maxHPs{maxHPs},
    maxThreads{maxThreads}
    {

    

#ifndef STATIC_HAZARD //Inizializza la memoria dinamica
    Hazard      = new std::atomic<T*>*  [maxThreads * CLPAD];
    Retired     = new std::vector<T*>   [maxThreads * CLPAD]; 
    for(int i = 0; i< maxThreads; i++){
        Hazard[i*CLPAD] = new std::atomic<T*>[maxHPs];
    }

    std::cout << "Dynamic" << std::endl;
#endif

        for(int iThread = 0; iThread < maxThreads; iThread++){
            for(int iHP = 0; iHP < maxHPs; iHP++){
                Hazard[iThread*CLPAD][iHP].store(nullptr,std::memory_order_relaxed);
            }
        }
    }

    //destructor
    ~HazardPointers() 
    {
#ifndef STATIC_HAZARD
        for(int i = 0; i< maxThreads; ++i){
            delete[] Hazard[i*CLPAD];
        }
        delete[] Hazard;

        for(int iThread = 0; iThread < maxThreads; iThread++){
            for(unsigned iRet = 0; iRet<Retired[iThread*CLPAD].size();iRet++){
                delete Retired[iThread * CLPAD][iRet];
            }
        }
        delete[] Retired;
#else
        for(int iThread = 0; iThread < MAX_THREADS; iThread++){
            for(unsigned iRet = 0; iRet < Retired[iThread*CLPAD].size(); iRet++){
                delete Retired[iThread * CLPAD][iRet];
            }
        }
#endif
    }

    /**
     * METHODS
     */

    void clear(const int tid){
        for(int iHP = 0; iHP < maxHPs; iHP++){
            Hazard[tid * CLPAD][iHP].store(nullptr,std::memory_order_release);
        }
    }

    void clear(const int iHP, const int tid){
        Hazard[tid * CLPAD][iHP].store(nullptr,std::memory_order_release);
    }

    //Atomic pointers
    T* protect(const int index, const std::atomic<T*>& atom, const int tid){
        T* n = nullptr;
        T* ret;

        while((ret = atom.load()) != n){
            Hazard[tid * CLPAD][index].store(ret);
            n = ret;
        }
        return ret;
    }

    T* protect(const int index, T* ptr, const int tid){
        //cout << "I: "<<tid*CLPAD<< "J: " <<index << endl;
        Hazard[tid * CLPAD][index].store(ptr);
        return ptr;
    }

    T* protectRelease(const int index, const T* ptr, const int tid){
        Hazard[tid*CLPAD][index].store(ptr,std::memory_order_release);
        return ptr;
    }

    void retire(T* ptr, const int tid) 
    {
        //cout << "TID "<< tid << "\nPre Size: " << Retired[tid*CLPAD].size() << endl;
        Retired[tid*CLPAD].push_back(ptr); //push;
        if(Retired[tid*CLPAD].size() < THRESHOLD_R) //elimina dopo che supera il threshold
            return;

        //cerca di deallocare ogni puntatore nella retiredList
        for(unsigned iRet = 0; iRet < Retired[tid * CLPAD].size();){
            auto obj = Retired[tid*CLPAD][iRet];

            bool canDelete = true;
            //loop in tutta la matrice
            for(int tid = 0; tid < maxThreads && canDelete; tid++){
                for(int iHP = maxHPs -1; iHP >= 0; iHP--){
                    if(Hazard[tid*CLPAD][iHP].load() == obj){
                        canDelete = false;
                        break;
                    }
                }
            }

            if(canDelete){
                Retired[tid*CLPAD].erase(Retired[tid*CLPAD].begin() + iRet);
                delete obj;
                continue;
            }
            
            iRet++; //incremento in fondo, perchè se canDelete è true, l'array ha una cella in meno
        }
        
        
    }


};


#else //stubs

template<typename T>
class HazardPointers {
public:
    HazardPointers( [[maybe_unused]] int maxHPs=0,
                    [[maybe_unused]] int maxThreads = 0) {}

    void clear(const int){}
    void clear(const int, const int){}
    T* protect(const int, std::atomic<T*>& atom,const int){return atom.load();}
    T* protect(const int, T* ptr, const int){return ptr;}
    T* protectRelease(const int, T* ptr, const int){return ptr;}
    void retire(T*, const int){};
};

#endif //_HAZARD_POINTERS_H_