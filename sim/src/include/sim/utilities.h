#pragma once

#include <algorithm>
#include <cassert>
#include <simlib/debug.h>
#include <simlib/string.h>
#include <simlib/time.h>

inline bool isUsername(StringView str) noexcept {
	return std::all_of(str.begin(), str.end(), [](int x) {
		return (isalnum(x) || x == '_' || x == '-');
	});
}

constexpr inline bool is_safe_timestamp(StringView str) noexcept {
	return isDigitNotGreaterThan<std::numeric_limits<time_t>::max()>(str);
}

constexpr inline bool is_safe_inf_timestamp(StringView str) noexcept {
	return (str == "+inf" or str == "-inf" or is_safe_timestamp(str));
}

class InfDatetime {
	enum class Type { NEG_INF, DATE, INF } type;
	// Format: YYYY-mm-dd HH:MM:SS
	InplaceBuff<19> date;

	static constexpr const char* NEG_INF_STR = "#";
	static constexpr const char* INF_STR = "@";

public:
	InfDatetime() : type(Type::INF) {}

	template <class T>
	explicit InfDatetime(T&& str) {
		from_str(std::forward<T>(str));
	}

	InfDatetime(const InfDatetime&) = default;
	InfDatetime(InfDatetime&&) noexcept = default;
	InfDatetime& operator=(const InfDatetime&) = default;
	InfDatetime& operator=(InfDatetime&&) noexcept = default;

	/// Assumption: to_str(NEG_INF) < to_str(DATE) < to_str(INF)
	/// WARNING: if type == DATE then returned value is ONLY a view on the
	/// internal field date
	StringView to_str() const noexcept {
		switch (type) {
		case Type::NEG_INF: return NEG_INF_STR;
		case Type::DATE: return date;
		case Type::INF: return INF_STR;
		}

		assert(false);
	}

	bool is_neg_inf() const noexcept { return type == Type::NEG_INF; }

	bool is_inf() const noexcept { return type == Type::INF; }

	void set_neg_inf() noexcept { type = Type::NEG_INF; }

	void set_inf() noexcept { type = Type::INF; }

	void set_datetime(CStringView datetime) {
		if (isDatetime(datetime)) {
			type = Type::DATE;
			date = datetime;
		} else {
			THROW("Invalid date datetime string: ", datetime);
		}
	}

	void from_str(CStringView str) {
		if (str == NEG_INF_STR)
			set_neg_inf();
		else if (str == INF_STR)
			set_inf();
		else
			set_datetime(str);
	}

	template <class T>
	std::enable_if_t<std::is_convertible<T, CStringView>::value, void>
	from_str(T&& str) {
		return from_str(CStringView(str));
	}

	template <class T>
	std::enable_if_t<std::is_convertible<T, StringView>::value &&
	                    !std::is_convertible<T, CStringView>::value,
	                 void>
	from_str(T&& str) {
		auto x = concat<20>(str);
		return from_str(x.to_cstr());
	}

	StringView to_api_str() const noexcept {
		switch (type) {
		case Type::NEG_INF: return "-inf";
		case Type::DATE: return date;
		case Type::INF: return "+inf";
		}

		assert(false);
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
	if (str == "+inf")
		res.set_inf();
	else if (str == "-inf")
		res.set_neg_inf();
	else
		res.set_datetime(mysql_date(strtoull(str)));

	return res;
}
