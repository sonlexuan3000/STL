# Shared warning / sanitizer settings for every test executable.
add_library(mini_stl_test_options INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(mini_stl_test_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

    if(MINI_STL_WARNINGS_AS_ERRORS)
        target_compile_options(mini_stl_test_options INTERFACE -Werror)
    endif()

    if(MINI_STL_SANITIZERS)
        target_compile_options(mini_stl_test_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(mini_stl_test_options INTERFACE
            -fsanitize=address,undefined)
    endif()
endif()

# mini_stl_add_test(<name> <source> [STAGE_MACRO <macro> STAGE <n>])
function(mini_stl_add_test name source)
    cmake_parse_arguments(ARG "" "STAGE_MACRO;STAGE" "" ${ARGN})

    if(DEFINED ARG_STAGE AND ARG_STAGE EQUAL 0)
        message(STATUS "mini_stl: ${name} disabled (stage 0)")
        return()
    endif()

    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE mini::stl mini_stl_test_options)

    if(DEFINED ARG_STAGE_MACRO)
        target_compile_definitions(${name} PRIVATE ${ARG_STAGE_MACRO}=${ARG_STAGE})
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

mini_stl_add_test(vector_tests vector_tests.cpp)

mini_stl_add_test(list_tests list_tests.cpp
    STAGE_MACRO MINI_LIST_STAGE STAGE ${MINI_LIST_STAGE})

mini_stl_add_test(ring_deque_tests ring_deque_tests.cpp
    STAGE_MACRO MINI_RING_DEQUE_STAGE STAGE ${MINI_RING_DEQUE_STAGE})

mini_stl_add_test(flat_hash_map_tests flat_hash_map_tests.cpp
    STAGE_MACRO MINI_FLAT_HASH_MAP_STAGE STAGE ${MINI_FLAT_HASH_MAP_STAGE})

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
