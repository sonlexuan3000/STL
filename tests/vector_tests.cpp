#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>

#include "mini/vector.hpp"

struct NoDefault {
int value;


NoDefault() = delete;

explicit NoDefault(int v)
    : value(v) {}
};

struct Lifetime {
static int direct_ctor;
static int copy_ctor;
static int move_ctor;
static int copy_assign;
static int move_assign;
static int dtor;
static int alive;

int id;
bool moved_from = false;

explicit Lifetime(int id_)
    : id(id_) {
    ++direct_ctor;
    ++alive;
}

Lifetime(const Lifetime& other)
    : id(other.id), moved_from(false) {
    ++copy_ctor;
    ++alive;
}

Lifetime(Lifetime&& other) noexcept
    : id(other.id), moved_from(false) {
    ++move_ctor;
    ++alive;

    other.id = -1;
    other.moved_from = true;
}

Lifetime& operator=(const Lifetime& other) {
    ++copy_assign;

    if (this != &other) {
        id = other.id;
        moved_from = false;
    }

    return *this;
}

Lifetime& operator=(Lifetime&& other) noexcept {
    ++move_assign;

    if (this != &other) {
        id = other.id;
        moved_from = false;

        other.id = -1;
        other.moved_from = true;
    }

    return *this;
}

~Lifetime() {
    ++dtor;
    --alive;
}
};

int Lifetime::direct_ctor = 0;
int Lifetime::copy_ctor = 0;
int Lifetime::move_ctor = 0;
int Lifetime::copy_assign = 0;
int Lifetime::move_assign = 0;
int Lifetime::dtor = 0;
int Lifetime::alive = 0;

void reset_lifetime_counts() {
Lifetime::direct_ctor = 0;
Lifetime::copy_ctor = 0;
Lifetime::move_ctor = 0;
Lifetime::copy_assign = 0;
Lifetime::move_assign = 0;
Lifetime::dtor = 0;
Lifetime::alive = 0;
}

struct MoveOnly {
static int alive;
static int move_ctor;
static int move_assign;
static int dtor;

int id;
bool moved_from = false;

explicit MoveOnly(int id_)
    : id(id_) {
    ++alive;
}

MoveOnly(const MoveOnly&) = delete;
MoveOnly& operator=(const MoveOnly&) = delete;

MoveOnly(MoveOnly&& other) noexcept
    : id(other.id), moved_from(false) {
    ++move_ctor;
    ++alive;

    other.id = -1;
    other.moved_from = true;
}

MoveOnly& operator=(MoveOnly&& other) noexcept {
    ++move_assign;

    if (this != &other) {
        id = other.id;
        moved_from = false;

        other.id = -1;
        other.moved_from = true;
    }

    return *this;
}

~MoveOnly() {
    ++dtor;
    --alive;
}

};

int MoveOnly::alive = 0;
int MoveOnly::move_ctor = 0;
int MoveOnly::move_assign = 0;
int MoveOnly::dtor = 0;

void reset_move_only_counts() {
MoveOnly::alive = 0;
MoveOnly::move_ctor = 0;
MoveOnly::move_assign = 0;
MoveOnly::dtor = 0;
}

struct ForwardProbe {
int tag;

explicit ForwardProbe(int&) : tag(1) {}
explicit ForwardProbe(int&&) : tag(2) {}

};

struct alignas(64) BigAligned {
long long payload[8];

explicit BigAligned(long long x) {
    for (int i = 0; i < 8; ++i) {
        payload[i] = x + i;
    }
}

};

template <typename Vec>
void assert_int_values(const Vec& v, std::initializer_list<int> expected) {
assert(v.size() == expected.size());

std::size_t i = 0;
for (int x : expected) {
    assert(v[i] == x);
    ++i;
}

}

template <typename Vec>
void assert_lifetime_ids(const Vec& v, std::initializer_list<int> expected) {
assert(v.size() == expected.size());

std::size_t i = 0;
for (int x : expected) {
    assert(v[i].id == x);
    ++i;
}

}

template <typename Vec>
void assert_move_only_ids(const Vec& v, std::initializer_list<int> expected) {
assert(v.size() == expected.size());

std::size_t i = 0;
for (int x : expected) {
    assert(v[i].id == x);
    ++i;
}

}

int main() {
std::cout << "[test 1] default constructor\n";
{
mini::Vector<int> v;

    assert(v.size() == 0);
    assert(v.capacity() == 0);
    assert(v.empty());
    assert(v.data() == nullptr);
}

std::cout << "[test 2] reserve basic\n";
{
    mini::Vector<int> v;

    v.reserve(0);

    assert(v.size() == 0);
    assert(v.capacity() == 0);
    assert(v.data() == nullptr);

    v.reserve(5);

    assert(v.size() == 0);
    assert(v.capacity() >= 5);
    assert(v.data() != nullptr);

    int* old_data = v.data();
    std::size_t old_capacity = v.capacity();

    v.reserve(3);

    assert(v.size() == 0);
    assert(v.capacity() == old_capacity);
    assert(v.data() == old_data);
}

std::cout << "[test 3] emplace_back / push_back int / growth\n";
{
    mini::Vector<int> v;

    for (int i = 0; i < 100; ++i) {
        int& ref = v.emplace_back(i);

        assert(ref == i);
        assert(v.size() == static_cast<std::size_t>(i + 1));
        assert(v.capacity() >= v.size());
        assert(v.back() == i);
    }

    assert(v.front() == 0);
    assert(v.back() == 99);

    for (int i = 0; i < 100; ++i) {
        assert(v[static_cast<std::size_t>(i)] == i);
    }

    int x = 123;
    v.push_back(x);
    assert(v.back() == 123);

    v.push_back(456);
    assert(v.back() == 456);

    assert(v.size() == 102);
    assert(v.capacity() >= v.size());
}

std::cout << "[test 4] data contiguity\n";
{
    mini::Vector<int> v;

    for (int i = 0; i < 50; ++i) {
        v.emplace_back(i * 10);
    }

    for (std::size_t i = 0; i < v.size(); ++i) {
        assert(v.data() + i == &v[i]);
    }
}

std::cout << "[test 5] pop_back / clear keeps capacity\n";
{
    mini::Vector<int> v;

    for (int i = 1; i <= 5; ++i) {
        v.emplace_back(i);
    }

    std::size_t cap = v.capacity();
    int* data = v.data();

    v.pop_back();
    assert_int_values(v, {1, 2, 3, 4});
    assert(v.capacity() == cap);
    assert(v.data() == data);

    v.pop_back();
    assert_int_values(v, {1, 2, 3});
    assert(v.capacity() == cap);
    assert(v.data() == data);

    v.clear();

    assert(v.size() == 0);
    assert(v.empty());
    assert(v.capacity() == cap);
    assert(v.data() == data);

    v.emplace_back(99);

    assert_int_values(v, {99});
    assert(v.capacity() == cap);
}

std::cout << "[test 6] non-default-constructible type\n";
{
    mini::Vector<NoDefault> v;

    v.reserve(10);

    assert(v.size() == 0);
    assert(v.capacity() >= 10);

    v.emplace_back(7);
    v.emplace_back(8);

    assert(v.size() == 2);
    assert(v[0].value == 7);
    assert(v[1].value == 8);
}

std::cout << "[test 7] lifetime: emplace/pop/clear/destructor\n";
{
    reset_lifetime_counts();

    {
        mini::Vector<Lifetime> v;

        v.reserve(3);

        assert(Lifetime::alive == 0);

        Lifetime& a = v.emplace_back(1);
        assert(a.id == 1);

        v.emplace_back(2);
        v.emplace_back(3);

        assert(v.size() == 3);
        assert(Lifetime::alive == 3);
        assert_lifetime_ids(v, {1, 2, 3});

        v.pop_back();

        assert(v.size() == 2);
        assert(Lifetime::alive == 2);
        assert_lifetime_ids(v, {1, 2});

        v.clear();

        assert(v.size() == 0);
        assert(Lifetime::alive == 0);

        v.emplace_back(4);

        assert(v.size() == 1);
        assert(Lifetime::alive == 1);
        assert(v[0].id == 4);
    }

    assert(Lifetime::alive == 0);
    assert(Lifetime::direct_ctor >= 4);
    assert(Lifetime::dtor >= 4);
}

std::cout << "[test 8] reserve reallocation preserves values\n";
{
    reset_lifetime_counts();

    {
        mini::Vector<Lifetime> v;

        v.emplace_back(10);
        v.emplace_back(20);
        v.emplace_back(30);

        Lifetime* old_data = v.data();
        std::size_t old_capacity = v.capacity();

        v.reserve(old_capacity + 10);

        assert(v.size() == 3);
        assert(v.capacity() >= old_capacity + 10);
        assert_lifetime_ids(v, {10, 20, 30});
        assert(Lifetime::alive == 3);

        if (old_data != nullptr) {
            assert(v.data() != old_data);
        }
    }

    assert(Lifetime::alive == 0);
}

std::cout << "[test 9] emplace perfect forwarding\n";
{
    mini::Vector<ForwardProbe> v;

    int x = 5;

    v.emplace_back(x);
    assert(v[0].tag == 1);

    v.emplace_back(5);
    assert(v[1].tag == 2);
}

std::cout << "[test 10] string values\n";
{
    mini::Vector<std::string> v;

    std::string s = "alpha";

    v.push_back(s);

    assert(v.size() == 1);
    assert(v[0] == "alpha");
    assert(s == "alpha");

    v.push_back(std::move(s));

    assert(v.size() == 2);
    assert(v[1] == "alpha");

    v.emplace_back(5, 'x');

    assert(v.size() == 3);
    assert(v[2] == "xxxxx");
}

std::cout << "[test 11] copy constructor deep copy\n";
{
    mini::Vector<int> a;

    for (int i = 0; i < 10; ++i) {
        a.emplace_back(i);
    }

    mini::Vector<int> b(a);

    assert_int_values(a, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    assert_int_values(b, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

    assert(a.data() != b.data());

    b[0] = 100;

    assert(a[0] == 0);
    assert(b[0] == 100);
}

std::cout << "[test 12] copy assignment cases\n";
{
    mini::Vector<int> empty_a;
    mini::Vector<int> empty_b;

    mini::Vector<int>& ret1 = (empty_a = empty_b);

    assert(&ret1 == &empty_a);
    assert(empty_a.empty());
    assert(empty_b.empty());

    mini::Vector<int> src;
    for (int i = 1; i <= 5; ++i) {
        src.emplace_back(i);
    }

    mini::Vector<int> dst;

    mini::Vector<int>& ret2 = (dst = src);

    assert(&ret2 == &dst);
    assert_int_values(src, {1, 2, 3, 4, 5});
    assert_int_values(dst, {1, 2, 3, 4, 5});
    assert(src.data() != dst.data());

    dst[0] = 99;
    assert(src[0] == 1);
    assert(dst[0] == 99);

    mini::Vector<int> already_has_value;
    already_has_value.emplace_back(-1);
    already_has_value.emplace_back(-2);

    already_has_value = src;

    assert_int_values(already_has_value, {1, 2, 3, 4, 5});

    already_has_value = empty_b;

    assert(already_has_value.empty());

    mini::Vector<int>& same = src;
    src = same;

    assert_int_values(src, {1, 2, 3, 4, 5});
}

std::cout << "[test 13] move constructor steals buffer\n";
{
    mini::Vector<int> a;

    for (int i = 0; i < 8; ++i) {
        a.emplace_back(i + 10);
    }

    int* old_data = a.data();
    std::size_t old_capacity = a.capacity();

    mini::Vector<int> b(std::move(a));

    assert_int_values(b, {10, 11, 12, 13, 14, 15, 16, 17});
    assert(b.data() == old_data);
    assert(b.capacity() == old_capacity);

    assert(a.size() == 0);
    assert(a.capacity() == 0);
    assert(a.data() == nullptr);
}

std::cout << "[test 14] move assignment cases\n";
{
    mini::Vector<int> src;

    for (int i = 0; i < 6; ++i) {
        src.emplace_back(i * 2);
    }

    int* old_data = src.data();
    std::size_t old_capacity = src.capacity();

    mini::Vector<int> dst;

    mini::Vector<int>& ret = (dst = std::move(src));

    assert(&ret == &dst);
    assert_int_values(dst, {0, 2, 4, 6, 8, 10});
    assert(dst.data() == old_data);
    assert(dst.capacity() == old_capacity);

    assert(src.size() == 0);
    assert(src.capacity() == 0);
    assert(src.data() == nullptr);

    mini::Vector<int> empty;
    dst = std::move(empty);

    assert(dst.size() == 0);
    assert(dst.capacity() == 0);
    assert(dst.data() == nullptr);

    mini::Vector<int> self;
    self.emplace_back(1);
    self.emplace_back(2);
    self.emplace_back(3);

    mini::Vector<int>& same = self;
    self = std::move(same);

    assert_int_values(self, {1, 2, 3});
}

std::cout << "[test 15] move-only type\n";
{
    reset_move_only_counts();

    {
        mini::Vector<MoveOnly> v;

        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);

        assert_move_only_ids(v, {1, 2, 3});
        assert(MoveOnly::alive == 3);

        std::size_t old_capacity = v.capacity();

        v.reserve(old_capacity + 10);

        assert_move_only_ids(v, {1, 2, 3});
        assert(v.capacity() >= old_capacity + 10);
        assert(MoveOnly::alive == 3);

        v.push_back(MoveOnly(4));

        assert_move_only_ids(v, {1, 2, 3, 4});
        assert(MoveOnly::alive == 4);

        mini::Vector<MoveOnly> moved(std::move(v));

        assert_move_only_ids(moved, {1, 2, 3, 4});

        assert(v.size() == 0);
        assert(v.capacity() == 0);
        assert(v.data() == nullptr);

        mini::Vector<MoveOnly> assigned;
        assigned = std::move(moved);

        assert_move_only_ids(assigned, {1, 2, 3, 4});

        assert(moved.size() == 0);
        assert(moved.capacity() == 0);
        assert(moved.data() == nullptr);
    }

    assert(MoveOnly::alive == 0);
}

std::cout << "[test 16] const access\n";
{
    mini::Vector<int> v;

    v.emplace_back(10);
    v.emplace_back(20);
    v.emplace_back(30);

    const mini::Vector<int>& cref = v;

    assert(cref.size() == 3);
    assert(!cref.empty());
    assert(cref[0] == 10);
    assert(cref.front() == 10);
    assert(cref.back() == 30);
    assert(cref.data() == v.data());
}

std::cout << "[test 17] alignment\n";
{
    mini::Vector<BigAligned> v;

    for (int i = 0; i < 8; ++i) {
        v.emplace_back(static_cast<long long>(i * 100));
    }

    for (std::size_t i = 0; i < v.size(); ++i) {
        auto addr = reinterpret_cast<std::uintptr_t>(&v[i]);

        assert(addr % alignof(BigAligned) == 0);
        assert(v[i].payload[0] == static_cast<long long>(i * 100));
        assert(v[i].payload[7] == static_cast<long long>(i * 100 + 7));
    }
}

std::cout << "[test 18] large growth\n";
{
    mini::Vector<int> v;

    constexpr int n = 10000;

    for (int i = 0; i < n; ++i) {
        v.emplace_back(i);
    }

    assert(v.size() == static_cast<std::size_t>(n));
    assert(v.capacity() >= v.size());

    for (int i = 0; i < n; ++i) {
        assert(v[static_cast<std::size_t>(i)] == i);
    }

    for (int i = 0; i < n; ++i) {
        v.pop_back();
    }

    assert(v.empty());
    assert(v.capacity() > 0);
}

std::cout << "[test 19] iterators begin/end/cbegin/cend\n";
{
    mini::Vector<int> v;

    v.emplace_back(10);
    v.emplace_back(20);
    v.emplace_back(30);

    assert(v.begin() == v.data());
    assert(v.end() == v.data() + v.size());
    assert(v.end() - v.begin() == static_cast<std::ptrdiff_t>(v.size()));

    assert(*v.begin() == 10);
    assert(*(v.end() - 1) == 30);

    *v.begin() = 99;
    assert(v[0] == 99);

    const mini::Vector<int>& cv = v;

    assert(cv.begin() == cv.data());
    assert(cv.end() == cv.data() + cv.size());
    assert(cv.cbegin() == cv.begin());
    assert(cv.cend() == cv.end());
    assert(cv.cend() - cv.cbegin() == static_cast<std::ptrdiff_t>(cv.size()));

    assert(*cv.cbegin() == 99);
    assert(*(cv.cend() - 1) == 30);
}

std::cout << "[test 20] swap\n";
{
    mini::Vector<int> a;
    mini::Vector<int> b;

    a.emplace_back(1);
    a.emplace_back(2);
    a.emplace_back(3);

    b.emplace_back(10);
    b.emplace_back(20);

    int* old_a_data = a.data();
    int* old_b_data = b.data();
    std::size_t old_a_capacity = a.capacity();
    std::size_t old_b_capacity = b.capacity();

    a.swap(b);

    assert_int_values(a, {10, 20});
    assert_int_values(b, {1, 2, 3});
    assert(a.data() == old_b_data);
    assert(b.data() == old_a_data);
    assert(a.capacity() == old_b_capacity);
    assert(b.capacity() == old_a_capacity);

    a.swap(a);

    assert_int_values(a, {10, 20});
    assert(a.data() == old_b_data);

    mini::Vector<int> empty;
    b.swap(empty);

    assert(b.empty());
    assert(b.data() == nullptr);
    assert_int_values(empty, {1, 2, 3});
    assert(empty.data() == old_a_data);
}

std::cout << "[test 21] resize int default/value/shrink\n";
{
    mini::Vector<int> v;

    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);

    std::size_t old_capacity = v.capacity();
    int* old_data = v.data();

    v.resize(1);

    assert_int_values(v, {1});
    assert(v.capacity() == old_capacity);
    assert(v.data() == old_data);

    v.resize(4);

    assert_int_values(v, {1, 0, 0, 0});
    assert(v.capacity() >= 4);

    v.resize(6, 7);

    assert_int_values(v, {1, 0, 0, 0, 7, 7});

    old_capacity = v.capacity();
    old_data = v.data();

    v.resize(0);

    assert(v.empty());
    assert(v.capacity() == old_capacity);
    assert(v.data() == old_data);

    v.resize(2, 9);

    assert_int_values(v, {9, 9});
}

std::cout << "[test 22] resize with lvalue/rvalue value\n";
{
    mini::Vector<std::string> v;

    std::string fill = "copy-me";

    v.resize(3, fill);

    assert(v.size() == 3);
    assert(v[0] == "copy-me");
    assert(v[1] == "copy-me");
    assert(v[2] == "copy-me");
    assert(fill == "copy-me");

    v.resize(5, std::string("temp"));

    assert(v.size() == 5);
    assert(v[0] == "copy-me");
    assert(v[1] == "copy-me");
    assert(v[2] == "copy-me");
    assert(v[3] == "temp");
    assert(v[4] == "temp");
}

std::cout << "[test 23] resize non-default-constructible with value\n";
{
    mini::Vector<NoDefault> v;
    NoDefault fill(42);

    v.resize(3, fill);

    assert(v.size() == 3);
    assert(v[0].value == 42);
    assert(v[1].value == 42);
    assert(v[2].value == 42);

    v.resize(1, fill);

    assert(v.size() == 1);
    assert(v[0].value == 42);
}

std::cout << "[test 24] resize lifetime shrink destroys removed elements\n";
{
    reset_lifetime_counts();

    {
        Lifetime fill(7);
        mini::Vector<Lifetime> v;

        assert(Lifetime::alive == 1);

        v.resize(3, fill);

        assert(v.size() == 3);
        assert_lifetime_ids(v, {7, 7, 7});
        assert(Lifetime::alive == 4);
        assert(Lifetime::copy_ctor >= 3);

        v.resize(1, fill);

        assert(v.size() == 1);
        assert_lifetime_ids(v, {7});
        assert(Lifetime::alive == 2);

        v.resize(0, fill);

        assert(v.empty());
        assert(Lifetime::alive == 1);
    }

    assert(Lifetime::alive == 0);
}

std::cout << "All vector tests passed\n";

}
