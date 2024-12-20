cmake_minimum_required(VERSION 3.5)

project(QueueBenchmark)  # Set project name
set(CMAKE_CXX_STANDARD 20)  # Use C++20 standard

# Header files
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/benchmark
    ${CMAKE_SOURCE_DIR}/include/queues
)

# Required libraries
# Search for the NUMA library and headers manually
find_library(NUMA_LIBRARY numa)
find_path(NUMA_INCLUDE_DIR numa.h)

# Source files for the main executables
set(MAIN_SRC
    src/main.cpp 
    src/benchmark/BenchmarkUtils.cpp
    src/AdditionalWork.cpp
)

# Create executables
add_executable(memoryManager src/benchmark/MemoryMonitor.cpp src/benchmark/BenchmarkUtils.cpp)
add_executable(padding ${MAIN_SRC})
add_executable(no_padding ${MAIN_SRC})
add_executable(fsan ${MAIN_SRC})
# Create the test executable
add_executable(tests src/test.cpp)

# Include directories for all executables
target_include_directories(memoryManager PRIVATE ${INCLUDE_DIRS})
target_include_directories(padding PRIVATE ${INCLUDE_DIRS})
target_include_directories(no_padding PRIVATE ${INCLUDE_DIRS})
target_include_directories(fsan PRIVATE ${INCLUDE_DIRS})
target_include_directories(tests PRIVATE ${INCLUDE_DIRS})

# Compile options
target_compile_options(padding PUBLIC -O3)
target_compile_options(fsan PRIVATE -fsanitize=address -O3 PUBLIC -g)
target_compile_options(no_padding PUBLIC -O3)
target_compile_options(memoryManager PUBLIC -O3)

# Compile definitions
target_compile_definitions(no_padding PUBLIC NO_PADDING)

# Link libraries
if(NUMA_LIBRARY AND NUMA_INCLUDE_DIR)
  target_link_libraries(fsan PRIVATE -fsanitize=address numa)
  target_link_libraries(padding PRIVATE numa)
  target_link_libraries(no_padding PRIVATE numa)
  target_link_libraries(tests PRIVATE numa)
else()
  target_compile_definitions(fsan PUBLIC DISABLE_NUMA)
  target_compile_definitions(padding PUBLIC DISABLE_NUMA)
  target_compile_definitions(no_padding PUBLIC DISABLE_NUMA)
  target_compile_definitions(tests PUBLIC DISABLE_NUMA)
endif()

# Uncomment the following lines if you need to link jemalloc
# target_link_libraries(padding PRIVATE ${JEMALLOC_LIBRARIES})
# target_include_directories(padding PRIVATE ${JEMALLOC_INCLUDE_DIRS})

# Fetch GoogleTest
include(FetchContent)

# Declare GoogleTest to be fetched
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2  # Use the latest release or specify the version
  DOWNLOAD_EXTRACT_TIMESTAMP true
)

# Make sure GoogleTest is built before it is used
FetchContent_MakeAvailable(googletest)

# Link GoogleTest and pthread to the test executable
target_link_libraries(tests PRIVATE GTest::gtest GTest::gmock pthread)

# Include directories for the test executable
target_include_directories(tests PRIVATE ${INCLUDE_DIRS} ${gtest_SOURCE_DIR}/googletest/include)

# Enable testing
enable_testing()

# Add a CMake test for running the tests
add_test(NAME tests COMMAND tests)
