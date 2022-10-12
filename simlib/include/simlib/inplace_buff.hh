#pragma once

#include "simlib/concat_common.hh"
#include "simlib/string_view.hh"

#include <utility>

class InplaceBuffBase {
public:
    size_t size = 0;

protected:
    size_t max_size_;
    char* p_;
    char* p_value_when_unallocated_;

public:
    void make_copy_of(const char* data, size_t len) {
        if (len > max_size_) {
            auto new_p = new char[len];
            deallocate();
            p_ = new_p;
            max_size_ = len;
        }
        size = len;
        std::copy(data, data + len, p_);
    }

    /**
     * @brief Changes buffer's size and max_size if needed, but in the latter
     *   case, or if an exception is thrown all the data are lost
     *
     * @param n new buffer's size
     */
    void lossy_resize(size_t n) {
        if (n > max_size_) {
            auto new_max_size = std::max(max_size_ << 1, n);
            deallocate();
            p_ = new char[new_max_size];
            max_size_ = new_max_size;
        }
        size = n;
    }

    /**
     * @brief Changes buffer's size and max_size if needed, preserves data
     *
     * @param n new buffer's size
     */
    constexpr void resize(size_t n) {
        if (n > max_size_) {
            size_t new_max_size = std::max(max_size_ << 1, n);
            char* new_p = new char[new_max_size];
            std::copy(p_, p_ + size, new_p);
            deallocate();
            p_ = new_p;
            max_size_ = new_max_size;
        }
        size = n;
    }

    constexpr void clear() noexcept { size = 0; }

    constexpr char* begin() noexcept { return data(); }

    [[nodiscard]] constexpr const char* begin() const noexcept { return data(); }

    constexpr char* end() noexcept { return data() + size; }

    [[nodiscard]] constexpr const char* end() const noexcept { return data() + size; }

    [[nodiscard]] constexpr const char* cbegin() const noexcept { return data(); }

    [[nodiscard]] constexpr const char* cend() const noexcept { return data() + size; }

    constexpr char* data() noexcept { return p_; }

    [[nodiscard]] constexpr const char* data() const noexcept { return p_; }

    [[nodiscard]] constexpr size_t max_size() const noexcept { return max_size_; }

    constexpr char& front() noexcept { return (*this)[0]; }

    [[nodiscard]] constexpr char front() const noexcept { return (*this)[0]; }

    constexpr char& back() noexcept { return (*this)[size - 1]; }

    [[nodiscard]] constexpr char back() const noexcept { return (*this)[size - 1]; }

    constexpr char& operator[](size_t i) noexcept { return p_[i]; }

    constexpr char operator[](size_t i) const noexcept { return p_[i]; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator StringView() const& noexcept { return {data(), size}; }

    // Do not allow to create StringView from a temporary object
    constexpr operator StringView() const&& = delete;

    // Do not allow to create StringBase<char> from a temporary object
    // NOLINTNEXTLINE(google-explicit-constructor)
    operator StringBase<char>() & noexcept { return {data(), size}; }

    operator StringBase<char>() && = delete;

    [[nodiscard]] std::string to_string() const { return {data(), size}; }

    constexpr CStringView to_cstr() & {
        resize(size + 1);
        (*this)[--size] = '\0';
        return {data(), size};
    }

    // Do not allow to create CStringView of a temporary object
    constexpr CStringView to_cstr() && = delete;

protected:
    constexpr InplaceBuffBase(
            size_t s, size_t max_s, char* p, char* p_value_when_unallocated) noexcept
    : size(s)
    , max_size_(max_s)
    , p_(p)
    , p_value_when_unallocated_(p_value_when_unallocated) {}

    InplaceBuffBase(const InplaceBuffBase&) noexcept = default;
    InplaceBuffBase(InplaceBuffBase&&) noexcept = default;
    InplaceBuffBase& operator=(const InplaceBuffBase&) noexcept = default;
    InplaceBuffBase& operator=(InplaceBuffBase&&) noexcept = default;

    [[nodiscard]] bool is_allocated() const noexcept { return (p_ != p_value_when_unallocated_); }

    void deallocate() noexcept {
        if (is_allocated()) {
            delete[] p_;
            p_ = p_value_when_unallocated_;
            max_size_ = 0;
        }
    }

public:
    virtual ~InplaceBuffBase() { deallocate(); }

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    constexpr InplaceBuffBase& append(Args&&... args) {
        [this](auto&&... str) {
            size_t pos = size;
            size_t final_len = (size + ... + string_length(str));
            resize(final_len);
            auto append_impl = [&](auto&& s) {
                auto slen = string_length(s);
                auto sdata = ::data(s);
                std::copy(sdata, sdata + slen, data() + pos);
                pos += slen;
            };
            (void)append_impl; // Fix GCC warning
            (append_impl(std::forward<decltype(str)>(str)), ...);
        }(stringify(std::forward<Args>(args))...);
        return *this;
    }
};

template <size_t N>
class InplaceBuff : public InplaceBuffBase {
private:
    std::array<char, N> a_;

    template <size_t M>
    friend class InplaceBuff;

public:
    static constexpr size_t max_static_size = N;

    constexpr InplaceBuff() noexcept
    : InplaceBuffBase(0, N, nullptr, nullptr) {
        // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
        p_value_when_unallocated_ =
                a_.data(); // a_ is uninitialized in the call to InplaceBuffBase()
        p_ = p_value_when_unallocated_;
    }

    constexpr explicit InplaceBuff(size_t n)
    : InplaceBuffBase(n, std::max(N, n), nullptr, nullptr) {
        // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
        p_value_when_unallocated_ =
                a_.data(); // a_ is uninitialized in the call to InplaceBuffBase()
        p_ = (n <= N ? p_value_when_unallocated_ : new char[n]);
    }

    constexpr InplaceBuff(const InplaceBuff& ibuff)
    : InplaceBuff(ibuff.size) {
        std::copy(ibuff.data(), ibuff.data() + ibuff.size, data());
    }

    InplaceBuff(InplaceBuff&& ibuff) noexcept
    : InplaceBuff() {
        if (ibuff.is_allocated()) {
            // Steal the allocated string
            p_ = std::exchange(ibuff.p_, ibuff.p_value_when_unallocated_);
            size = std::exchange(ibuff.size, 0);
            max_size_ = std::exchange(ibuff.max_size_, N);
        } else {
            make_copy_of(ibuff.data(), ibuff.size);
        }
    }

    template <class T,
            std::enable_if_t<not std::is_integral_v<std::decay_t<T>> and
                            not std::is_same_v<std::decay_t<T>, InplaceBuff>,
                    int> = 0>
    // NOLINTNEXTLINE(bugprone-forwarding-reference-overload): see enable_if
    constexpr explicit InplaceBuff(T&& str)
    : InplaceBuff(string_length(str)) {
        std::copy(::data(str), ::data(str) + size, p_);
    }

    // Used to unify constructors, as constructor taking one argument is
    // explicit
    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    constexpr explicit InplaceBuff(std::in_place_t /*unused*/, Args&&... args)
    : InplaceBuff() {
        append(std::forward<Args>(args)...);
    }

    template <class Arg1, class Arg2, class... Args,
            std::enable_if_t<not std::is_same_v<std::remove_cv_t<std::remove_reference_t<Arg1>>,
                                     std::in_place_t>,
                    int> = 0>
    constexpr InplaceBuff(Arg1&& arg1, Arg2&& arg2, Args&&... args)
    : InplaceBuff(std::in_place, std::forward<Arg1>(arg1), std::forward<Arg2>(arg2),
              std::forward<Args>(args)...) {}

    template <size_t M, std::enable_if_t<M != N, int> = 0>
    explicit InplaceBuff(InplaceBuff<M>&& ibuff) noexcept
    : InplaceBuff() {
        if (ibuff.is_allocated() and ibuff.size > N) {
            // Steal the allocated string
            p_ = std::exchange(ibuff.p_, ibuff.p_value_when_unallocated_);
            size = std::exchange(ibuff.size, 0);
            max_size_ = std::exchange(ibuff.max_size_, M);
        } else {
            make_copy_of(ibuff.data(), ibuff.size);
        }
    }

    ~InplaceBuff() override = default;

    constexpr InplaceBuff& operator=(const InplaceBuff& ibuff) {
        if (this == &ibuff) {
            return *this;
        }
        make_copy_of(ibuff.data(), ibuff.size);
        return *this;
    }

    constexpr InplaceBuff& operator=(InplaceBuff&& ibuff) noexcept {
        assign_move_impl(std::move(ibuff));
        return *this;
    }

    template <class T, std::enable_if_t<not std::is_same_v<std::decay_t<T>, InplaceBuff>, int> = 0>
    // NOLINTNEXTLINE(misc-unconventional-assign-operator)
    constexpr InplaceBuff& operator=(T&& str) {
        make_copy_of(::data(str), string_length(str));
        return *this;
    }

    template <size_t M, std::enable_if_t<M != N, int> = 0>
    constexpr InplaceBuff& operator=(InplaceBuff<M>&& ibuff) noexcept {
        assign_move_impl(std::move(ibuff));
        return *this;
    }

private:
    template <size_t M>
    InplaceBuff& assign_move_impl(InplaceBuff<M>&& ibuff) {
        if (ibuff.is_allocated() and ibuff.max_size() >= max_size()) {
            // Steal the allocated string
            deallocate();
            p_ = std::exchange(ibuff.p_, ibuff.p_value_when_unallocated_);
            size = std::exchange(ibuff.size, 0);
            max_size_ = std::exchange(ibuff.max_size_, M);
        } else {
            make_copy_of(ibuff.data(), ibuff.size);
        }
        return *this;
    }
};

template <size_t N>
std::string& operator+=(std::string& str, const InplaceBuff<N>& ibs) {
    return str.append(ibs.data(), ibs.size);
}

// This function allows @p str to be converted to StringView, but
// keep in mind that if any StringView or alike value generated from the result
// of this function cannot be saved to a variable! -- it would (and probably
// will) cause use-after-free error, because @p str will be already deleted when
// the initialization is done
template <size_t N>
constexpr StringView intentional_unsafe_string_view(const InplaceBuff<N>&& str) noexcept {
    return StringView(static_cast<const InplaceBuff<N>&>(str));
}

// This function allows @p str to be converted to CStringView, but
// keep in mind that if any StringView or alike value generated from the result
// of this function cannot be saved to a variable! -- it would (and probably
// will) cause use-after-free error, because @p str will be already deleted when
// the initialization is done
template <size_t N>
constexpr CStringView intentional_unsafe_cstring_view(InplaceBuff<N>&& str) noexcept {
    return static_cast<InplaceBuff<N>&>(str).to_cstr();
}

template <size_t T>
constexpr auto string_length(const InplaceBuff<T>& buff) -> decltype(buff.size) {
    return buff.size;
}

template <size_t N, class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
InplaceBuff<N>& back_insert(InplaceBuff<N>& buff, Args&&... args) {
    buff.append(std::forward<Args>(args)...);
    return buff;
}
