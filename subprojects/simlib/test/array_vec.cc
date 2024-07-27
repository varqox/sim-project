#include <cstdint>
#include <cstring>
#include <exception>
#include <gtest/gtest.h>
#include <limits>
#include <ostream>
#include <simlib/array_vec.hh>
#include <simlib/meta/max.hh>
#include <simlib/meta/min.hh>
#include <simlib/random.hh>
#include <stdexcept>
#include <utility>

using std::as_const;
using std::vector;

struct Metadata {
    int constructed = 0;
    int default_constructed = 0;
    int from_int_constructed = 0;
    int copy_constructed = 0;
    int move_constructed = 0;
    int assigned = 0;
    int copy_assigned = 0;
    int move_assigned = 0;
    int destructed = 0;
} metadata; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

struct Checker {
    int val = 0;

    Checker() noexcept {
        ++metadata.constructed;
        ++metadata.default_constructed;
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    Checker(int val) noexcept : val{val} {
        ++metadata.constructed;
        ++metadata.from_int_constructed;
    }

    Checker(const Checker& other) noexcept : val{other.val} {
        ++metadata.constructed;
        ++metadata.copy_constructed;
    }

    Checker(Checker&& other) noexcept : val{other.val} {
        ++metadata.constructed;
        ++metadata.move_constructed;
    }

    Checker& operator=(const Checker& other) noexcept {
        val = other.val;
        ++metadata.assigned;
        ++metadata.copy_assigned;
        return *this;
    }

    Checker& operator=(Checker&& other) noexcept {
        val = other.val;
        ++metadata.assigned;
        ++metadata.move_assigned;
        return *this;
    }

    ~Checker() { ++metadata.destructed; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator int() const noexcept { return val; }
};

template <class T>
vector<T> random_vector(
    size_t max_size,
    T min_val = std::numeric_limits<T>::min(),
    T max_val = std::numeric_limits<T>::max()
) {
    auto size = get_random<size_t>(0, max_size);
    vector<T> v;
    v.reserve(size);
    while (v.size() < size) {
        v.emplace_back(get_random(min_val, max_val));
    }
    return v;
}

template <class T, size_t max_size>
std::pair<vector<T>, ArrayVec<T, max_size>> random_vec_and_array_vec(
    T min_val = std::numeric_limits<T>::min(), T max_val = std::numeric_limits<T>::max()
) {
    auto vec = random_vector<T>(max_size, min_val, max_val);
    auto av = ArrayVec<T, max_size>::from_range(vec.begin(), vec.end());
    return {std::move(vec), std::move(av)};
}

namespace std {
template <class T> // NOLINTNEXTLINE(cert-dcl58-cpp)
ostream& operator<<(ostream& os, const vector<T>& v) {
    os << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        os << (i == 0 ? "" : ", ") << v[i];
    }
    return os << ']';
}
} // namespace std

// NOLINTNEXTLINE
TEST(ArrayVec, default_constructor) {
    metadata = {};
    {
        ArrayVec<Checker, 1> a;
    }
    ASSERT_EQ(metadata.constructed, 0);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 0);
}

// NOLINTNEXTLINE
TEST(ArrayVec, from_values_constructor_1) {
    metadata = {};
    {
        ArrayVec<Checker, 7> x{4};
        ASSERT_EQ(x.size(), 1);
        ASSERT_EQ(x[0], 4);
    }
    ASSERT_EQ(metadata.constructed, 1);
    ASSERT_EQ(metadata.from_int_constructed, 1);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 1);
}

// NOLINTNEXTLINE
TEST(ArrayVec, from_values_constructor_2) {
    metadata = {};
    {
        ArrayVec<Checker, 7> x{4, 7};
        ASSERT_EQ(x.size(), 2);
        ASSERT_EQ(x[0], 4);
        ASSERT_EQ(x[1], 7);
    }
    ASSERT_EQ(metadata.constructed, 2);
    ASSERT_EQ(metadata.from_int_constructed, 2);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 2);
}

// NOLINTNEXTLINE
TEST(ArrayVec, from_values_constructor_3) {
    metadata = {};
    {
        ArrayVec<Checker, 7> x{4, 7, 19};
        ASSERT_EQ(x.size(), 3);
        ASSERT_EQ(x[0], 4);
        ASSERT_EQ(x[1], 7);
        ASSERT_EQ(x[2], 19);
    }
    ASSERT_EQ(metadata.constructed, 3);
    ASSERT_EQ(metadata.from_int_constructed, 3);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 3);
}

// NOLINTNEXTLINE
TEST(ArrayVec, named_constructor_from_0) {
    metadata = {};
    auto x = ArrayVec<Checker, 2>::from();
    ASSERT_EQ(x.size(), 0);
    ASSERT_EQ(metadata.constructed, 0);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 0);
}

// NOLINTNEXTLINE
TEST(ArrayVec, named_constructor_from_1) {
    metadata = {};
    {
        auto x = ArrayVec<Checker, 7>::from(4);
        ASSERT_EQ(x.size(), 1);
        ASSERT_EQ(x[0], 4);
    }
    ASSERT_EQ(metadata.constructed, 1);
    ASSERT_EQ(metadata.from_int_constructed, 1);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 1);
}

// NOLINTNEXTLINE
TEST(ArrayVec, named_constructor_from_2) {
    metadata = {};
    {
        auto x = ArrayVec<Checker, 7>::from(4, 7);
        ASSERT_EQ(x.size(), 2);
        ASSERT_EQ(x[0], 4);
        ASSERT_EQ(x[1], 7);
    }
    ASSERT_EQ(metadata.constructed, 2);
    ASSERT_EQ(metadata.from_int_constructed, 2);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 2);
}

// NOLINTNEXTLINE
TEST(ArrayVec, named_constructor_from_3) {
    metadata = {};
    {
        auto x = ArrayVec<Checker, 7>::from(4, 7, 19);
        ASSERT_EQ(x.size(), 3);
        ASSERT_EQ(x[0], 4);
        ASSERT_EQ(x[1], 7);
        ASSERT_EQ(x[2], 19);
    }
    ASSERT_EQ(metadata.constructed, 3);
    ASSERT_EQ(metadata.from_int_constructed, 3);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 3);
}

// NOLINTNEXTLINE
TEST(ArrayVec, named_constructor_from_range) {
    for (int iter = 0; iter < 10; ++iter) {
        auto vec = random_vector<int>(6);
        metadata = {};
        {
            auto x = ArrayVec<Checker, 7>::from_range(vec.begin(), vec.end());
            ASSERT_EQ(x.size(), vec.size());
            for (size_t i = 0; i < vec.size(); ++i) {
                ASSERT_EQ(x[i], vec[i]);
            }
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, copy_constructor) {
    metadata = {};
    {
        ArrayVec<Checker, 6> x{4, 7, 8, 9};
        auto y = x; // NOLINT(performance-unnecessary-copy-initialization)
        ASSERT_EQ(x, y);
    }
    ASSERT_EQ(metadata.constructed, 8);
    ASSERT_EQ(metadata.copy_constructed, 4);
    ASSERT_EQ(metadata.move_constructed, 0);
    ASSERT_EQ(metadata.from_int_constructed, 4);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 8);
}

// NOLINTNEXTLINE
TEST(ArrayVec, move_constructor) {
    metadata = {};
    {
        ArrayVec<Checker, 6> x{4, 7, 8, 9};
        auto y = std::move(x);
        ASSERT_EQ(y, (ArrayVec{4, 7, 8, 9}));
    }
    ASSERT_EQ(metadata.constructed, 8);
    ASSERT_EQ(metadata.copy_constructed, 0);
    ASSERT_EQ(metadata.move_constructed, 4);
    ASSERT_EQ(metadata.from_int_constructed, 4);
    ASSERT_EQ(metadata.assigned, 0);
    ASSERT_EQ(metadata.destructed, 8);
}

// NOLINTNEXTLINE
TEST(ArrayVec, template_copy_constructor) {
    for (int iter = 0; iter < 100; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        // Into Checker
        metadata = {};
        {
            auto x = ArrayVec<int, 53>::from_range(vec.begin(), vec.end());
            ArrayVec<Checker, 45> y{x};
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
        // From Checker
        metadata = {};
        {
            auto x = ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end());
            ArrayVec<int, 45> y{x};
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
        // From Checker into Checker
        metadata = {};
        {
            auto x = ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end());
            ArrayVec<Checker, 45> y{x};
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size() * 2);
        ASSERT_EQ(metadata.copy_constructed, vec.size());
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size() * 2);
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, template_copy_constructor_terminates_if_other_is_too_big) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            auto other = ArrayVec<int, 5>{1, 2, 3, 4, 5};
            ArrayVec<int, 4> va{other};
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, template_move_constructor) {
    for (int iter = 0; iter < 100; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        // Into Checker
        metadata = {};
        {
            ArrayVec<Checker, 45> x{ArrayVec<int, 53>::from_range(vec.begin(), vec.end())};
            ASSERT_EQ(x, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
        // From Checker
        metadata = {};
        {
            ArrayVec<int, 45> x{ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end())};
            ASSERT_EQ(x, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
        // From Checker into Checker
        metadata = {};
        {
            ArrayVec<Checker, 45> x{ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end())};
            ASSERT_EQ(x, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size() * 2);
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, vec.size());
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size() * 2);
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, template_move_constructor_terminates_if_other_is_too_big) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] { ArrayVec<int, 4> va{ArrayVec<int, 5>{1, 2, 3, 4, 5}}; }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, template_copy_assignment) {
    for (int iter = 0; iter < 100; ++iter) {
        auto [prev_vec, prev_va] = random_vec_and_array_vec<int, 37>();
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        // Into Checker
        metadata = {};
        {
            ArrayVec<Checker, 45> y{prev_va};
            auto x = ArrayVec<int, 53>::from_range(vec.begin(), vec.end());
            y = x;
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(metadata.constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.copy_assigned, 0);
        ASSERT_EQ(metadata.move_assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.destructed, vec.size() + prev_vec.size());
        // From Checker
        metadata = {};
        {
            ArrayVec<int, 45> y{prev_va};
            auto x = ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end());
            y = x;
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
        // From Checker into Checker
        metadata = {};
        {
            ArrayVec<Checker, 45> y{prev_va};
            auto x = ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end());
            y = x;
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(
            metadata.constructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
        ASSERT_EQ(metadata.copy_constructed, vec.size() - std::min(vec.size(), prev_vec.size()));
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.copy_assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.move_assigned, 0);
        ASSERT_EQ(
            metadata.destructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, template_copy_assignment_terminates_if_other_is_too_big) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            auto other = ArrayVec<int, 5>{1, 2, 3, 4, 5};
            ArrayVec<int, 4> va;
            va = other;
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, template_move_assignment) {
    for (int iter = 0; iter < 100; ++iter) {
        auto [prev_vec, prev_va] = random_vec_and_array_vec<int, 37>();
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        // Into Checker
        metadata = {};
        {
            ArrayVec<Checker, 45> y{prev_va};
            y = ArrayVec<int, 53>::from_range(vec.begin(), vec.end());
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(metadata.constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.copy_assigned, 0);
        ASSERT_EQ(metadata.move_assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.destructed, vec.size() + prev_vec.size());
        // From Checker
        metadata = {};
        {
            ArrayVec<int, 45> y{prev_va};
            y = ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end());
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
        // From Checker into Checker
        metadata = {};
        {
            ArrayVec<Checker, 45> y{prev_va};
            y = ArrayVec<Checker, 53>::from_range(vec.begin(), vec.end());
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(
            metadata.constructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, vec.size() - std::min(vec.size(), prev_vec.size()));
        ASSERT_EQ(metadata.from_int_constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.copy_assigned, 0);
        ASSERT_EQ(metadata.move_assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(
            metadata.destructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, template_move_assignment_terminates_if_other_is_too_big) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 4> va;
            va = ArrayVec<int, 5>{1, 2, 3, 4, 5};
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, copy_assignment) {
    for (int iter = 0; iter < 100; ++iter) {
        auto [prev_vec, prev_va] = random_vec_and_array_vec<int, 37>();
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        metadata = {};
        {
            ArrayVec<Checker, 45> y{prev_va};
            auto x = decltype(y)::from_range(vec.begin(), vec.end());
            y = x;
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(
            metadata.constructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
        ASSERT_EQ(metadata.copy_constructed, vec.size() - std::min(vec.size(), prev_vec.size()));
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.copy_assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.move_assigned, 0);
        ASSERT_EQ(
            metadata.destructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, move_assignment) {
    for (int iter = 0; iter < 100; ++iter) {
        auto [prev_vec, prev_va] = random_vec_and_array_vec<int, 37>();
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        metadata = {};
        {
            ArrayVec<Checker, 45> y{prev_va};
            y = decltype(y)::from_range(vec.begin(), vec.end());
            ASSERT_EQ(y, va);
        }
        ASSERT_EQ(
            metadata.constructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, vec.size() - std::min(vec.size(), prev_vec.size()));
        ASSERT_EQ(metadata.from_int_constructed, prev_vec.size() + vec.size());
        ASSERT_EQ(metadata.assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(metadata.copy_assigned, 0);
        ASSERT_EQ(metadata.move_assigned, meta::min(prev_vec.size(), vec.size()));
        ASSERT_EQ(
            metadata.destructed,
            prev_vec.size() + vec.size() * 2 - std::min(vec.size(), prev_vec.size())
        );
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, is_empty) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        ASSERT_EQ(vec.empty(), va.is_empty());
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, size) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        ASSERT_EQ(vec.size(), va.size());
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, max_size) {
    ASSERT_EQ((ArrayVec<int, 1>{}.max_size()), 1);
    ASSERT_EQ((ArrayVec<int, 2>{}.max_size()), 2);
    ASSERT_EQ((ArrayVec<int, 3>{}.max_size()), 3);
    ASSERT_EQ((ArrayVec<int, 17>{}.max_size()), 17);
    ASSERT_EQ((ArrayVec<int, 4215>{}.max_size()), 4215);
}

// NOLINTNEXTLINE
TEST(ArrayVec, data) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<unsigned, 37>();
        EXPECT_EQ(vector<unsigned>(va.data(), va.data() + va.size()), vec);
        const auto& cva = va;
        EXPECT_EQ(vector<unsigned>(cva.data(), cva.data() + cva.size()), vec);
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, begin_end) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<unsigned, 37>();
        EXPECT_EQ(vector<unsigned>(va.begin(), va.end()), vec);
        const auto& cva = va;
        EXPECT_EQ(vector<unsigned>(cva.begin(), cva.end()), vec);
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, front) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<unsigned, 37>();
        if (!vec.empty()) {
            static_assert(std::is_same_v<decltype(va.front()), decltype(vec.front())>);
            EXPECT_EQ(va.front(), vec.front());
            static_assert(std::is_same_v<
                          decltype(as_const(va).front()),
                          decltype(as_const(vec).front())>);
            EXPECT_EQ(as_const(va).front(), vec.front());
        }
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, back) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<unsigned, 37>();
        if (!vec.empty()) {
            static_assert(std::is_same_v<decltype(va.back()), decltype(vec.back())>);
            EXPECT_EQ(va.back(), vec.back());
            static_assert(std::is_same_v<
                          decltype(as_const(va).back()),
                          decltype(as_const(vec).back())>);
            EXPECT_EQ(std::as_const(va).back(), vec.back());
        }
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, operator_index) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<unsigned, 37>();
        if (!vec.empty()) {
            for (int index_iter = 0; index_iter < 10; ++index_iter) {
                auto i = get_random<size_t>(0, vec.size() - 1);
                static_assert(std::is_same_v<decltype(va[i]), decltype(vec[i])>);
                EXPECT_EQ(va[i], vec[i]);
                static_assert(std::
                                  is_same_v<decltype(as_const(va)[i]), decltype(as_const(vec)[i])>);
                EXPECT_EQ(std::as_const(va)[i], vec[i]);
            }
        }
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, operator_index_out_of_range) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            (void)va[1];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            (void)va[2];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 17> va;
            (void)va[17];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 17> va;
            (void)va[1642];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, const_operator_index_out_of_range) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            const ArrayVec<int, 0> va;
            (void)va[1];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            const ArrayVec<int, 1> va;
            (void)va[2];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            const ArrayVec<int, 17> va;
            (void)va[17];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            const ArrayVec<int, 17> va;
            (void)va[1642];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, try_emplace) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        constexpr auto v_cap = decltype(va)::capacity / 2;
        metadata = {};
        {
            ArrayVec<Checker, v_cap> v;
            size_t i = 0;
            for (auto x : vec) {
                ASSERT_EQ(v.try_emplace(x), i < v_cap);
                ASSERT_EQ(v.size(), meta::min(i + 1, v_cap));
                ASSERT_EQ(
                    vector<int>(v.begin(), v.end()),
                    vector<int>(vec.begin(), vec.begin() + v.size())
                );
                ++i;
                ASSERT_EQ(metadata.from_int_constructed, meta::min(i, v_cap));
                ASSERT_EQ(metadata.destructed, 0);
            }
        }
        ASSERT_EQ(metadata.constructed, meta::min(vec.size(), v_cap));
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, meta::min(vec.size(), v_cap));
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, meta::min(vec.size(), v_cap));
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, emplace) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v;
            size_t i = 0;
            for (auto x : vec) {
                v.emplace(x);
                ASSERT_EQ(v.size(), i + 1);
                ASSERT_EQ(
                    vector<int>(v.begin(), v.end()),
                    vector<int>(vec.begin(), vec.begin() + v.size())
                );
                ++i;
                ASSERT_EQ(metadata.from_int_constructed, i);
                ASSERT_EQ(metadata.destructed, 0);
            }
        }
        ASSERT_EQ(metadata.constructed, vec.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size());
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, emplace_terminates_if_container_is_full) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            va.emplace(4);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            va.emplace(4);
            va.emplace(8);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 2> va;
            va.emplace(4);
            va.emplace(8);
            va.emplace(3);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 3> va;
            va.emplace(4);
            va.emplace(8);
            va.emplace(3);
            va.emplace(5);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, try_push) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        constexpr auto v_cap = decltype(va)::capacity / 2;
        // try_push from const-ref
        metadata = {};
        {
            ArrayVec<Checker, v_cap> v;
            size_t i = 0;
            for (auto x : vec) {
                Checker y{x};
                ASSERT_EQ(v.try_push(y), i < v_cap);
                ASSERT_EQ(v.size(), meta::min(i + 1, v_cap));
                ASSERT_EQ(
                    vector<int>(v.begin(), v.end()),
                    vector<int>(vec.begin(), vec.begin() + v.size())
                );
                ++i;
                ASSERT_EQ(metadata.copy_constructed, meta::min(i, v_cap));
                ASSERT_EQ(metadata.from_int_constructed, i);
                ASSERT_EQ(metadata.destructed, i - 1);
            }
        }
        ASSERT_EQ(metadata.constructed, vec.size() + meta::min(vec.size(), v_cap));
        ASSERT_EQ(metadata.copy_constructed, meta::min(vec.size(), v_cap));
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size() + meta::min(vec.size(), v_cap));
        // try_push from x-value
        metadata = {};
        {
            ArrayVec<Checker, v_cap> v;
            size_t i = 0;
            for (auto x : vec) {
                ASSERT_EQ(v.try_push(Checker{x}), i < v_cap);
                ASSERT_EQ(v.size(), meta::min(i + 1, v_cap));
                ASSERT_EQ(
                    vector<int>(v.begin(), v.end()),
                    vector<int>(vec.begin(), vec.begin() + v.size())
                );
                ++i;
                ASSERT_EQ(metadata.move_constructed, meta::min(i, v_cap));
                ASSERT_EQ(metadata.from_int_constructed, i);
                ASSERT_EQ(metadata.destructed, i);
            }
        }
        ASSERT_EQ(metadata.constructed, vec.size() + meta::min(vec.size(), v_cap));
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, meta::min(vec.size(), v_cap));
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size() + meta::min(vec.size(), v_cap));
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, push) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        // push from const-ref
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v;
            size_t i = 0;
            for (auto x : vec) {
                Checker y{x};
                v.emplace(y);
                ASSERT_EQ(v.size(), i + 1);
                ASSERT_EQ(
                    vector<int>(v.begin(), v.end()),
                    vector<int>(vec.begin(), vec.begin() + v.size())
                );
                ++i;
                ASSERT_EQ(metadata.copy_constructed, i);
                ASSERT_EQ(metadata.from_int_constructed, i);
                ASSERT_EQ(metadata.destructed, i - 1);
            }
        }
        ASSERT_EQ(metadata.constructed, vec.size() * 2);
        ASSERT_EQ(metadata.copy_constructed, vec.size());
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size() * 2);
        // push from x-value
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v;
            size_t i = 0;
            for (auto x : vec) {
                v.emplace(Checker{x});
                ASSERT_EQ(v.size(), i + 1);
                ASSERT_EQ(
                    vector<int>(v.begin(), v.end()),
                    vector<int>(vec.begin(), vec.begin() + v.size())
                );
                ++i;
                ASSERT_EQ(metadata.move_constructed, i);
                ASSERT_EQ(metadata.from_int_constructed, i);
                ASSERT_EQ(metadata.destructed, i);
            }
        }
        ASSERT_EQ(metadata.constructed, vec.size() * 2);
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, vec.size());
        ASSERT_EQ(metadata.from_int_constructed, vec.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, vec.size() * 2);
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, push_terminates_if_container_is_full) {
    // push from const-ref
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            int x = 4;
            va.push(x);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            va.push(4);
            int x = 8;
            va.push(x);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 2> va;
            va.push(4);
            va.push(8);
            int x = 3;
            va.push(x);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 3> va;
            va.push(4);
            va.push(8);
            va.push(3);
            int x = 5;
            va.push(x);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // push from x-value
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            va.push(4);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            va.push(4);
            va.push(8);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 2> va;
            va.push(4);
            va.push(8);
            va.push(3);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 3> va;
            va.push(4);
            va.push(8);
            va.push(3);
            va.push(5);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, append) {
    for (int iter = 0; iter < 10; ++iter) {
        static constexpr auto N = 37;
        auto v1 = random_vector<int>(N);
        auto v2 = random_vector<int>(N);
        if (v1.size() + v2.size() <= N) {
            metadata = {};
            {
                auto va = ArrayVec<Checker, N>::from_range(v1.begin(), v1.end());
                va.append(v2.begin(), v2.size());
                auto v = v1;
                v.insert(v.end(), v2.begin(), v2.end());
                ASSERT_EQ(v, vector<int>(va.begin(), va.end()));
                ASSERT_EQ(metadata.from_int_constructed, v1.size() + v2.size());
            }
            ASSERT_EQ(metadata.constructed, v1.size() + v2.size());
            ASSERT_EQ(metadata.default_constructed, 0);
            ASSERT_EQ(metadata.copy_constructed, 0);
            ASSERT_EQ(metadata.move_constructed, 0);
            ASSERT_EQ(metadata.from_int_constructed, v1.size() + v2.size());
            ASSERT_EQ(metadata.assigned, 0);
            ASSERT_EQ(metadata.destructed, v1.size() + v2.size());
        }
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, append_terminates_if_container_would_overflow) {
    for (int iter = 0; iter < 10; ++iter) {
        static constexpr auto N = 37;
        auto v1 = random_vector<int>(N);
        auto v2 = random_vector<int>(N);
        if (v1.size() + v2.size() > N) {
            // NOLINTNEXTLINE
            EXPECT_EXIT(
                ([&] {
                    auto va = ArrayVec<int, N>::from_range(v1.begin(), v1.end());
                    va.append(v2.begin(), v2.size());
                }()),
                testing::KilledBySignal(SIGABRT),
                ""
            );
        }
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, pop) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v{va};
            while (!vec.empty()) {
                vec.pop_back();
                v.pop();
                ASSERT_EQ(vec.size(), v.size());
                ASSERT_EQ(vec, vector<int>(v.begin(), v.end()));
                ASSERT_EQ(metadata.destructed, va.size() - v.size());
            }
        }
        ASSERT_EQ(metadata.constructed, va.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, va.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, va.size());
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, pop_terminates_if_container_is_empty) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            va.pop();
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            va.pop();
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 17> va;
            va.pop();
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, pop_value) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v{va};
            while (!vec.empty()) {
                auto val = v.pop_value();
                ASSERT_EQ(val, vec.back());
                vec.pop_back();
                ASSERT_EQ(vec.size(), v.size());
                ASSERT_EQ(vec, vector<int>(v.begin(), v.end()));
                ASSERT_EQ(metadata.move_constructed, va.size() - v.size());
                ASSERT_EQ(metadata.destructed, 2 * (va.size() - v.size()) - 1);
            }
        }
        ASSERT_EQ(metadata.constructed, va.size() * 2);
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, va.size());
        ASSERT_EQ(metadata.from_int_constructed, va.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, va.size() * 2);
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, pop_value_terminates_if_container_is_empty) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            (void)va.pop_value();
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            (void)va.pop_value();
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 17> va;
            (void)va.pop_value();
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, clear) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v{va};
            v.clear();
            ASSERT_EQ(metadata.destructed, va.size());
        }
        ASSERT_EQ(metadata.constructed, va.size());
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, va.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, va.size());
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, resize) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        int old_size = static_cast<int>(va.size());
        int new_size = get_random<int>(0, 37);
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v{va};
            v.resize(new_size);
            ASSERT_EQ(metadata.default_constructed, meta::max(new_size - old_size, 0));
            ASSERT_EQ(metadata.destructed, meta::max(old_size - new_size, 0));
            for (int i = 0; i < meta::min(old_size, new_size); ++i) {
                ASSERT_EQ(v[i], va[i]);
            }
            for (int i = old_size; i < new_size; ++i) {
                ASSERT_EQ(v[i], 0);
            }
        }
        ASSERT_EQ(metadata.constructed, va.size() + meta::max(new_size - old_size, 0));
        ASSERT_EQ(metadata.default_constructed, meta::max(new_size - old_size, 0));
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, va.size());
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, va.size() + meta::max(new_size - old_size, 0));
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, resize_with_value) {
    for (int iter = 0; iter < 10; ++iter) {
        auto [vec, va] = random_vec_and_array_vec<int, 37>();
        int old_size = static_cast<int>(va.size());
        int new_size = get_random<int>(0, 37);
        // value is a constructor argument
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v{va};
            v.resize(new_size, 42);
            ASSERT_EQ(metadata.from_int_constructed, va.size() + meta::max(new_size - old_size, 0));
            ASSERT_EQ(metadata.destructed, meta::max(old_size - new_size, 0));
            for (int i = 0; i < meta::min(old_size, new_size); ++i) {
                ASSERT_EQ(v[i], va[i]);
            }
            for (int i = old_size; i < new_size; ++i) {
                ASSERT_EQ(v[i], 42);
            }
        }
        ASSERT_EQ(metadata.constructed, va.size() + meta::max(new_size - old_size, 0));
        ASSERT_EQ(metadata.default_constructed, 0);
        ASSERT_EQ(metadata.copy_constructed, 0);
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, va.size() + meta::max(new_size - old_size, 0));
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, va.size() + meta::max(new_size - old_size, 0));
        // value is constructed value_type
        metadata = {};
        {
            ArrayVec<Checker, decltype(va)::capacity> v{va};
            v.resize(new_size, Checker{42});
            ASSERT_EQ(metadata.copy_constructed, meta::max(new_size - old_size, 0));
            ASSERT_EQ(metadata.destructed, meta::max(old_size - new_size, 0) + 1);
        }
        ASSERT_EQ(metadata.constructed, va.size() + 1 + meta::max(new_size - old_size, 0));
        ASSERT_EQ(metadata.default_constructed, 0);
        ASSERT_EQ(metadata.copy_constructed, meta::max(new_size - old_size, 0));
        ASSERT_EQ(metadata.move_constructed, 0);
        ASSERT_EQ(metadata.from_int_constructed, va.size() + 1);
        ASSERT_EQ(metadata.assigned, 0);
        ASSERT_EQ(metadata.destructed, va.size() + meta::max(new_size - old_size, 0) + 1);
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec_DeathTest, resize_terminates_if_requested_size_is_greater_than_max_size) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            va.resize(1);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            va.resize(10);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 17> va;
            va.resize(18);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 0> va;
            va.resize(1, 5);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 1> va;
            va.resize(10, 5);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            ArrayVec<int, 17> va;
            va.resize(18, 5);
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(ArrayVec, deduction_guides) {
    auto x = ArrayVec{1};
    static_assert(std::is_same_v<decltype(x)::value_type, int>);
    static_assert(decltype(x)::capacity == 1);
    ASSERT_EQ(x.max_size(), 1);
    auto y = ArrayVec{1.0, 2.0, 3.0};
    static_assert(std::is_same_v<decltype(y)::value_type, double>);
    static_assert(decltype(y)::capacity == 3);
    ASSERT_EQ(y.max_size(), 3);
}

// NOLINTNEXTLINE
TEST(ArrayVec, comparison_operators) {
    for (int iter = 0; iter < 1000; ++iter) {
        auto [vec1, va1] = random_vec_and_array_vec<int, 6>(4);
        auto [vec2, va2] = random_vec_and_array_vec<int, 6>(4);
        ASSERT_EQ(vec1 == vec2, va1 == va2) << "vec1: " << vec1 << " vec2: " << vec2;
        ASSERT_EQ(vec1 != vec2, va1 != va2) << "vec1: " << vec1 << " vec2: " << vec2;
        ASSERT_EQ(vec1 < vec2, va1 < va2) << "vec1: " << vec1 << " vec2: " << vec2;
        ASSERT_EQ(vec1 > vec2, va1 > va2) << "vec1: " << vec1 << " vec2: " << vec2;
        ASSERT_EQ(vec1 <= vec2, va1 <= va2) << "vec1: " << vec1 << " vec2: " << vec2;
        ASSERT_EQ(vec1 >= vec2, va1 >= va2) << "vec1: " << vec1 << " vec2: " << vec2;
    }
}

// NOLINTNEXTLINE
TEST(ArrayVec, used_as_buffer) {
    uint32_t x = 0xdeadbeef;
    uint16_t y = 2137;
    ArrayVec<std::byte, sizeof(x) + sizeof(y)> buff;
    buff.append(reinterpret_cast<std::byte*>(&x), sizeof(x));
    buff.append(reinterpret_cast<std::byte*>(&y), sizeof(y));
    decltype(x) xd;
    std::memcpy(&xd, buff.data(), sizeof(xd));
    decltype(y) yd;
    std::memcpy(&yd, buff.data() + sizeof(xd), sizeof(yd));
    ASSERT_EQ(x, xd);
    ASSERT_EQ(y, yd);
}
