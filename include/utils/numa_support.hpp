#pragma once
#ifndef DISABLE_NUMA
    #ifdef __has_include
        #if __has_include(<numa.h>)
            #include <numa.h>
            #include <pthread.h>
        #else
            #define DISABLE_NUMA
            #warning "numa_ctrl not found, NUMA support will be disabled"
        #endif
    #endif
#endif


#ifndef DISABLE_NUMA

inline bool isNumaAvailable() {
    return numa_available() == 0;
}

inline int getNumaNode() {
    return numa_node_of_cpu(sched_getcpu());
}

#else

inline bool isNumaAvailable() {
    return false;
}

inline int getNumaNode() {
    return 0;
}

#endif  //DISABLE_NUMA



