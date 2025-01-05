cmake_minimum_required(VERSION 3.5)

project(QueueBenchmark)
set(CMAKE_CXX_STANDARD 20)  # Use C++20 standard


set(CMAKE_BUILD_TYPE Debug)

# Header files
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/benchmark
    ${CMAKE_SOURCE_DIR}/include/queues
    ${CMAKE_SOURCE_DIR}/include/utils
)

# Search for the NUMA library and headers manually
find_library(NUMA_LIBRARY numa)
find_path(NUMA_INCLUDE_DIR numa.h)

# Source files for the main executables
set(MAIN_SRC
    src/main.cpp 
    src/utils/AdditionalWork.cpp
)

# Create executables
add_executable(.memoryManager src/benchmark/MemoryMonitor.cpp)
add_executable(paddedMain ${MAIN_SRC})
add_executable(unpaddedMain ${MAIN_SRC})
add_executable(fsanMain ${MAIN_SRC})

# Include directories for all executables
target_include_directories(.memoryManager PRIVATE ${INCLUDE_DIRS})
target_include_directories(paddedMain     PRIVATE ${INCLUDE_DIRS})
target_include_directories(unpaddedMain   PRIVATE ${INCLUDE_DIRS})
target_include_directories(fsanMain       PRIVATE ${INCLUDE_DIRS})

# Compile options
target_compile_options(paddedMain   PUBLIC -O3)
target_compile_options(fsanMain PRIVATE -fsanitize=address -O3 PUBLIC -g)
target_link_libraries(fsanMain  PRIVATE -fsanitize=address)

target_compile_options(unpaddedMain   PUBLIC -O3)
target_compile_options(.memoryManager PUBLIC -O3)

# Compile definitions
target_compile_definitions(unpaddedMain PUBLIC NO_PADDING)

# # Link libraries
# if(NUMA_LIBRARY AND NUMA_INCLUDE_DIR)
#   target_link_libraries(fsanMain      PRIVATE -fsanitize=address numa)
#   target_link_libraries(paddedMain    PRIVATE numa)
#   target_link_libraries(unpaddedMain  PRIVATE numa)
# else()
target_compile_definitions(fsanMain     PUBLIC DISABLE_NUMA)
target_compile_definitions(paddedMain   PUBLIC DISABLE_NUMA)
target_compile_definitions(unpaddedMain PUBLIC DISABLE_NUMA)
# endif()

### --- TESTING --- ###
# Add GoogleTest as a submodule
add_subdirectory(include/googletest)

set(GTEST_DIR ${CMAKE_SOURCE_DIR}/include/googletest/googletest/include)

add_executable(testCases_pow2 src/test/queueTestCases.cpp)
add_executable(testCases src/test/queueTestCases.cpp)
target_include_directories(testCases PRIVATE ${INCLUDE_DIRS} ${GTEST_DIR})
target_include_directories(testCases_pow2 PRIVATE ${INCLUDE_DIRS} ${GTEST_DIR})

# if(NUMA_LIBRARY AND NUMA_INCLUDE_DIR) # to ensure that test can run with/without NUMA
#   target_link_libraries(testCases gtest gtest_main pthread numa)
# else()
target_compile_definitions(testCases PUBLIC DISABLE_NUMA DISABLE_POW2)
target_compile_definitions(testCases_pow2 PUBLIC DISABLE_NUMA)
target_compile_options(testCases PUBLIC -fsanitize=address -g)
target_compile_options(testCases_pow2 PUBLIC -fsanitize=address -g)

target_link_libraries(testCases gtest gtest_main pthread -fsanitize=address)
target_link_libraries(testCases_pow2 gtest gtest_main pthread -fsanitize=address)
# endif()

enable_testing()
add_test(NAME test COMMAND testCases)
add_test(NAME test_pow2 COMMAND testCases_pow2)



# Hard Inclusion of the google test library so it works without FetchContent
#
#
# Fetch GoogleTest
# include(FetchContent)

# # Declare GoogleTest to be fetched
# FetchContent_Declare(
#   googletest
#   GIT_REPOSITORY https://github.com/google/googletest.git
#   GIT_TAG        v1.15.2  # Use the latest release or specify the version
#   DOWNLOAD_EXTRACT_TIMESTAMP true
# )

# # Make sure GoogleTest is built before it is used
# FetchContent_MakeAvailable(googletest)

# # Link GoogleTest and pthread to the test executable
# target_compile_options(testCases PRIVATE -fsanitize=address PUBLIC -g)
# target_link_libraries(testCases PRIVATE GTest::gtest GTest::gmock pthread -fsanitize=address)

# # Include directories for the test executable
# target_include_directories(testCases PRIVATE ${INCLUDE_DIRS} ${gtest_SOURCE_DIR}/googletest/include)

# # Enable testing
# enable_testing()

# # Add a CMake test for running the tests
# add_test(NAME testCases COMMAND testCases testCases_pow2)
