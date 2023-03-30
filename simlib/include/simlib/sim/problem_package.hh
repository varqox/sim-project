#pragma once

#include <set>
#include <simlib/inplace_buff.hh>
#include <simlib/libzip.hh>

namespace sim {

// Holds a list of entries (full path for each)
class PackageContents {
    struct Span {
        size_t begin, end;
    };

    InplaceBuff<0> buff;

    struct Comparer {
        struct is_transparent;

        decltype(buff)* buff_ref;

        explicit Comparer(decltype(buff)& br) : buff_ref(&br) {}

        static StringView to_str(StringView str) noexcept { return str; }

        [[nodiscard]] StringView to_str(Span x) const {
            return {buff_ref->data() + x.begin, x.end - x.begin};
        }

        template <class A, class B>
        bool operator()(A&& a, B&& b) const {
            return to_str(a) < to_str(b);
        }
    };

    std::set<Span, Comparer> entries{Comparer(buff)};

    [[nodiscard]] StringView to_str(Span x) const {
        return {buff.data() + x.begin, x.end - x.begin};
    }

public:
    PackageContents() = default;

    PackageContents(const PackageContents&) = delete;
    PackageContents(PackageContents&&) noexcept = default;
    PackageContents& operator=(const PackageContents&) = delete;
    PackageContents& operator=(PackageContents&&) noexcept = default;

    ~PackageContents() = default;

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    void add_entry(Args&&... args) {
        auto prev_size = buff.size;
        buff.append(std::forward<Args>(args)...);
        entries.insert({prev_size, buff.size});
    }

    // Returns bool denoting whether the erasing took place
    bool remove(const StringView& entry) {
        auto it = entries.find(entry);
        if (it == entries.end()) {
            return false;
        }
        entries.erase(it);
        return true;
    }

    void remove_with_prefix(const StringView& prefix) {
        auto it = entries.lower_bound(prefix);
        while (it != entries.end() and has_prefix(to_str(*it), prefix)) {
            entries.erase(it++);
        }
    }

    /// @p callback should take one argument of type StringView - it will
    /// contain the entry's path
    template <class Func>
    void for_each_with_prefix(const StringView& prefix, Func&& callback) const {
        auto it = entries.lower_bound(prefix);
        while (it != entries.end()) {
            StringView entry = to_str(*it);
            if (not has_prefix(entry, prefix)) {
                break;
            }

            callback(entry);
            ++it;
        }
    }

    [[nodiscard]] bool exists(const StringView& entry) const {
        return entries.find(entry) != entries.end();
    }

    /// Finds main directory (with trailing '/') if such does not exist "" is
    /// returned
    [[nodiscard]] StringView main_dir() const {
        if (entries.empty()) {
            return "";
        }

        StringView candidate = to_str(*entries.begin());
        {
            auto pos = candidate.find('/');
            if (pos == StringView::npos) {
                return ""; // There is no main dir
            }
            candidate = candidate.substr(0, pos + 1);
        }

        if (not has_prefix(to_str(*entries.rbegin()), candidate)) {
            return ""; // There is no main dir
        }
        return candidate;
    }

    void load_from_directory(StringView pkg_path, bool retain_pkg_path_prefix = false);

    void load_from_zip(FilePath pkg_path);
};

/// Finds main directory (with trailing '/') in @p pkg_path if such does not
/// exist "" is returned
std::string zip_package_main_dir(ZipFile& zip);

/// Finds main directory (with trailing '/') in @p pkg_path if such does not
/// exist "" is returned
inline std::string zip_package_main_dir(FilePath pkg_path) {
    ZipFile zip(pkg_path, ZIP_RDONLY);
    return zip_package_main_dir(zip);
}

} // namespace sim
