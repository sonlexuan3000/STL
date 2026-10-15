// Compile-only check that mini/hash_map.hpp can be submitted to the WhiteBox
// judge: C++17, standard headers already included, and the names of the
// standard containers defined away so they cannot appear in the submission.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#if __cplusplus > 201703L
#error "hash_map_whitebox_check must be compiled as C++17"
#endif

#define vector WHITEBOX_BANNED_IDENTIFIER_vector
#define deque WHITEBOX_BANNED_IDENTIFIER_deque
#define list WHITEBOX_BANNED_IDENTIFIER_list
#define forward_list WHITEBOX_BANNED_IDENTIFIER_forward_list
#define array WHITEBOX_BANNED_IDENTIFIER_array
#define valarray WHITEBOX_BANNED_IDENTIFIER_valarray
#define map WHITEBOX_BANNED_IDENTIFIER_map
#define multimap WHITEBOX_BANNED_IDENTIFIER_multimap
#define set WHITEBOX_BANNED_IDENTIFIER_set
#define multiset WHITEBOX_BANNED_IDENTIFIER_multiset
#define unordered_map WHITEBOX_BANNED_IDENTIFIER_unordered_map
#define unordered_multimap WHITEBOX_BANNED_IDENTIFIER_unordered_multimap
#define unordered_set WHITEBOX_BANNED_IDENTIFIER_unordered_set
#define unordered_multiset WHITEBOX_BANNED_IDENTIFIER_unordered_multiset

#include "mini/hash_map.hpp"

// Instantiate every member that has a definition.
template class mini::HashMap<int, int>;
template class mini::HashMap<std::string, std::string>;
