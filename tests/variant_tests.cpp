#include "mini/variant.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#ifndef MINI_VARIANT_STAGE
#define MINI_VARIANT_STAGE 0
#endif

static_assert(MINI_VARIANT_STAGE >= 0 && MINI_VARIANT_STAGE <= 3,
              "MINI_VARIANT_STAGE must be between 0 and 3");

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

constexpr std::uint32_t kLiveMagic = 0x11FE11FEU;
constexpr std::uint32_t kDeadMagic = 0xDEADDEADU;

// Counts every special member call and aborts on use-after-destroy or a
// double destroy.
class Tracked {
public:
    static inline int alive = 0;
    static inline int constructed = 0;
    static inline int copy_constructed = 0;
    static inline int move_constructed = 0;
    static inline int copy_assigned = 0;
    static inline int move_assigned = 0;

    static void reset() {
        CHECK(alive == 0);
        constructed = copy_constructed = move_constructed = 0;
        copy_assigned = move_assigned = 0;
    }

    Tracked() : value_(0) { born(); }
    explicit Tracked(int value) : value_(value) { born(); }
    Tracked(const Tracked& other) : value_(other.value()) {
        born();
        ++copy_constructed;
    }
    Tracked(Tracked&& other) noexcept : value_(other.value()) {
        born();
        ++move_constructed;
        other.value_ = -1;
    }
    Tracked& operator=(const Tracked& other) {
        CHECK(magic_ == kLiveMagic);
        value_ = other.value();
        ++copy_assigned;
        return *this;
    }
    Tracked& operator=(Tracked&& other) noexcept {
        CHECK(magic_ == kLiveMagic);
        value_ = other.value();
        other.value_ = -1;
        ++move_assigned;
        return *this;
    }
    ~Tracked() {
        CHECK(magic_ == kLiveMagic);
        magic_ = kDeadMagic;
        --alive;
    }

    int value() const {
        CHECK(magic_ == kLiveMagic);
        return value_;
    }

private:
    void born() {
        magic_ = kLiveMagic;
        ++alive;
        ++constructed;
    }

    int value_;
    std::uint32_t magic_ = 0;
};

struct alignas(64) OverAligned {
    int value = 0;
};

struct NoDefault {
    explicit NoDefault(int v) : value(v) {}
    int value;
};

struct ThrowingMove {
    ThrowingMove() = default;
    ThrowingMove(const ThrowingMove&) = default;
    ThrowingMove& operator=(const ThrowingMove&) = default;
    ThrowingMove(ThrowingMove&&) noexcept(false) {}
    ThrowingMove& operator=(ThrowingMove&&) noexcept(false) { return *this; }
};

#if MINI_VARIANT_STAGE >= 1

using Basic = mini::Variant<int, std::string, double>;

static_assert(mini::variant_size_v<Basic> == 3);
static_assert(mini::variant_size_v<mini::Variant<char>> == 1);
static_assert(std::is_same_v<mini::variant_alternative_t<0, Basic>, int>);
static_assert(std::is_same_v<mini::variant_alternative_t<1, Basic>, std::string>);
static_assert(std::is_same_v<mini::variant_alternative_t<2, Basic>, double>);

// Shared storage: the size follows the largest alternative, not the sum.
using Big = std::array<char, 256>;
static_assert(sizeof(mini::Variant<Big, Big, Big>) < 2 * sizeof(Big));
static_assert(alignof(mini::Variant<char, OverAligned>) >= alignof(OverAligned));
static_assert(alignof(mini::Variant<char, double>) >= alignof(double));

// The converting constructor only accepts exact alternatives.
static_assert(std::is_constructible_v<Basic, int>);
static_assert(std::is_constructible_v<Basic, std::string&>);
static_assert(std::is_constructible_v<Basic, const std::string&>);
static_assert(std::is_constructible_v<Basic, std::string&&>);
static_assert(!std::is_constructible_v<Basic, float>);
static_assert(!std::is_constructible_v<Basic, int*>);
static_assert(!std::is_constructible_v<mini::Variant<int, int>, int>);  // ambiguous alternative

static_assert(std::is_same_v<decltype(mini::get<1>(std::declval<Basic&>())), std::string&>);
static_assert(std::is_same_v<decltype(mini::get<1>(std::declval<const Basic&>())),
                             const std::string&>);
static_assert(std::is_same_v<decltype(mini::get_if<double>(std::declval<Basic*>())), double*>);
static_assert(noexcept(std::declval<const Basic&>().index()));

void stage_1() {
    std::cout << "[stage 1] default construction and index\n";
    {
        Basic value;
        CHECK(value.index() == 0);
        CHECK(mini::holds_alternative<int>(value));
        CHECK(!mini::holds_alternative<std::string>(value));
        CHECK(mini::get<0>(value) == 0);  // value-initialized

        mini::Variant<std::string, int> text_first;
        CHECK(text_first.index() == 0);
        CHECK(mini::get<std::string>(text_first).empty());
    }

    std::cout << "[stage 1] converting construction picks the exact alternative\n";
    {
        Basic from_int(42);
        CHECK(from_int.index() == 0);
        CHECK(mini::get<int>(from_int) == 42);

        Basic from_double(2.5);
        CHECK(from_double.index() == 2);
        CHECK(mini::get<2>(from_double) == 2.5);

        std::string text = "hello";
        Basic from_lvalue(text);
        CHECK(from_lvalue.index() == 1);
        CHECK(mini::get<std::string>(from_lvalue) == "hello");
        CHECK(text == "hello");  // copied, not moved

        std::string movable = "a string long enough to live on the heap...";
        Basic from_rvalue(std::move(movable));
        CHECK(mini::get<1>(from_rvalue) == "a string long enough to live on the heap...");
        CHECK(movable.empty());  // moved, not copied

        mini::Variant<NoDefault, int> no_default(NoDefault{7});
        CHECK(mini::get<NoDefault>(no_default).value == 7);
    }

    std::cout << "[stage 1] get through references writes the stored value\n";
    {
        Basic value(std::string("abc"));
        mini::get<std::string>(value) += "def";
        CHECK(mini::get<1>(value) == "abcdef");

        const Basic& view = value;
        CHECK(&mini::get<1>(view) == &mini::get<1>(value));
    }

    std::cout << "[stage 1] wrong alternative throws\n";
    {
        Basic value(1.5);
        bool threw = false;
        try {
            (void)mini::get<int>(value);
        } catch (const mini::bad_variant_access&) {
            threw = true;
        }
        CHECK(threw);

        threw = false;
        try {
            (void)mini::get<1>(static_cast<const Basic&>(value));
        } catch (const std::exception& error) {  // derives from std::exception
            threw = true;
            CHECK(std::string(error.what()).size() > 0);
        }
        CHECK(threw);
    }

    std::cout << "[stage 1] get_if\n";
    {
        Basic value(std::string("x"));
        CHECK(mini::get_if<int>(&value) == nullptr);
        CHECK(mini::get_if<std::string>(&value) != nullptr);
        CHECK(*mini::get_if<std::string>(&value) == "x");
        CHECK(mini::get_if<std::string>(&value) == &mini::get<1>(value));

        const Basic* view = &value;
        CHECK(mini::get_if<double>(view) == nullptr);
        CHECK(mini::get_if<int>(static_cast<Basic*>(nullptr)) == nullptr);
    }

    std::cout << "[stage 1] repeated alternatives work by index\n";
    {
        mini::Variant<int, int> twins;
        CHECK(twins.index() == 0);
        CHECK(mini::get<0>(twins) == 0);
    }

    std::cout << "[stage 1] exactly one alternative alive, destroyed once\n";
    Tracked::reset();
    {
        mini::Variant<int, Tracked> holder(Tracked(5));
        CHECK(holder.index() == 1);
        CHECK(Tracked::alive == 1);
        CHECK(mini::get<Tracked>(holder).value() == 5);

        mini::Variant<Tracked, int> defaulted;
        CHECK(Tracked::alive == 2);

        mini::Variant<int, Tracked> inactive(3);
        CHECK(Tracked::alive == 2);  // no Tracked constructed
    }
    CHECK(Tracked::alive == 0);

    std::cout << "[stage 1] over-aligned alternative\n";
    {
        mini::Variant<char, OverAligned> aligned(OverAligned{9});
        const auto address = reinterpret_cast<std::uintptr_t>(&mini::get<1>(aligned));
        CHECK(address % alignof(OverAligned) == 0);
        CHECK(mini::get<OverAligned>(aligned).value == 9);
    }
}

#endif

#if MINI_VARIANT_STAGE >= 2

static_assert(std::is_copy_constructible_v<Basic>);
static_assert(std::is_nothrow_move_constructible_v<mini::Variant<int, std::string>>);
static_assert(!std::is_nothrow_move_constructible_v<mini::Variant<int, ThrowingMove>>);
static_assert(!std::is_nothrow_move_assignable_v<mini::Variant<int, ThrowingMove>>);

void stage_2() {
    std::cout << "[stage 2] copy and move construction\n";
    {
        Basic original(std::string("original"));
        Basic copy(original);  // non-const lvalue: must copy, not convert
        CHECK(copy.index() == 1);
        mini::get<1>(copy) = "copy";
        CHECK(mini::get<1>(original) == "original");

        Basic moved(std::move(copy));
        CHECK(moved.index() == 1);
        CHECK(mini::get<1>(moved) == "copy");
        CHECK(copy.index() == 1);  // moved-from keeps its index
    }

    std::cout << "[stage 2] assignment, same alternative assigns\n";
    Tracked::reset();
    {
        mini::Variant<int, Tracked> left(Tracked(1));
        mini::Variant<int, Tracked> right(Tracked(2));
        const int constructed_before = Tracked::constructed;

        left = right;
        CHECK(mini::get<1>(left).value() == 2);
        CHECK(Tracked::copy_assigned == 1);
        CHECK(Tracked::constructed == constructed_before);  // no destroy + rebuild

        left = std::move(right);
        CHECK(Tracked::move_assigned == 1);
        CHECK(Tracked::alive == 2);
    }
    CHECK(Tracked::alive == 0);

    std::cout << "[stage 2] assignment, different alternative destroys then constructs\n";
    Tracked::reset();
    {
        mini::Variant<int, Tracked> holder(Tracked(4));
        CHECK(Tracked::alive == 1);

        mini::Variant<int, Tracked> number(10);
        holder = number;
        CHECK(holder.index() == 0);
        CHECK(mini::get<int>(holder) == 10);
        CHECK(Tracked::alive == 0);  // the Tracked was destroyed

        mini::Variant<int, Tracked> tracked(Tracked(8));
        number = tracked;
        CHECK(number.index() == 1);
        CHECK(mini::get<Tracked>(number).value() == 8);
        CHECK(Tracked::alive == 2);
        CHECK(Tracked::copy_constructed == 1);

        auto& same = number;
        number = same;  // self-assignment
        CHECK(mini::get<Tracked>(number).value() == 8);
        CHECK(Tracked::alive == 2);
    }
    CHECK(Tracked::alive == 0);

    std::cout << "[stage 2] emplace\n";
    Tracked::reset();
    {
        mini::Variant<int, Tracked, std::string> value(3);
        Tracked& made = value.emplace<Tracked>(11);
        CHECK(value.index() == 1);
        CHECK(made.value() == 11);
        CHECK(&made == &mini::get<1>(value));
        CHECK(Tracked::alive == 1);

        std::string& text = value.emplace<2>(std::size_t{3}, 'z');
        CHECK(text == "zzz");
        CHECK(Tracked::alive == 0);

        mini::Variant<int, int> twins;
        twins.emplace<1>(5);
        CHECK(twins.index() == 1);
        CHECK(mini::get<1>(twins) == 5);
    }
    CHECK(Tracked::alive == 0);

    std::cout << "[stage 2] assigning an alternative\n";
    Tracked::reset();
    {
        mini::Variant<int, Tracked> value(Tracked(1));
        value = Tracked(2);  // same alternative: assignment
        CHECK(Tracked::move_assigned == 1);
        CHECK(mini::get<1>(value).value() == 2);

        value = 7;  // different alternative: destroy + construct
        CHECK(value.index() == 0);
        CHECK(Tracked::alive == 0);

        Basic text(1);
        std::string word = "word";
        text = word;
        CHECK(text.index() == 1);
        CHECK(word == "word");
    }
    CHECK(Tracked::alive == 0);

    std::cout << "[stage 2] move-only alternative\n";
    {
        mini::Variant<int, std::unique_ptr<int>> owner(std::make_unique<int>(5));
        mini::Variant<int, std::unique_ptr<int>> other(std::move(owner));
        CHECK(*mini::get<1>(other) == 5);
        CHECK(mini::get<1>(owner) == nullptr);

        owner = std::move(other);
        CHECK(*mini::get<1>(owner) == 5);
    }
}

#endif

#if MINI_VARIANT_STAGE >= 3

template <class... Fs>
struct Overloaded : Fs... {
    using Fs::operator()...;
};
template <class... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

void stage_3() {
    std::cout << "[stage 3] visit returns the visitor's result\n";
    {
        const auto describe = Overloaded{
            [](int) { return std::string("int"); },
            [](const std::string&) { return std::string("string"); },
            [](double) { return std::string("double"); },
        };
        CHECK(mini::visit(describe, Basic(1)) == "int");
        CHECK(mini::visit(describe, Basic(std::string("s"))) == "string");
        CHECK(mini::visit(describe, Basic(0.5)) == "double");
    }

    std::cout << "[stage 3] visit passes references\n";
    {
        Basic value(std::string("ab"));
        mini::visit([](auto& alternative) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(alternative)>, std::string>) {
                alternative += "c";
            }
        }, value);
        CHECK(mini::get<1>(value) == "abc");

        const Basic& view = value;
        const std::string* seen = nullptr;
        mini::visit([&](const auto& alternative) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(alternative)>, std::string>) {
                seen = &alternative;
            }
        }, view);
        CHECK(seen == &mini::get<1>(value));
    }

    std::cout << "[stage 3] visit with many alternatives\n";
    {
        using Wide = mini::Variant<char, short, int, long, long long, float, double, std::string>;
        Wide value(std::string("last"));
        const std::size_t size = mini::visit([](const auto& alternative) {
            return sizeof(alternative);
        }, value);
        CHECK(size == sizeof(std::string));

        value = 3.0f;
        CHECK(mini::visit([](const auto& alternative) { return sizeof(alternative); }, value) ==
              sizeof(float));
    }

    std::cout << "[stage 3] operator==\n";
    {
        CHECK(Basic(1) == Basic(1));
        CHECK(!(Basic(1) == Basic(2)));
        CHECK(!(Basic(1) == Basic(1.0)));  // different alternative
        CHECK(Basic(std::string("a")) == Basic(std::string("a")));

        mini::Variant<int, int> left;
        mini::Variant<int, int> right;
        right.emplace<1>(0);
        CHECK(!(left == right));  // same value, different index
    }
}

#endif

} // namespace

int main() {
#if MINI_VARIANT_STAGE >= 1
    stage_1();
#endif
#if MINI_VARIANT_STAGE >= 2
    stage_2();
#endif
#if MINI_VARIANT_STAGE >= 3
    stage_3();
#endif
    std::cout << "All enabled mini::Variant tests passed (stage " << MINI_VARIANT_STAGE << ")\n";
}
