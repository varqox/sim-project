#pragma once

#include <cstdint>
#include <simlib/string_transform.hh>
#include <simlib/time.hh>

namespace sim {

constexpr bool is_safe_timestamp(StringView str) noexcept {
	return (str <= intentional_unsafe_string_view(
	                  to_string(std::numeric_limits<time_t>::max())));
}

constexpr bool is_safe_inf_timestamp(StringView str) noexcept {
	return (str == "+inf" or str == "-inf" or is_safe_timestamp(str));
}

class InfDatetime {
	enum class Type { NEG_INF, DATE, INF } type = Type::INF;
	// Format: YYYY-mm-dd HH:MM:SS
	InplaceBuff<19> date;

	static constexpr const char NEG_INF_STR[] = "#";
	static constexpr const char INF_STR[] = "@";

public:
	InfDatetime() = default;

	template <class T,
	          std::enable_if_t<not std::is_same_v<std::decay_t<T>, InfDatetime>,
	                           int> = 0>
	// NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
	explicit InfDatetime(T&& str) {
		from_str(std::forward<T>(str));
	}

	InfDatetime(const InfDatetime&) = default;
	InfDatetime(InfDatetime&&) noexcept = default;
	InfDatetime& operator=(const InfDatetime&) = default;
	InfDatetime& operator=(InfDatetime&&) noexcept = default;
	~InfDatetime() = default;

	/// Assumption: to_str(NEG_INF) < to_str(DATE) < to_str(INF)
	/// WARNING: if type == DATE then returned value is ONLY a view on the
	/// internal field date
	[[nodiscard]] StringView to_str() const noexcept {
		switch (type) {
		case Type::NEG_INF: return NEG_INF_STR;
		case Type::DATE: return date;
		case Type::INF: return INF_STR;
		}

		__builtin_unreachable();
	}

	[[nodiscard]] bool is_neg_inf() const noexcept {
		return type == Type::NEG_INF;
	}

	[[nodiscard]] bool is_inf() const noexcept { return type == Type::INF; }

	InfDatetime& set_neg_inf() noexcept {
		type = Type::NEG_INF;
		return *this;
	}

	InfDatetime& set_inf() noexcept {
		type = Type::INF;
		return *this;
	}

	InfDatetime& set_datetime(CStringView datetime) {
		if (is_datetime(datetime)) {
			type = Type::DATE;
			date = datetime;
		} else {
			THROW("Invalid date datetime string: ", datetime);
		}

		return *this;
	}

	InfDatetime& from_str(CStringView str) {
		if (str == NEG_INF_STR) {
			set_neg_inf();
		} else if (str == INF_STR) {
			set_inf();
		} else {
			set_datetime(str);
		}

		return *this;
	}

	template <class T>
	std::enable_if_t<std::is_convertible<T, CStringView>::value, InfDatetime&>
	from_str(T&& str) {
		return from_str(CStringView(str));
	}

	template <class T>
	std::enable_if_t<std::is_convertible<T, StringView>::value &&
	                    !std::is_convertible<T, CStringView>::value,
	                 InfDatetime&>
	from_str(T&& str) {
		auto x = concat<20>(str);
		return from_str(x.to_cstr());
	}

	[[nodiscard]] StringView to_api_str() const noexcept {
		switch (type) {
		case Type::NEG_INF: return "-inf";
		case Type::DATE: return date;
		case Type::INF: return "+inf";
		}

		__builtin_unreachable();
	}

	friend bool operator<(StringView str, const InfDatetime& d) noexcept {
		return str < d.to_str();
	}

	friend bool operator<(const InfDatetime& d1,
	                      const InfDatetime& d2) noexcept {
		return d1.to_str() < d2.to_str();
	}

	friend bool operator<(const InfDatetime& d, StringView str) noexcept {
		return d.to_str() < str;
	}

	friend bool operator>(StringView str, const InfDatetime& d) noexcept {
		return d < str;
	}

	friend bool operator>(const InfDatetime& d1,
	                      const InfDatetime& d2) noexcept {
		return d2 < d1;
	}

	friend bool operator>(const InfDatetime& d, StringView str) noexcept {
		return str < d;
	}

	friend bool operator<=(StringView str, const InfDatetime& d) noexcept {
		return not(d < str);
	}

	friend bool operator<=(const InfDatetime& d1,
	                       const InfDatetime& d2) noexcept {
		return not(d2 < d1);
	}

	friend bool operator<=(const InfDatetime& d, StringView str) noexcept {
		return not(str < d);
	}

	friend bool operator>=(StringView str, const InfDatetime& d) noexcept {
		return d <= str;
	}

	friend bool operator>=(const InfDatetime& d1,
	                       const InfDatetime& d2) noexcept {
		return d2 <= d1;
	}

	friend bool operator>=(const InfDatetime& d, StringView str) noexcept {
		return str <= d;
	}
};

inline InfDatetime inf_timestamp_to_InfDatetime(StringView str) {
	InfDatetime res;
	if (str == "+inf") {
		res.set_inf();
	} else if (str == "-inf") {
		res.set_neg_inf();
	} else {
		res.set_datetime(intentional_unsafe_cstring_view(
		   mysql_date(WONT_THROW(str2num<uintmax_t>(str).value()))));
	}

	return res;
}

} // namespace sim
