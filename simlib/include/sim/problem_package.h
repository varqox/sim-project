#pragma once

#include "../string.h"

#include <set>

namespace sim {

// Holds a list of entries (full path for each)
class PackageContents {
	struct Span {
		size_t begin, end;
	};

	InplaceBuff<1> buff;
	struct Comparer {
		decltype(buff)& buff_ref;

		Comparer(decltype(buff)& br) : buff_ref(br) {}

		StringView to_str(Span x) const {
			return {buff_ref.data() + x.begin, x.end - x.begin};
		}

		bool operator()(Span a, Span b) const { return to_str(a) < to_str(b); }
	};
	std::set<Span, Comparer> entries {Comparer(buff)};

	StringView to_str(Span x) const {
		return {buff.data() + x.begin, x.end - x.begin};
	}

	Span to_span(StringView str) {
		buff.append(str);
		buff.size -= str.size();
		// Range in which str resides in the buff is still valid, so we can
		// use it afterwards, as long as buff won't be modified
		return {buff.size, buff.size + str.size()};
	}

public:
	template<class... Args>
	void add_entry(Args&&... args) {
		auto prev_size = buff.size;
		buff.append(std::forward<Args>(args)...);
		entries.insert({prev_size, buff.size});
	}

	void remove_with_prefix(StringView prefix) {
		auto it = entries.lower_bound(to_span(prefix));
		while (it != entries.end() and hasPrefix(to_str(*it), prefix))
			entries.erase(it++);
	}

	/// @p callback should take one argument of type StringView - it will
	/// contain the entry's path
	template<class Func>
	void for_each_with_prefix(StringView prefix, Func&& callback) {
		auto it = entries.lower_bound(to_span(prefix));
		while (it != entries.end() and hasPrefix(to_str(*it), prefix)) {
			callback(to_str(*it));
			++it;
		}
	}

	bool exists(StringView entry) {
		return (entries.find(to_span(entry)) != entries.end());
	}

	/// Finds master directory (with trailing '/') if such does not exist "" is
	/// returned
	StringView master_dir() const {
		if (entries.empty())
			return "";

		auto it = entries.begin();
		StringView candidate = to_str(*it);
		{
			auto pos = candidate.find('/');
			if (pos == StringView::npos)
				return ""; // There is no master dir
			candidate = candidate.substr(0, pos + 1);
		}

		while (++it != entries.end())
			if (not hasPrefix(to_str(*it), candidate))
				return ""; // There is no master dir

		return candidate;
	}

	void load_from_directory(std::string pkg_path);

	void load_from_zip(CStringView pkg_path);
};

/// Finds master directory(with trailing '/')  in @p pkg_path if such does not
/// exist "" is returned
std::string zip_package_master_dir(CStringView pkg_path);

/// Finds master directory(with trailing '/')  in @p pkg_path if such does not
/// exist "" is returned
std::string zip_package_master_dir(int pkg_fd);

} // namespace sim
