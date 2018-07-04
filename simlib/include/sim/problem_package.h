#pragma once

#include "../string.h"
#include "../avl_dict.h"

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

		static StringView to_str(StringView str) noexcept { return str; }

		StringView to_str(Span x) const {
			return {buff_ref.data() + x.begin, x.end - x.begin};
		}

		template<class A, class B>
		bool operator()(A&& a, B&& b) const { return to_str(a) < to_str(b); }
	};

	AVLDictSet<Span, Comparer> entries {Comparer(buff)};

	StringView to_str(Span x) const {
		return {buff.data() + x.begin, x.end - x.begin};
	}

public:
	template<class... Args>
	void add_entry(Args&&... args) {
		auto prev_size = buff.size;
		buff.append(std::forward<Args>(args)...);
		entries.insert({prev_size, buff.size});
	}

	bool remove(StringView entry) { return entries.erase(entry); }

	void remove_with_prefix(StringView prefix) {
		const Span* s = entries.lower_bound(prefix);
		while (s and hasPrefix(to_str(*s), prefix)) {
			entries.erase(*s);
			s = entries.lower_bound(prefix);
		}
	}

	/// @p callback should take one argument of type StringView - it will
	/// contain the entry's path
	template<class Func>
	void for_each_with_prefix(StringView prefix, Func&& callback) const {
		entries.foreach_since_lower_bound(prefix, [&](Span s) {
			StringView entry = to_str(s);
			if (not hasPrefix(entry, prefix))
				return false;

			callback(entry);
			return true;
		});
	}

	bool exists(StringView entry) const {
		return (entries.find(entry) != nullptr);
	}

	/// Finds master directory (with trailing '/') if such does not exist "" is
	/// returned
	StringView master_dir() const {
		if (entries.empty())
			return "";

		StringView candidate = to_str(*entries.front());
		{
			auto pos = candidate.find('/');
			if (pos == StringView::npos)
				return ""; // There is no master dir
			candidate = candidate.substr(0, pos + 1);
		}

		entries.for_each([&](Span s) {
			if (not hasPrefix(to_str(s), candidate)) {
				candidate = ""; // There is no master dir
				return false;
			}

			return true;
		});

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
