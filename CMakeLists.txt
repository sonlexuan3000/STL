cmake_minimum_required(VERSION 3.21)

project(mini_stl
    VERSION 0.1.0
    DESCRIPTION "Educational re-implementations of C++ standard library containers"
    LANGUAGES CXX)

# Header-only library -----------------------------------------------------------

add_library(mini_stl INTERFACE)
add_library(mini::stl ALIAS mini_stl)

target_include_directories(mini_stl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(mini_stl INTERFACE cxx_std_20)

# Options -----------------------------------------------------------------------

option(MINI_STL_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
option(MINI_STL_SANITIZERS "Build tests with AddressSanitizer and UBSan" ON)
option(MINI_STL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Each container is built in checkpoints (see docs/). The test suite only
# enables the checkpoints up to the stage selected here; 0 disables the test.
set(MINI_LIST_STAGE          3 CACHE STRING "Highest mini::List test stage to build (0-4)")
set(MINI_RING_DEQUE_STAGE    3 CACHE STRING "Highest mini::RingDeque test stage to build (0-3)")
set(MINI_PRIORITY_QUEUE_STAGE 3 CACHE STRING "Highest mini::PriorityQueue test stage to build (0-3)")
set(MINI_SHARED_PTR_STAGE    9 CACHE STRING "Highest mini::SharedPtr test stage to build (0-9)")
set(MINI_WEAK_PTR_STAGE      4 CACHE STRING "Highest mini::WeakPtr test stage to build (0-4)")
set(MINI_FUNCTION_STAGE      2 CACHE STRING "Highest mini::Function test stage to build (0-4)")
set(MINI_FLAT_HASH_MAP_STAGE 0 CACHE STRING "Highest mini::FlatHashMap test stage to build (0-3)")
set(MINI_HASH_MAP_STAGE      2 CACHE STRING "Highest mini::HashMap test stage to build (0-2)")

if(MINI_STL_BUILD_TESTS)
    find_package(Threads REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()
