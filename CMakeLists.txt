cmake_minimum_required(VERSION 3.10)

project(QueueBenchmark) # Set project name
set(CMAKE_CXX_STANDARD 20) # c++ 20 version


#header files
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/benchmark)
include_directories(${CMAKE_SOURCE_DIR}/include/queues)

#Source files
set(SOURCES src/main.cpp src/benchmark/BenchmarkUtils.cpp src/AdditionalWork.cpp)

# Create executable
add_executable(padding ${SOURCES})
add_executable(no_padding ${SOURCES})
add_executable(fsan ${SOURCES})

target_compile_options(padding PUBLIC -O3)
target_compile_options(fsan PRIVATE -fsanitize=address -O3 PUBLIC -g)
target_compile_options(no_padding PUBLIC -O3)

target_compile_definitions(no_padding PUBLIC NO_PADDING)
target_link_libraries(fsan PRIVATE -fsanitize=address)

