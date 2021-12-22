#pragma once

#include "simlib/string_view.hh"

#include <optional>

// Returns an absolute path that does not contain any . or .. components,
// nor any repeated path separators (/). @p curr_dir can be empty. If path
// begins with / then @p curr_dir is ignored.
std::string path_absolute(StringView path, std::string curr_dir = "/");

/**
 * @brief Returns the filename (last non-directory component) of the @p path
 * @details Examples:
 *   "/my/path/foo.bar" -> "foo.bar"
 *   "/my/path/" -> ""
 *   "/" -> ""
 *   "/my/path/." -> "."
 *   "/my/path/.." -> ".."
 *   "foo" -> "foo"
 */
template <class T>
constexpr auto path_filename(T&& path) noexcept {
    using RetType =
            std::conditional_t<std::is_convertible_v<T, CStringView>, CStringView, StringView>;
    RetType path_str(std::forward<T>(path));
    auto pos = path_str.rfind('/');
    return path_str.substr(pos == path_str.npos ? 0 : pos + 1);
}

// Returns extension (without dot) e.g. "foo.cc" -> "cc", "bar" -> ""
template <class T>
constexpr auto path_extension(T&& path) noexcept {
    using RetType =
            std::conditional_t<std::is_convertible_v<T, CStringView>, CStringView, StringView>;

    RetType path_str(std::forward<T>(path));
    size_t start_pos = path_str.rfind('/');
    if (start_pos == path_str.npos) {
        start_pos = 0;
    } else {
        ++start_pos;
    }

    size_t x = path_str.rfind('.', start_pos);
    if (x == path_str.npos) {
        return RetType{}; // No extension
    }

    return path_str.substr(x + 1);
}

/**
 * @brief Returns prefix of the @p path that is a dirpath of the @p path
 * @details Examples:
 *   "/my/path/foo.bar" -> "/my/path/"
 *   "/my/path/" -> "/my/path/"
 *   "/" -> "/"
 *   "/my/path/." -> "/my/path/"
 *   "/my/path/.." -> "/my/path/"
 *   "abc/efg" -> "abc/"
 *   "foo" -> ""
 */
constexpr inline StringView path_dirpath(const StringView& path) noexcept {
    auto pos = path.rfind('/');
    return path.substring(0, pos == StringView::npos ? 0 : pos + 1);
}

/**
 * @brief Checks every ancestor (parent) directory of @p path if it has @p
 *   subpath and if true returns path of @p subpath in ancestor directory
 * @details Examples:
 *   1) @p path == "a/b/c/file", @p subpath == "/x/y/z", tried paths will be:
 *     "a/b/c/x/y/z"
 *     "a/b/x/y/z"
 *     "a/x/y/z"
 *     "x/y/z"
 *   2) @p path == "/a/b/c/", @p subpath == "x/y/", tried paths will be:
 *     "/a/b/c/x/y/"
 *     "/a/b/x/y/"
 *     "/a/x/y/"
 *     "/x/y/"
 */
std::optional<std::string> deepest_ancestor_dir_with_subpath(
        std::string path, StringView subpath);
