#pragma once

#include "simlib/string_view.hh"

template <size_t N>
class InplaceBuff;

// This type should NOT be returned from a function call
class FilePath {
	const char* str_;
	size_t size_;

public:
	constexpr FilePath(const FilePath&) noexcept = default;
	constexpr FilePath(FilePath&&) noexcept = default;

	// NOLINTNEXTLINE(google-explicit-constructor)
	constexpr FilePath(const char* str) noexcept
	: str_(str)
	, size_(std::char_traits<char>::length(str)) {}

	// NOLINTNEXTLINE(google-explicit-constructor)
	constexpr FilePath(const CStringView& str) noexcept
	: str_(str.c_str())
	, size_(str.size()) {}

	// NOLINTNEXTLINE(google-explicit-constructor)
	FilePath(const std::string& str) noexcept
	: str_(str.c_str())
	, size_(str.size()) {}

	template <size_t N>
	// NOLINTNEXTLINE(google-explicit-constructor)
	constexpr FilePath(InplaceBuff<N>& str) noexcept
	: str_(str.to_cstr().data())
	, size_(str.size) {}

	template <size_t N>
	// NOLINTNEXTLINE(google-explicit-constructor)
	constexpr FilePath(InplaceBuff<N>&& str) noexcept
	: str_(str.to_cstr().data())
	, size_(str.size) {}

	constexpr FilePath& operator=(const FilePath&) noexcept = delete;
	constexpr FilePath& operator=(FilePath&&) noexcept = delete;
	// NOLINTNEXTLINE(misc-unconventional-assign-operator)
	constexpr FilePath& operator=(FilePath&) noexcept = default;

	constexpr FilePath& operator=(const char* str) noexcept {
		str_ = str;
		size_ = std::char_traits<char>::length(str);
		return *this;
	}

	constexpr FilePath& operator=(CStringView& str) noexcept {
		str_ = str.c_str();
		size_ = str.size();
		return *this;
	}

	FilePath& operator=(std::string& str) noexcept {
		str_ = str.c_str();
		size_ = str.size();
		return *this;
	}

	template <size_t N>
	constexpr FilePath& operator=(InplaceBuff<N>& str) noexcept {
		str_ = str.to_cstr().data();
		size_ = str.size;
		return *this;
	}

	~FilePath() = default;

	// NOLINTNEXTLINE(google-explicit-constructor)
	constexpr operator const char*() const noexcept { return str_; }

	[[nodiscard]] constexpr CStringView to_cstr() const noexcept {
		return {str_, size_};
	}

	[[nodiscard]] std::string to_str() const noexcept {
		return std::string(str_, size_);
	}

	[[nodiscard]] constexpr const char* data() const noexcept { return str_; }

	[[nodiscard]] constexpr size_t size() const noexcept { return size_; }
};
