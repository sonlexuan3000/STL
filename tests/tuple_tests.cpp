#include "mini/tuple.hpp"
#include "mini/type_list.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#ifndef MINI_TUPLE_STAGE
#define MINI_TUPLE_STAGE 0
#endif

static_assert(MINI_TUPLE_STAGE >= 0 && MINI_TUPLE_STAGE <= 3,
              "MINI_TUPLE_STAGE must be between 0 and 3");

// Unlike assert(), CHECK stays active in Release builds.
#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << __FILE__ << ':' << __LINE__                          \
                      << ": CHECK failed: " #condition "\n";                  \
            std::abort();                                                     \
        }                                                                     \
    } while (false)

namespace {

using mini::type_list;

#if MINI_TUPLE_STAGE >= 1

// length
static_assert(mini::length_v<type_list<>> == 0);
static_assert(mini::length_v<type_list<int>> == 1);
static_assert(mini::length_v<type_list<int, char, double>> == 3);
static_assert(mini::length_v<type_list<int, int, int, int>> == 4);
static_assert(std::is_same_v<decltype(mini::length_v<type_list<int>>), const std::size_t>);

// front
template <class List>
concept HasFront = requires { typename mini::front<List>::type; };

static_assert(std::is_same_v<mini::front_t<type_list<int, char>>, int>);
static_assert(std::is_same_v<mini::front_t<type_list<const int&>>, const int&>);
static_assert(HasFront<type_list<int>>);
static_assert(!HasFront<type_list<>>);

// push_front
static_assert(std::is_same_v<mini::push_front_t<bool, type_list<>>, type_list<bool>>);
static_assert(std::is_same_v<mini::push_front_t<bool, type_list<int, char>>,
                             type_list<bool, int, char>>);
static_assert(std::is_same_v<mini::push_front_t<int&, type_list<int>>, type_list<int&, int>>);

// contains
static_assert(mini::contains_v<int, type_list<int>>);
static_assert(mini::contains_v<int, type_list<char, double, int>>);
static_assert(!mini::contains_v<int, type_list<>>);
static_assert(!mini::contains_v<int, type_list<const int, int&, long>>);
static_assert(std::is_same_v<decltype(mini::contains_v<int, type_list<int>>), const bool>);

// at
static_assert(std::is_same_v<mini::at_t<0, type_list<int, char>>, int>);
static_assert(std::is_same_v<mini::at_t<1, type_list<int, char>>, char>);
static_assert(std::is_same_v<mini::at_t<3, type_list<int, char, int, double&>>, double&>);

#endif

#if MINI_TUPLE_STAGE >= 2

using Triple = mini::Tuple<int, std::string, double>;

static_assert(mini::tuple_size_v<mini::Tuple<>> == 0);
static_assert(mini::tuple_size_v<Triple> == 3);
static_assert(std::is_same_v<mini::tuple_element_t<0, Triple>, int>);
static_assert(std::is_same_v<mini::tuple_element_t<1, Triple>, std::string>);
static_assert(std::is_same_v<mini::tuple_element_t<2, Triple>, double>);

static_assert(std::is_same_v<decltype(mini::get<1>(std::declval<Triple&>())), std::string&>);
static_assert(std::is_same_v<decltype(mini::get<1>(std::declval<const Triple&>())),
                             const std::string&>);
static_assert(noexcept(mini::get<0>(std::declval<Triple&>())));

static_assert(std::is_copy_constructible_v<Triple>);
static_assert(std::is_nothrow_move_constructible_v<mini::Tuple<std::unique_ptr<int>>>);
static_assert(!std::is_copy_constructible_v<mini::Tuple<std::unique_ptr<int>>>);

void stage_2() {
    std::cout << "[stage 2] construction and get<I>\n";
    {
        Triple triple(1, "hi", 2.5);
        CHECK(mini::get<0>(triple) == 1);
        CHECK(mini::get<1>(triple) == "hi");
        CHECK(mini::get<2>(triple) == 2.5);

        mini::get<0>(triple) = 42;
        mini::get<1>(triple) += " there";
        CHECK(mini::get<0>(triple) == 42);
        CHECK(mini::get<1>(triple) == "hi there");

        const Triple& view = triple;
        CHECK(mini::get<2>(view) == 2.5);
        CHECK(&mini::get<1>(view) == &mini::get<1>(triple));
    }

    std::cout << "[stage 2] default construction value-initializes\n";
    {
        mini::Tuple<int, double, std::string> zero;
        CHECK(mini::get<0>(zero) == 0);
        CHECK(mini::get<1>(zero) == 0.0);
        CHECK(mini::get<2>(zero).empty());

        mini::Tuple<> empty;
        (void)empty;
    }

    std::cout << "[stage 2] repeated element types\n";
    {
        mini::Tuple<int, int, int> numbers(1, 2, 3);
        CHECK(mini::get<0>(numbers) == 1);
        CHECK(mini::get<1>(numbers) == 2);
        CHECK(mini::get<2>(numbers) == 3);
        CHECK(&mini::get<0>(numbers) != &mini::get<1>(numbers));
    }

    std::cout << "[stage 2] copy and move\n";
    {
        Triple original(7, "original", 1.0);
        Triple copy = original;
        mini::get<1>(copy) = "copy";
        CHECK(mini::get<1>(original) == "original");
        CHECK(mini::get<1>(copy) == "copy");

        Triple moved = std::move(copy);
        CHECK(mini::get<1>(moved) == "copy");

        copy = original;
        CHECK(mini::get<1>(copy) == "original");
    }

    std::cout << "[stage 2] move-only elements\n";
    {
        mini::Tuple<std::unique_ptr<int>, int> owner;
        CHECK(mini::get<0>(owner) == nullptr);
        mini::get<0>(owner) = std::make_unique<int>(5);

        mini::Tuple<std::unique_ptr<int>, int> other = std::move(owner);
        CHECK(mini::get<0>(owner) == nullptr);
        CHECK(*mini::get<0>(other) == 5);
    }
}

#endif

#if MINI_TUPLE_STAGE >= 3

static_assert(mini::index_of_v<int, type_list<int>> == 0);
static_assert(mini::index_of_v<int, type_list<char, int, double>> == 1);
static_assert(mini::index_of_v<double, type_list<char, int, double>> == 2);
static_assert(mini::index_of_v<int, type_list<char, int, int>> == 1);

static_assert(std::is_same_v<decltype(mini::make_tuple(1, 2.0)), mini::Tuple<int, double>>);

static_assert(std::is_same_v<decltype(mini::get<1>(std::declval<Triple&&>())), std::string&&>);

struct Point {
    int x;
    int y;
    friend bool operator==(const Point&, const Point&) = default;
};

void stage_3() {
    std::cout << "[stage 3] get<T>\n";
    {
        mini::Tuple<int, std::string, double> tuple(7, "x", 0.5);
        CHECK(mini::get<int>(tuple) == 7);
        mini::get<std::string>(tuple) += "y";
        CHECK(mini::get<1>(tuple) == "xy");
        CHECK(&mini::get<double>(tuple) == &mini::get<2>(tuple));

        const auto& view = tuple;
        static_assert(std::is_same_v<decltype(mini::get<std::string>(view)), const std::string&>);
        CHECK(mini::get<std::string>(view) == "xy");
    }

    std::cout << "[stage 3] make_tuple strips references and cv\n";
    {
        const int constant = 5;
        int value = 6;
        int& reference = value;
        auto made = mini::make_tuple(constant, reference, std::string("s"));
        static_assert(std::is_same_v<decltype(made), mini::Tuple<int, int, std::string>>);
        CHECK(mini::get<0>(made) == 5);
        CHECK(mini::get<1>(made) == 6);
        mini::get<1>(made) = 100;
        CHECK(value == 6);  // a copy, not a reference
        CHECK(mini::get<2>(made) == "s");
    }

    std::cout << "[stage 3] operator==\n";
    {
        using Pair = mini::Tuple<int, std::string>;
        CHECK(Pair(1, "a") == Pair(1, "a"));
        CHECK(!(Pair(1, "a") == Pair(2, "a")));
        CHECK(!(Pair(1, "a") == Pair(1, "b")));
        CHECK(mini::Tuple<>() == mini::Tuple<>());
        CHECK(mini::make_tuple(Point{1, 2}) == mini::make_tuple(Point{1, 2}));
    }

    std::cout << "[stage 3] structured bindings\n";
    {
        auto [number, text, ratio] = mini::make_tuple(1, std::string("two"), 3.0);
        CHECK(number == 1);
        CHECK(text == "two");
        CHECK(ratio == 3.0);

        mini::Tuple<int, std::string> tuple(1, "a");
        auto& [first, second] = tuple;
        first = 9;
        second = "b";
        CHECK(mini::get<0>(tuple) == 9);
        CHECK(mini::get<1>(tuple) == "b");

        auto [copied_first, copied_second] = tuple;  // a copy of the tuple
        copied_second = "c";
        CHECK(copied_first == 9);
        CHECK(mini::get<1>(tuple) == "b");

        mini::Tuple<std::string> source("moved");
        std::string taken = mini::get<0>(std::move(source));
        CHECK(taken == "moved");
    }
}

#endif

} // namespace

int main() {
#if MINI_TUPLE_STAGE >= 2
    stage_2();
#endif
#if MINI_TUPLE_STAGE >= 3
    stage_3();
#endif
    std::cout << "All enabled mini::Tuple tests passed (stage " << MINI_TUPLE_STAGE << ")\n";
}
