#pragma once

#include "to_string.hh"

#include <cstring>
#include <memory>

#ifdef _GLIBCXX_DEBUG
#include <cassert>
#endif

template <class Char>
class StringBase {
public:
	// Types
	using value_type = Char;
	using const_reference = const Char&;
	using reference = Char&;
	using pointer = Char*;
	using const_pointer = const Char*;
	using const_iterator = const_pointer;
	using iterator = const_iterator;
	using size_type = size_t;

	static constexpr size_type npos = -1;

protected:
	pointer str;
	size_type len;

public:
	constexpr StringBase() noexcept : str(""), len(0) {}

	constexpr StringBase(std::nullptr_t) = delete;

	template <size_t N>
	constexpr StringBase(const char (&s)[N])
	   : str(s), len(std::char_traits<char>::length(s)) {}

	// Do not treat as possible string literal
	template <size_t N>
	constexpr StringBase(char (&s)[N])
	   : str(s), len(std::char_traits<char>::length(s)) {}

	template <size_t N>
	constexpr StringBase(const StaticCStringBuff<N>& s) noexcept
	   : str(s.data()), len(s.size()) {}

	constexpr StringBase(pointer s) noexcept
	   : str(s), len(std::char_traits<char>::length(s)) {}

	constexpr StringBase(pointer s, size_type n) noexcept : str(s), len(n) {}

	constexpr StringBase(std::string&&) noexcept = delete;

	constexpr StringBase(const std::string&&) noexcept = delete;

	// Constructs StringView from substring [beg, beg + n) of string s
	constexpr StringBase(const std::string& s, size_type beg = 0,
	                     size_type n = npos) noexcept
	   : str(s.data() + std::min(beg, s.size())),
	     len(std::min(n, s.size() - std::min(beg, s.size()))) {}

	// Constructs StringBase from substring [beg, beg + n) of string s
	constexpr StringBase(std::string& s, size_type beg = 0,
	                     size_type n = npos) noexcept
	   : str(s.data() + std::min(beg, s.size())),
	     len(std::min(n, s.size() - std::min(beg, s.size()))) {}

	constexpr StringBase(const StringBase& s) noexcept
	   : str(s.data()), len(s.size()) {}

	constexpr StringBase(StringBase&& s) noexcept
	   : str(s.data()), len(s.size()) {}

	constexpr StringBase& operator=(StringBase&& s) noexcept {
		str = s.data();
		len = s.size();
		return *this;
	}

	constexpr StringBase& operator=(const StringBase& s) noexcept {
		str = s.data();
		len = s.size();
		return *this;
	}

	constexpr StringBase& operator=(std::nullptr_t) = delete;

	~StringBase() = default;

	// Returns whether the StringBase is empty (size() == 0)
	constexpr bool empty() const noexcept { return (len == 0); }

	// Returns the number of characters in the StringBase
	constexpr size_type size() const noexcept { return len; }

	// Returns the number of characters in the StringBase
	constexpr size_type length() const noexcept { return len; }

	// Returns a pointer to the underlying character array
	constexpr pointer data() noexcept { return str; }

	// Returns a const pointer to the underlying character array
	constexpr const_pointer data() const noexcept { return str; }

	constexpr iterator begin() noexcept { return str; }

	constexpr iterator end() noexcept { return str + len; }

	constexpr const_iterator begin() const noexcept { return str; }

	constexpr const_iterator end() const noexcept { return str + len; }

	constexpr const_iterator cbegin() const noexcept { return str; }

	constexpr const_iterator cend() const noexcept { return str + len; }

	// Returns reference to first element
	constexpr reference front() noexcept { return str[0]; }

	// Returns const_reference to first element
	constexpr const_reference front() const noexcept { return str[0]; }

	// Returns reference to last element
	constexpr reference back() noexcept { return str[len - 1]; }

	// Returns const_reference to last element
	constexpr const_reference back() const noexcept { return str[len - 1]; }

	// Returns reference to n-th element
	constexpr reference operator[](size_type n) noexcept {
#ifdef _GLIBCXX_DEBUG
		assert(n >= 0);
		assert(n < len);
#endif
		return str[n];
	}

	// Returns const_reference to n-th element
	constexpr const_reference operator[](size_type n) const noexcept {
#ifdef _GLIBCXX_DEBUG
		assert(n >= 0);
		assert(n < len);
#endif
		return str[n];
	}

	// Like operator[] but throws exception if n >= size()
	constexpr reference at(size_type n) {
		if (n >= len)
			throw std::out_of_range("StringBase::at");

#ifdef _GLIBCXX_DEBUG
		assert(n >= 0);
		assert(n < len);
#endif
		return str[n];
	}

	// Like operator[] but throws exception if n >= size()
	constexpr const_reference at(size_type n) const {
		if (n >= len)
			throw std::out_of_range("StringBase::at");

		return str[n];
	}

	// Swaps two StringBase
	void swap(StringBase& s) noexcept {
		// Swap str
		pointer p = str;
		str = s.str;
		s.str = p;
		// Swap len
		size_type x = len;
		len = s.len;
		s.len = x;
	}

	/**
	 * @brief Compares two StringBase
	 *
	 * @param s StringBase to compare with
	 *
	 * @return <0 - this < @p s, 0 - equal, >0 - this > @p s
	 */
	constexpr int compare(const StringBase& s) const noexcept {
		size_type clen = std::min(len, s.len);
		int rc = std::char_traits<Char>::compare(str, s.str, clen);
		return rc != 0 ? rc : (len == s.len ? 0 : ((len < s.len) ? -1 : 1));
	}

	constexpr int compare(size_type pos, size_type count,
	                      const StringBase& s) const {
		return substr(pos, count).compare(s);
	}

	// Returns position of the first character of the first substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type find(const StringBase& s) const noexcept {
		if (s.len == 0)
			return 0;

		// KMP algorithm
		auto p = std::make_unique<size_type[]>(s.len);
		size_type k = p[0] = 0;
		// Fill p
		for (size_type i = 1; i < s.len; ++i) {
			while (k > 0 && s[i] != s[k])
				k = p[k - 1];
			if (s[i] == s[k])
				++k;
			p[i] = k;
		}

		k = 0;
		for (size_type i = 0; i < len; ++i) {
			while (k > 0 && str[i] != s[k])
				k = p[k - 1];
			if (str[i] == s[k]) {
				++k;
				if (k == s.len)
					return i - s.len + 1;
			}
		}

		return npos;
	}

	constexpr size_type find(const StringBase& s, size_type beg1) const {
		return find(s.substr(beg1));
	}

	constexpr size_type find(const StringBase& s, size_type beg1,
	                         size_type endi1) const {
		return find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type find(size_type beg, const StringBase& s,
	                         size_type beg1 = 0) const {
		return substr(beg).find(s.substr(beg1, len - beg1));
	}

	constexpr size_type find(size_type beg, const StringBase& s, size_type beg1,
	                         size_type endi1) const {
		return substr(beg).find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type find(size_type beg, size_type endi, const StringBase& s,
	                         size_type beg1 = 0) const {
		return substr(beg, endi).find(s.substr(beg1, len - beg1));
	}

	constexpr size_type find(size_type beg, size_type endi, const StringBase& s,
	                         size_type beg1, size_type endi1) const {
		return substr(beg, endi).find(
		   s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type find(char c, size_type beg = 0) const noexcept {
		for (; beg < len; ++beg)
			if (str[beg] == c)
				return beg;

		return npos;
	}

	constexpr size_type find(char c, size_type beg, size_type endi) const
	   noexcept {
		if (endi > len)
			endi = len;

		for (; beg < endi; ++beg)
			if (str[beg] == c)
				return beg;

		return npos;
	}

	// Returns position of the first character of the last substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type rfind(const StringBase& s) const noexcept {
		if (s.len == 0)
			return 0;

		// KMP algorithm
		auto p = std::make_unique<size_type[]>(s.len);
		size_type slen1 = s.len - 1, k = p[slen1] = slen1;
		// Fill p
		for (size_type i = slen1 - 1; i != npos; --i) {
			while (k < slen1 && s[i] != s[k])
				k = p[k + 1];
			if (s[i] == s[k])
				--k;
			p[i] = k;
		}

		k = slen1;
		for (size_type i = len - 1; i != npos; --i) {
			while (k < slen1 && str[i] != s[k])
				k = p[k + 1];
			if (str[i] == s[k]) {
				--k;
				if (k == npos)
					return i;
			}
		}

		return npos;
	}

	constexpr size_type rfind(const StringBase& s, size_type beg1) const {
		return rfind(s.substr(beg1, len - beg1));
	}

	constexpr size_type rfind(const StringBase& s, size_type beg1,
	                          size_type endi1) const {
		return rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type rfind(size_type beg, const StringBase& s,
	                          size_type beg1 = 0) const {
		return substr(beg).rfind(s.substr(beg1, len - beg1));
	}

	constexpr size_type rfind(size_type beg, const StringBase& s,
	                          size_type beg1, size_type endi1) const {
		return substr(beg).rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type rfind(size_type beg, size_type endi,
	                          const StringBase& s, size_type beg1 = 0) const {
		return substr(beg, endi).rfind(s.substr(beg1, len - beg1));
	}

	constexpr size_type rfind(size_type beg, size_type endi,
	                          const StringBase& s, size_type beg1,
	                          size_type endi1) const {
		return substr(beg, endi).rfind(
		   s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type rfind(char c, size_type beg = 0) const noexcept {
		for (size_type endi = len; endi > beg;)
			if (str[--endi] == c)
				return endi;

		return npos;
	}

	constexpr size_type rfind(char c, size_type beg, size_type endi) const
	   noexcept {
		if (endi > len)
			endi = len;

		for (; endi > beg;)
			if (str[--endi] == c)
				return endi;

		return npos;
	}

protected:
	// Returns a StringBase of the substring [pos, ...)
	constexpr StringBase substr(size_type pos) const {
		if (pos > len)
			throw std::out_of_range("StringBase::substr");

		return StringBase(str + pos, len - pos);
	}

	// Returns a StringBase of the substring [pos, pos + count)
	constexpr StringBase substr(size_type pos, size_type count) const {
		if (pos > len)
			throw std::out_of_range("StringBase::substr");

		return StringBase(str + pos, std::min(count, len - pos));
	}

	// Returns a StringBase of the substring [beg, ...)
	constexpr StringBase substring(size_type beg) const { return substr(beg); }

	constexpr StringBase substring(size_type beg, size_type endi) const {
		if (beg > endi || beg > len)
			throw std::out_of_range("StringBase::substring");

		return StringBase(str + beg, std::min(len, endi) - beg);
	}

public:
	std::string to_string() const { return std::string(str, len); }
};

template <class Char>
constexpr std::string& operator+=(std::string& str, const StringBase<Char>& s) {
	return str.append(s.data(), s.size());
}

template <class CharT, class Traits, class Char>
constexpr std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const StringBase<Char>& s) {
	return os.write(s.data(), s.size());
}

class StringView : public StringBase<const char> {
public:
	using StringBase::StringBase;

	constexpr StringView() noexcept : StringBase("", 0) {}

	constexpr StringView(std::nullptr_t) noexcept = delete;

	constexpr StringView(const StringView&) noexcept = default;
	constexpr StringView(StringView&&) noexcept = default;
	StringView& operator=(const StringView&) noexcept = default;
	StringView& operator=(StringView&&) noexcept = default;

	constexpr StringView& operator=(std::nullptr_t) noexcept = delete;

	constexpr StringView operator=(pointer p) noexcept {
		return operator=(StringView {p});
	}

	template <class T, std::enable_if_t<std::is_rvalue_reference_v<T>, int> = 0>
	StringView& operator=(T&&) = delete; // Protect from assigning unsafe data

	constexpr StringView(const StringBase& s) noexcept : StringBase(s) {}

	~StringView() = default;

	constexpr void clear() noexcept { len = 0; }

	// Removes prefix of length n
	constexpr StringView& remove_prefix(size_type n) noexcept {
		if (n > len)
			n = len;
		str += n;
		len -= n;
		return *this;
	}

	// Removes suffix of length n
	constexpr StringView& remove_suffix(size_type n) noexcept {
		if (n > len)
			len = 0;
		else
			len -= n;
		return *this;
	}

	// Extracts prefix of length n
	constexpr StringView extract_prefix(size_type n) noexcept {
		if (n > len)
			n = len;

		StringView res = substring(0, n);
		str += n;
		len -= n;
		return res;
	}

	// Extracts suffix of length n
	constexpr StringView extract_suffix(size_type n) noexcept {
		if (n > len)
			len = n;
		len -= n;
		return {data() + len, n};
	}

	// Removes leading characters for which f() returns true
	template <class Func>
	constexpr StringView& remove_leading(Func&& f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {
		}
		str += i;
		len -= i;
		return *this;
	}

	constexpr StringView& remove_leading(char c) noexcept {
		size_type i = 0;
		for (; i < len && str[i] == c; ++i) {
		}
		str += i;
		len -= i;
		return *this;
	}

	// Removes trailing characters for which f() returns true
	template <class Func>
	constexpr StringView& remove_trailing(Func&& f) {
		while (len > 0 && f(back()))
			--len;
		return *this;
	}

	constexpr StringView& remove_trailing(char c) noexcept {
		while (len > 0 and back() == c)
			--len;
		return *this;
	}

	// Extracts leading characters for which f() returns true
	template <class Func>
	constexpr StringView extract_leading(Func&& f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {
		}

		StringView res = substring(0, i);
		str += i;
		len -= i;

		return res;
	}

	// Extracts trailing characters for which f() returns true
	template <class Func>
	constexpr StringView extract_trailing(Func&& f) {
		size_type i = len;
		for (; i > 0 && f(str[i - 1]); --i) {
		}

		StringView res = substring(i, len);
		len = i;

		return res;
	}

	constexpr StringView without_prefix(size_t n) noexcept {
		return StringView(*this).remove_prefix(n);
	}

	constexpr StringView without_suffix(size_t n) noexcept {
		return StringView(*this).remove_suffix(n);
	}

	template <class T>
	constexpr StringView without_leading(T&& arg) {
		return StringView(*this).remove_leading(std::forward<T>(arg));
	}

	template <class T>
	constexpr StringView without_trailing(T&& arg) {
		return StringView(*this).remove_trailing(std::forward<T>(arg));
	}

	template<class... Args>
	StringView substr(Args&&... args) {
		return StringBase::substr(std::forward<Args>(args)...);
	}

	template<class... Args>
	StringView substring(Args&&... args) {
		return StringBase::substring(std::forward<Args>(args)...);
	}
};

inline std::string& operator+=(std::string& str, StringView s) {
	return str.append(s.data(), s.size());
}

inline StringView intentional_unsafe_string_view(const char* str) noexcept {
	return {str};
}

inline StringView intentional_unsafe_string_view(StringView str) noexcept {
	return str;
}

// This function allows std::string&& to be converted to StringView, but keep
// in mind that if any StringView or alike value generated from the result of
// this function cannot be saved to variable! - it would (and probably will)
// cause using of the provided std::string's data that was already deallocated
// = memory corruption
inline StringView
intentional_unsafe_string_view(const std::string&& str) noexcept {
	return StringView(static_cast<const std::string&>(str));
}

// comparison operators
constexpr bool operator==(StringView a, StringView b) noexcept {
	return (a.size() == b.size() &&
	        std::char_traits<char>::compare(a.data(), b.data(), a.size()) == 0);
}

constexpr bool operator!=(StringView a, StringView b) noexcept {
	return (a.size() != b.size() ||
	        std::char_traits<char>::compare(a.data(), b.data(), a.size()) != 0);
}

constexpr bool operator<(StringView a, StringView b) noexcept {
	return (a.compare(b) < 0);
}

constexpr bool operator>(StringView a, StringView b) noexcept {
	return (a.compare(b) > 0);
}

constexpr bool operator<=(StringView a, StringView b) noexcept {
	return (a.compare(b) <= 0);
}

constexpr bool operator>=(StringView a, StringView b) noexcept {
	return (a.compare(b) >= 0);
}

class CStringView : public StringBase<const char> {
public:
	constexpr CStringView() : StringBase("", 0) {}

	constexpr CStringView(std::nullptr_t) : CStringView() {}

	template <size_t N>
	constexpr CStringView(const char (&s)[N])
	   : StringBase(s, std::char_traits<char>::length(s)) {}

	// Do not treat as possible string literal
	template <size_t N>
	constexpr CStringView(char (&s)[N]) : StringBase(s) {}

	template <size_t N>
	constexpr CStringView(const StaticCStringBuff<N>& s) : StringBase(s) {}

	CStringView(const std::string& s) noexcept
	   : StringBase(s.data(), s.size()) {}

	// Be careful with the constructor below! @p s cannot be null
	constexpr explicit CStringView(pointer s) noexcept : StringBase(s) {
#ifdef _GLIBCXX_DEBUG
		assert(s);
#endif
	}

	// Be careful with the constructor below! @p s cannot be null
	constexpr CStringView(pointer s, size_type n) noexcept : StringBase(s, n) {
#ifdef _GLIBCXX_DEBUG
		assert(s);
		assert(s[n] == '\0');
#endif
	}

	constexpr CStringView(const CStringView&) noexcept = default;
	constexpr CStringView(CStringView&&) noexcept = default;
	constexpr CStringView& operator=(const CStringView&) noexcept = default;
	constexpr CStringView& operator=(CStringView&&) noexcept = default;

	constexpr operator StringView() const { return {data(), size()}; }

	constexpr CStringView substr(size_type pos) const {
		const auto x = StringBase::substr(pos);
		return CStringView {x.data(), x.size()};
	}

	constexpr StringView substr(size_type pos, size_type count) const {
		return StringBase::substr(pos, count);
	}

	constexpr CStringView substring(size_type beg) const { return substr(beg); }

	constexpr StringView substring(size_type beg, size_type end) const {
		return StringBase::substring(beg, end);
	}

	constexpr const_pointer c_str() const noexcept { return data(); }
};

constexpr StringView substring(StringView str, StringView::size_type beg,
                               StringView::size_type end = StringView::npos) {
	return str.substring(beg, end);
}

// Like string::find() but searches in [beg, end)
constexpr size_t find(StringView str, char c, size_t beg = 0) {
	return str.find(c, beg);
}

// Like string::find() but searches in [beg, end)
constexpr size_t find(StringView str, char c, size_t beg, size_t end) {
	return str.find(c, beg, end);
}

// Removes trailing characters for which f() returns true
template <class Func>
void remove_trailing(std::string& str, Func&& f) {
	auto it = str.end();
	while (it != str.begin())
		if (!f(*--it)) {
			++it;
			break;
		}
	str.erase(it, str.end());
}

/// Removes trailing @p c
inline void remove_trailing(std::string& str, char c) noexcept {
	remove_trailing(str, [c](char x) { return (x == c); });
}
