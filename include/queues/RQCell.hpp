#pragma once

#include <atomic>
#include <cstddef>  // For alignas

#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif


/*
 *  Dichiarazione delle strutture che immagazzinano i singoli
 *  valori dei buffer;
 * 
 *  Doppia definizione: padding a linea di cache oppure no;
 *  
 */


namespace detail{

template<class, bool padded>
struct CRQCell;

template<class T>
struct alignas(CACHE_LINE) CRQCell<T,true>{
    std::atomic<T>          val;
    std::atomic<uint64_t>   idx;
    char __pad[CACHE_LINE - (sizeof(std::atomic<T>) + sizeof(std::atomic<uint64_t>))];

};

template<class T>
struct alignas(16) CRQCell<T,false>{
    std::atomic<T>          val;
    std::atomic<uint64_t>   idx;
};

template<class T,bool padded>
struct PlainCell;

template<class T>
struct alignas(CACHE_LINE) PlainCell<T,true>{
    std::atomic<T>          val;
    char __pad[CACHE_LINE - sizeof(std::atomic<T>)];
};

template<class T>
struct PlainCell<T,false>{
    std::atomic<T>          val;
};


}