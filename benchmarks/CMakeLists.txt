add_executable(spsc_queue_benchmark spsc_queue_benchmark.cpp)
target_link_libraries(spsc_queue_benchmark PRIVATE mini::stl Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(spsc_queue_benchmark PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()
