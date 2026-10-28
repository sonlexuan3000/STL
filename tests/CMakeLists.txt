# Shared warning / sanitizer settings for every test executable.
add_library(mini_stl_test_options INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(mini_stl_test_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

    if(MINI_STL_WARNINGS_AS_ERRORS)
        target_compile_options(mini_stl_test_options INTERFACE -Werror)
    endif()

    if(MINI_STL_THREAD_SANITIZER)
        target_compile_options(mini_stl_test_options INTERFACE
            -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(mini_stl_test_options INTERFACE
            -fsanitize=thread)
    elseif(MINI_STL_SANITIZERS)
        target_compile_options(mini_stl_test_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(mini_stl_test_options INTERFACE
            -fsanitize=address,undefined)
    endif()
endif()

# mini_stl_add_test(<name> <source> [CXX17] [STAGE_MACRO <macro> STAGE <n>])
#
# CXX17 builds the test as C++17 instead of the library's C++20, for components
# that must also compile on C++17-only online judges.
function(mini_stl_add_test name source)
    cmake_parse_arguments(ARG "CXX17" "STAGE_MACRO;STAGE" "" ${ARGN})

    if(DEFINED ARG_STAGE AND ARG_STAGE EQUAL 0)
        message(STATUS "mini_stl: ${name} disabled (stage 0)")
        return()
    endif()

    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE mini_stl_test_options)

    if(ARG_CXX17)
        # Not linked to mini::stl, whose cxx_std_20 requirement would win.
        target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/include")
        set_target_properties(${name} PROPERTIES
            CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    else()
        target_link_libraries(${name} PRIVATE mini::stl)
    endif()

    if(DEFINED ARG_STAGE_MACRO)
        target_compile_definitions(${name} PRIVATE ${ARG_STAGE_MACRO}=${ARG_STAGE})
    endif()

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

mini_stl_add_test(vector_tests vector_tests.cpp)

mini_stl_add_test(list_tests list_tests.cpp
    STAGE_MACRO MINI_LIST_STAGE STAGE ${MINI_LIST_STAGE})

mini_stl_add_test(ring_deque_tests ring_deque_tests.cpp
    STAGE_MACRO MINI_RING_DEQUE_STAGE STAGE ${MINI_RING_DEQUE_STAGE})

mini_stl_add_test(priority_queue_tests priority_queue_tests.cpp
    STAGE_MACRO MINI_PRIORITY_QUEUE_STAGE STAGE ${MINI_PRIORITY_QUEUE_STAGE})

mini_stl_add_test(shared_ptr_tests shared_ptr_tests.cpp
    STAGE_MACRO MINI_SHARED_PTR_STAGE STAGE ${MINI_SHARED_PTR_STAGE})

if(TARGET shared_ptr_tests)
    target_link_libraries(shared_ptr_tests PRIVATE Threads::Threads)
endif()

mini_stl_add_test(shared_ptr_exception_tests shared_ptr_exception_tests.cpp
    STAGE_MACRO MINI_SHARED_PTR_STAGE STAGE ${MINI_SHARED_PTR_STAGE})

mini_stl_add_test(weak_ptr_tests weak_ptr_tests.cpp
    STAGE_MACRO MINI_WEAK_PTR_STAGE STAGE ${MINI_WEAK_PTR_STAGE})

if(TARGET weak_ptr_tests)
    target_link_libraries(weak_ptr_tests PRIVATE Threads::Threads)
endif()

mini_stl_add_test(function_tests function_tests.cpp
    STAGE_MACRO MINI_FUNCTION_STAGE STAGE ${MINI_FUNCTION_STAGE})

mini_stl_add_test(spsc_queue_tests spsc_queue_tests.cpp
    STAGE_MACRO MINI_SPSC_QUEUE_STAGE STAGE ${MINI_SPSC_QUEUE_STAGE})

if(TARGET spsc_queue_tests)
    target_link_libraries(spsc_queue_tests PRIVATE Threads::Threads)
endif()

mini_stl_add_test(type_traits_tests type_traits_tests.cpp
    STAGE_MACRO MINI_TYPE_TRAITS_STAGE STAGE ${MINI_TYPE_TRAITS_STAGE})

mini_stl_add_test(tuple_tests tuple_tests.cpp
    STAGE_MACRO MINI_TUPLE_STAGE STAGE ${MINI_TUPLE_STAGE})

mini_stl_add_test(variant_tests variant_tests.cpp
    STAGE_MACRO MINI_VARIANT_STAGE STAGE ${MINI_VARIANT_STAGE})

mini_stl_add_test(flat_hash_map_tests flat_hash_map_tests.cpp
    STAGE_MACRO MINI_FLAT_HASH_MAP_STAGE STAGE ${MINI_FLAT_HASH_MAP_STAGE})

mini_stl_add_test(hash_map_tests hash_map_tests.cpp CXX17
    STAGE_MACRO MINI_HASH_MAP_STAGE STAGE ${MINI_HASH_MAP_STAGE})

# mini::HashMap must stay submittable to the WhiteBox judge (C++17, no
# standard container names anywhere in the class).
add_library(hash_map_whitebox_check OBJECT hash_map_whitebox_check.cpp)
target_include_directories(hash_map_whitebox_check PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(hash_map_whitebox_check PRIVATE mini_stl_test_options)
set_target_properties(hash_map_whitebox_check PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# Every public header must compile on its own (no hidden include-order deps).
file(GLOB MINI_STL_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/mini/*.hpp")

set(header_check_sources "")
foreach(header IN LISTS MINI_STL_HEADERS)
    get_filename_component(header_name "${header}" NAME_WE)
    set(check_source "${CMAKE_CURRENT_BINARY_DIR}/header_check/${header_name}.cpp")
    file(GENERATE OUTPUT "${check_source}"
        CONTENT "#include \"mini/${header_name}.hpp\"\n")
    list(APPEND header_check_sources "${check_source}")
endforeach()

add_library(header_self_containment OBJECT ${header_check_sources})
target_link_libraries(header_self_containment PRIVATE mini::stl mini_stl_test_options)
