#include <array>
#include <initializer_list>
#include <simlib/contains.hh>

using std::array;
using std::initializer_list;

int main() {
    static_assert(!contains(array<int, 0>{}, 42));
    static_assert(contains(array{1, 2, 3}, 1));
    static_assert(contains(array{1, 2, 3}, 2));
    static_assert(contains(array{1, 2, 3}, 3));
    static_assert(!contains(array{1, 2, 3}, 4));
    static_assert(!contains(array{2, 3}, 1));

    static_assert(!contains(initializer_list<int>{}, 42));
    static_assert(contains(initializer_list<int>{1, 2, 3}, 1));
    static_assert(contains(initializer_list<int>{1, 2, 3}, 2));
    static_assert(contains(initializer_list<int>{1, 2, 3}, 3));
    static_assert(!contains(initializer_list<int>{1, 2, 3}, 4));
    static_assert(!contains(initializer_list<int>{2, 3}, 1));
}
