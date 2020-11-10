#pragma once

#include <functional>
#include <type_traits>
#include <utility>

template <class T, class Class, T Class::*member, class Comp = std::less<T>>
class MemberComparator {
    Comp compare;

public:
    MemberComparator() = default;
    MemberComparator(const MemberComparator&) = default;
    MemberComparator(MemberComparator&&) noexcept = default;
    MemberComparator& operator=(const MemberComparator&) = default;
    MemberComparator& operator=(MemberComparator&&) noexcept = default;
    ~MemberComparator() = default;

    template <class... Args, std::enable_if_t<std::is_constructible_v<Comp, Args...>, int> = 0>
    explicit MemberComparator(std::in_place_t /*unused*/, Args&&... args)
    : compare(std::forward<Args>(args)...) {}

    bool operator()(const Class& a, const Class& b) const {
        return compare(a.*member, b.*member);
    }
};

#define MEMBER_COMPARATOR(Class, member) \
    MemberComparator<decltype(Class::member), Class, &Class::member>

template <class T, class Class, T Class::*member, class Comp = std::less<>>
class TransparentMemberComparator {
    Comp compare;

    template <class U>
    [[nodiscard]] decltype(auto) value(U&& x) const noexcept {
        if constexpr (std::is_same_v<std::decay_t<U>, Class>) {
            return x.*member;
        } else {
            return std::forward<U>(x);
        }
    }

public:
    struct is_transparent;

    TransparentMemberComparator() = default;
    TransparentMemberComparator(const TransparentMemberComparator&) = default;
    TransparentMemberComparator(TransparentMemberComparator&&) noexcept = default;
    TransparentMemberComparator& operator=(const TransparentMemberComparator&) = default;
    TransparentMemberComparator& operator=(TransparentMemberComparator&&) noexcept = default;
    ~TransparentMemberComparator() = default;

    template <class... Args, std::enable_if_t<std::is_constructible_v<Comp, Args...>, int> = 0>
    explicit TransparentMemberComparator(std::in_place_t /*unused*/, Args&&... args)
    : compare(std::forward<Args>(args)...) {}

    template <class A, class B>
    bool operator()(A&& a, B&& b) const {
        return compare(value(std::forward<A>(a)), value(std::forward<B>(b)));
    }
};

#define TRANSPARENT_MEMBER_COMPARATOR(Class, member) \
    TransparentMemberComparator<decltype(Class::member), Class, &Class::member>
