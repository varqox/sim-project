#pragma once

#include "simlib/avl_dict.hh"
#include "simlib/inplace_buff.hh"
#include "simlib/libzip.hh"

namespace sim {

// Holds a list of entries (full path for each)
class PackageContents {
	struct Span {
		size_t begin, end;
	};

	InplaceBuff<1> buff;
	struct Comparer {
		decltype(buff)* buff_ref;

		Comparer(decltype(buff)& br) : buff_ref(&br) {}

		static StringView to_str(StringView str) noexcept { return str; }

		StringView to_str(Span x) const {
			return {buff_ref->data() + x.begin, x.end - x.begin};
		}

		template <class A, class B>
		bool operator()(A&& a, B&& b) const {
			return to_str(a) < to_str(b);
		}
	};

	AVLDictSet<Span, Comparer> entries {Comparer(buff)};

	StringView to_str(Span x) const {
		return {buff.data() + x.begin, x.end - x.begin};
	}

public:
	PackageContents() = default;

	PackageContents(const PackageContents&) = delete;
	PackageContents(PackageContents&&) noexcept = default;
	PackageContents& operator=(const PackageContents&) = delete;
	PackageContents& operator=(PackageContents&&) noexcept = default;

	template <class... Args,
	          std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
	void add_entry(Args&&... args) {
		auto prev_size = buff.size;
		buff.append(std::forward<Args>(args)...);
		entries.insert({prev_size, buff.size});
	}

	bool remove(StringView entry) { return entries.erase(entry); }

	void remove_with_prefix(StringView prefix) {
		const Span* s = entries.lower_bound(prefix);
		while (s and has_prefix(to_str(*s), prefix)) {
			entries.erase(*s);
			s = entries.lower_bound(prefix);
		}
	}

	/// @p callback should take one argument of type StringView - it will
	/// contain the entry's path
	template <class Func>
	void for_each_with_prefix(StringView prefix, Func&& callback) const {
		entries.foreach_since_lower_bound(prefix, [&](Span s) {
			StringView entry = to_str(s);
			if (not has_prefix(entry, prefix))
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
			if (not has_prefix(to_str(s), candidate)) {
				candidate = ""; // There is no master dir
				return false;
			}

			return true;
		});

		return candidate;
	}

	void load_from_directory(StringView pkg_path,
	                         bool retain_pkg_path_prefix = false);

	void load_from_zip(FilePath pkg_path);
};

/// Finds master directory (with trailing '/') in @p pkg_path if such does not
/// exist "" is returned
std::string zip_package_master_dir(ZipFile& zip);

/// Finds master directory (with trailing '/') in @p pkg_path if such does not
/// exist "" is returned
inline std::string zip_package_master_dir(FilePath pkg_path) {
	ZipFile zip(pkg_path, ZIP_RDONLY);
	return zip_package_master_dir(zip);
}

} // namespace sim
