#pragma once

#include <algorithm>
#include <memory>
#include <simlib/inplace_buff.hh>
#include <type_traits>
#include <utility>

#if 0 // Clang does not support std::launder yet
#if __cplusplus > 201402L
#warning "Since C++17 std::launder() has to be used to wrap the reinterpret_casts below"
#endif
#endif

template <class T, size_t N>
class InplaceArray {
    size_t size_{0}, max_size_{};
    using Elem = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
    Elem a_[N];
    Elem* p_;

    [[nodiscard]] bool is_allocated() const noexcept { return p_ != a_; }

    void deallocate() noexcept {
        if (is_allocated()) {
            delete[] p_;
        }
    }

    void destruct_and_deallocate() noexcept {
        std::destroy(begin(), end());
        deallocate();
    }

public:
    InplaceArray() : max_size_(N), p_(a_) {}

    explicit InplaceArray(size_t n)
    : size_(n)
    , max_size_(std::max(n, N))
    , p_(n > N ? new Elem[n] : a_) {
        std::uninitialized_default_construct(begin(), end());
    }

    InplaceArray(size_t n, const T& val)
    : size_(n)
    , max_size_(std::max(n, N))
    , p_(n > N ? new Elem[n] : a_) {
        std::uninitialized_fill(begin(), end(), val);
    }

private:
    template <size_t N1>
    InplaceArray(std::in_place_t /*unused*/, const InplaceArray<T, N1>& a)
    : size_(a.size_)
    , max_size_(std::max(size_, N))
    , p_(size_ > N ? new Elem[size_] : a_) {
        std::uninitialized_copy(a.begin(), a.end(), begin());
    }

public:
    template <size_t N1>
    explicit InplaceArray(const InplaceArray<T, N1>& a) : InplaceArray(std::in_place, a) {}

    InplaceArray(const InplaceArray& a) : InplaceArray(std::in_place, a) {}

private:
    template <size_t N1>
    explicit InplaceArray(std::in_place_t /*unused*/, InplaceArray<T, N1>&& a)
    : size_(a.size_)
    , max_size_(std::max(size_, N)) {
        if (size_ <= N) {
            std::uninitialized_move(a.begin(), a.end(), begin());

        } else if (a.is_allocated()) { // move the pointer
            p_ = a.p_;
            a.p_ = a.a_;
            a.max_size_ = N1;

        } else { // allocate memory and then move
            p_ = new Elem[size_];
            std::uninitialized_move(a.begin(), a.end(), begin());
        }

        a.size_ = 0;
    }

public:
    template <size_t N1>
    explicit InplaceArray(InplaceArray<T, N1>&& a) : InplaceArray(std::in_place, std::move(a)) {}

    InplaceArray(InplaceArray&& a) noexcept : InplaceArray(std::in_place, std::move(a)) {}

    template <size_t N1>
    InplaceArray& operator=(const InplaceArray<T, N1>& a) {
        if (a.size_ <= N) {
            destruct_and_deallocate();
            p_ = a_;
            max_size_ = N;
            try {
                std::uninitialized_copy(a.begin(), a.end(), begin());
                size_ = a.size_;
            } catch (...) {
                size_ = 0;
                throw;
            }

        } else if (a.size_ <= size_) {
            // At first, potentially free memory
            if (size_ > a.size_) {
                std::destroy(begin() + a.size_, end());
                size_ = a.size_;
            }

            // It is OK to use std::copy as the objects in p_[0..a.size_] exist
            std::copy(a.begin(), a.end(), begin());

        } else {
            destruct_and_deallocate();
            try {
                p_ = new Elem[a.size_];
                max_size_ = size_ = a.size_;
                std::uninitialized_copy(a.begin(), a.end(), begin());
            } catch (...) {
                p_ = a_;
                size_ = 0;
                max_size_ = N;
                throw;
            }
        }

        return *this;
    }

    InplaceArray& operator=(const InplaceArray& a) {
        *this = InplaceArray(a);
        return *this;
    }

private:
    template <size_t N1>
    InplaceArray& assign(InplaceArray<T, N1>&& a) {
        if (a.size_ <= N) {
            destruct_and_deallocate();
            p_ = a_;
            max_size_ = N;
            try {
                std::uninitialized_move(a.begin(), a.end(), begin());
                size_ = a.size_;
            } catch (...) {
                size_ = 0;
                throw;
            }
        } else if (a.is_allocated()) {
            destruct_and_deallocate();
            p_ = a.p_;
            size_ = a.size_;
            max_size_ = a.max_size_;

            a.p_ = a.a_;
            a.max_size_ = N1;
        } else if (a.size_ <= size_) {
            // At first, potentially free memory
            if (size_ > a.size_) {
                std::destroy(begin() + a.size_, end());
                size_ = a.size_;
            }
            // It is OK to use std::move as the objects in p_[0..a.size_] exist
            std::move(a.begin(), a.end(), begin());
        } else {
            destruct_and_deallocate();
            try {
                p_ = new Elem[a.size_];
                std::uninitialized_move(a.begin(), a.end(), begin());
                max_size_ = size_ = a.size_;
            } catch (...) {
                p_ = a_;
                size_ = 0;
                max_size_ = N;
            }
        }

        a.size_ = 0;
        return *this;
    }

public:
    template <size_t N1>
    InplaceArray& operator=(InplaceArray<T, N1>&& a) {
        assign(std::in_place, std::move(a));
        return *this;
    }

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    InplaceArray& operator=(InplaceArray&& a) {
        if (&a != this) {
            assign(std::in_place, std::move(a));
        }
        return *this;
    }

    /**
     * @brief Increases array's max_size if needed, on reallcation all data is
     *   lost
     *
     * @param n lower bound on new capacity of the array
     */
    void lossy_reserve_for(size_t n) {
        if (n > max_size_) {
            size_t new_max_size = std::max(max_size_ << 1, n);
            auto new_p = std::make_unique<Elem[]>(new_max_size);
            deallocate();
            p_ = new_p.release();
            size_ = 0;
            max_size_ = new_max_size;
        }
    }

    /**
     * @brief Increases array's max_size if needed, preserves data
     *
     * @param n lower bound on new capacity of the array
     */
    void reserve_for(size_t n) {
        if (n > max_size_) {
            size_t new_max_size = std::max(max_size_ << 1, n);
            auto new_p = std::make_unique<Elem[]>(new_max_size);
            std::uninitialized_move(iterator(new_p.get()), iterator(new_p.get() + size_), begin());
            deallocate();
            p_ = new_p.release();
            max_size_ = new_max_size;
        }
    }

    /**
     * @brief Changes array's size and max_size if needed, preserves data
     *
     * @param n new array's size
     */
    void resize(size_t n) {
        reserve_for(n);
        std::uninitialized_default_construct(begin(), begin() + n);
        size_ = n;
    }

    /**
     * @brief Changes array's size and max_size if needed, preserves data
     *
     * @param n new array's size
     * @param val the value to which the new elements will be set
     */
    void resize(size_t n, const T& val) {
        reserve_for(n);

        if (n < size_) {
            std::destroy(begin() + n, end());
        } else {
            std::uninitialized_fill(end(), begin() + n, val);
        }
    }

    template <class... Args>
    T& emplace_back(Args&&... args) {
        reserve_for(size_ + 1);
        return *(::new (std::addressof((*this)[size_++])) T(std::forward<Args>(args)...));
    }

    void clear() noexcept { size_ = 0; }

    [[nodiscard]] size_t size() const noexcept { return size_; }

private:
    template <class Type>
    class Iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Type;
        using difference_type = std::ptrdiff_t;
        using pointer = Type*;
        using reference = Type&;

    private:
        Elem* p = nullptr;

        explicit Iterator(Elem* x) : p(x) {}

        friend class InplaceArray;

    public:
        Iterator() = default;

        reference operator*() const noexcept { return *reinterpret_cast<T*>(p); }

        pointer operator->() const noexcept { return reinterpret_cast<T*>(p); }

        Iterator& operator++() noexcept {
            ++p;
            return *this;
        }

        // NOLINTNEXTLINE(readability-const-return-type)
        const Iterator operator++(int) noexcept { return Iterator(p++); }

        Iterator& operator--() noexcept {
            --p;
            return *this;
        }

        // NOLINTNEXTLINE(readability-const-return-type)
        const Iterator operator--(int) noexcept { return Iterator(p++); }

        reference operator[](difference_type n) const noexcept {
            return *reinterpret_cast<T*>(p + n);
        }

        Iterator& operator+=(difference_type n) const noexcept {
            p += n;
            return *this;
        }

        Iterator operator+(difference_type n) const noexcept { return Iterator(p + n); }

        Iterator& operator-=(difference_type n) const noexcept {
            p -= n;
            return *this;
        }

        Iterator operator-(difference_type n) const noexcept { return Iterator(p - n); }

        friend bool operator==(Iterator a, Iterator b) noexcept { return (a.p == b.p); }

        friend bool operator!=(Iterator a, Iterator b) noexcept { return (a.p != b.p); }

        friend bool operator<(Iterator a, Iterator b) noexcept { return (a.p < b.p); }

        friend bool operator>(Iterator a, Iterator b) noexcept { return (a.p > b.p); }

        friend bool operator<=(Iterator a, Iterator b) noexcept { return (a.p <= b.p); }

        friend bool operator>=(Iterator a, Iterator b) noexcept { return (a.p >= b.p); }

        friend difference_type operator-(Iterator a, Iterator b) noexcept { return (a.p - b.p); }

        friend Iterator operator+(difference_type n, Iterator b) noexcept {
            return Iterator(n + b.p);
        }
    };

public:
    using iterator = Iterator<T>;
    using const_iterator = Iterator<const T>;

    static_assert(sizeof(Elem) == sizeof(T), "Needed by data()");

    T* data() noexcept { return std::addressof(front()); }

    [[nodiscard]] const T* data() const noexcept { return std::addressof(front()); }

    iterator begin() noexcept { return iterator(p_); }

    [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(p_); }

    iterator end() noexcept { return iterator(p_ + size_); }

    [[nodiscard]] const_iterator end() const noexcept { return const_iterator(p_ + size_); }

    T& front() noexcept { return begin()[0]; }

    [[nodiscard]] const T& front() const noexcept { return begin()[0]; }

    T& back() noexcept { return end()[-1]; }

    [[nodiscard]] const T& back() const noexcept { return end()[-1]; }

    T& operator[](size_t i) noexcept { return begin()[i]; }

    const T& operator[](size_t i) const noexcept { return begin()[i]; }

    ~InplaceArray() { destruct_and_deallocate(); }
};
