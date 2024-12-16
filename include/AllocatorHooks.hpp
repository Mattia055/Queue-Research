#ifndef MEMORY_HOOKS_H
#define MEMORY_HOOKS_H

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <functional>

// Global variable to track memory usage
extern size_t __totalMemory;

// Map to store the size of each allocation
extern std::unordered_map<size_t, size_t> __allocationSizes;

// Mutex to ensure thread-safety
extern std::mutex memory_mutex;

// Custom new operator



void* operator new(size_t size) {
    std::lock_guard<std::mutex> lock(memory_mutex);  // Lock the mutex for thread-safety

    __totalMemory += size;  // Increment total memory
    void* ptr = std::malloc(size);  // Use malloc to allocate memory
    if (!ptr) {
        throw std::bad_alloc();  // Handle memory allocation failure
    }

    __allocationSizes[reinterpret_cast<size_t>(ptr)] = size;    // Store the allocation size
    std::cout << "Allocated " << size << " bytes. Total memory: " << __totalMemory << " bytes\n";
    return ptr;
}

// Custom delete operator
void operator delete(void* pointer) noexcept {
    std::lock_guard<std::mutex> lock(memory_mutex);  // Lock the mutex for thread-safety

    if (pointer) {
        auto it = __allocationSizes.find(reinterpret_cast<size_t>(pointer));//Check if the pointer exists in the map
        if (it != __allocationSizes.end()) {
            size_t size = it->second;
            __totalMemory -= size;  // Decrement total memory
            __allocationSizes.erase(it);  // Remove from the map
            std::free(pointer);  // Use free to deallocate memory
            std::cout << "Deallocated " << size << " bytes. Total memory: " << __totalMemory << " bytes\n";
        } else {
            std::cerr << "Warning: Trying to delete a pointer not found in allocation map.\n";
        }
    }
}

// Custom new[] operator
void* operator new[](size_t size) {
    std::lock_guard<std::mutex> lock(memory_mutex);  // Lock the mutex for thread-safety

    __totalMemory += size;  // Increment total memory
    void* ptr = std::malloc(size);  // Use malloc to allocate memory for the array
    if (!ptr) {
        throw std::bad_alloc();  // Handle memory allocation failure
    }

    size_t ptrsize = reinterpret_cast<size_t>(ptr);
    std::cout << ptrsize << "Bucket count "<<  __allocationSizes.bucket_count() << std::endl;
    __allocationSizes[ptrsize] = size;    // Store the allocation size
    std::cout << "Allocated array of " << size << " bytes. Total memory: " << __totalMemory << " bytes\n";
    return ptr;
}

// Custom delete[] operator
void operator delete[](void* pointer) noexcept {
    std::lock_guard<std::mutex> lock(memory_mutex);  // Lock the mutex for thread-safety

    if (pointer) {
        auto it = __allocationSizes.find(reinterpret_cast<size_t>(pointer)); //heck if the pointer exists in the map
        if (it != __allocationSizes.end()) {
            size_t size = it->second;
            __totalMemory -= size;  // Decrement total memory
            __allocationSizes.erase(it);  // Remove from the map
            std::free(pointer);  // Use free to deallocate memory
            std::cout << "Deallocated array of " << size << " bytes. Total memory: " << __totalMemory << " bytes\n";
        } else {
            std::cerr << "Warning: Trying to delete[] a pointer not found in allocation map.\n";
        }
    }
}



#endif // MEMORY_HOOKS_H
