#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <simlib/debug_logger.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/strongly_typed_function.hh>
#include <tuple>
#include <type_traits>
#include <utility>

namespace http {

namespace detail {

template <const char* str_, size_t idx, size_t len>
struct LiteralComponent {
    static constexpr const char* str = str_;
    static constexpr auto beg = idx;
    static constexpr auto end = idx + len;
    static constexpr StringView data{str + idx, len};
};

template <class...>
static constexpr inline bool is_literal_component = false;
template <const char* str, size_t idx, size_t len>
static constexpr inline bool is_literal_component<LiteralComponent<str, idx, len>> = true;

template <class...>
static constexpr inline bool is_std_optional = false;
template <class T>
static constexpr inline bool is_std_optional<std::optional<T>> = true;

template <auto Parser>
struct VariableComponent {
    static constexpr decltype(Parser) parser = Parser;
    using ParserRetType = std::invoke_result_t<decltype(Parser), StringView>;
    static_assert(is_std_optional<ParserRetType>);
    using T = typename ParserRetType::value_type;
};

class UrlParsing {
private:
    template <class ParsedComponentsTuple, size_t idx = 0, class... Args>
    [[nodiscard]] static constexpr auto handler_args_tuple() {
        if constexpr (idx == std::tuple_size_v<ParsedComponentsTuple>) {
            return std::tuple<Args...>{};
        } else {
            using ArgT = std::tuple_element_t<idx, ParsedComponentsTuple>;
            if constexpr (is_literal_component<ArgT>) {
                return handler_args_tuple<ParsedComponentsTuple, idx + 1, Args...>();
            } else {
                return handler_args_tuple<
                    ParsedComponentsTuple,
                    idx + 1,
                    Args...,
                    typename ArgT::T>();
            }
        }
    }

    template <const char* url, class ParsedComponentsTuple, size_t idx = 0, size_t res_size = 1>
    [[nodiscard]] static constexpr StringView get_literal_prefix() {
        static_assert(url[0] == '/');
        if constexpr (idx == std::tuple_size_v<ParsedComponentsTuple>) {
            return StringView{url, res_size};
        } else {
            using Elem = std::tuple_element_t<idx, ParsedComponentsTuple>;
            if constexpr (not is_literal_component<Elem>) {
                return StringView{url, res_size};
            } else {
                static_assert(res_size == Elem::beg);
                constexpr size_t new_res_size = Elem::end + (url[Elem::end] == '/');
                return get_literal_prefix<url, ParsedComponentsTuple, idx + 1, new_res_size>();
            }
        }
    }

    template <
        const char* url,
        class ParsedComponentsTuple,
        size_t idxp1 = std::tuple_size_v<ParsedComponentsTuple>,
        size_t res_beg = StringView{url}.size()>
    [[nodiscard]] static constexpr StringView get_literal_suffix() {
        static_assert(url[0] == '/');
        if constexpr (idxp1 == 0) {
            return StringView{url + res_beg};
        } else {
            using Elem = std::tuple_element_t<idxp1 - 1, ParsedComponentsTuple>;
            if constexpr (not is_literal_component<Elem>) {
                return StringView{url + res_beg};
            } else {
                static_assert(res_beg == Elem::end);
                constexpr size_t new_res_beg = Elem::beg - (url[Elem::beg - 1] == '/');
                return get_literal_suffix<url, ParsedComponentsTuple, idxp1 - 1, new_res_beg>();
            }
        }
    }

    template <const char* url_pattern_, class ParsedComponentsTuple>
    struct ParsedUrl {
        static constexpr const char* url_pattern = url_pattern_;
        using HandlerArgsTuple = decltype(handler_args_tuple<ParsedComponentsTuple>());

        static constexpr StringView literal_prefix =
            get_literal_prefix<url_pattern, ParsedComponentsTuple>();
        static constexpr StringView literal_suffix =
            get_literal_suffix<url_pattern, ParsedComponentsTuple>();

        template <size_t component_idx = 0, class... ParsedArgs>
        [[nodiscard]] static constexpr std::optional<HandlerArgsTuple>
        try_parse(StringView url, ParsedArgs&&... parsed_args) {
            if constexpr (component_idx == std::tuple_size_v<ParsedComponentsTuple>) {
                if (url.empty()) {
                    return std::tuple(std::forward<ParsedArgs>(parsed_args)...);
                }
                return std::nullopt; // url is too long
            } else if (url.empty()) {
                return std::nullopt; // url is too short
            } else {
                using ExpectedComponent =
                    std::tuple_element_t<component_idx, ParsedComponentsTuple>;
                if constexpr (component_idx == 0) {
                    // ^ Optimization, because invariant holds that url begins
                    // with '/' if component_idx > 0
                    if (not has_prefix(url, "/")) {
                        return std::nullopt;
                    }
                }
                assert(has_prefix(url, "/"));
                StringView component = url.substring(1, url.find('/', 1));
                url = url.substring(1 + component.size());
                if constexpr (is_literal_component<ExpectedComponent>) {
                    // Literal
                    if (component != ExpectedComponent::data) {
                        return std::nullopt;
                    }
                    return try_parse<component_idx + 1>(
                        url, std::forward<ParsedArgs>(parsed_args)...
                    );
                } else {
                    // Variable
                    auto opt = ExpectedComponent::parser(component);
                    if (not opt) {
                        return std::nullopt;
                    }
                    return try_parse<component_idx + 1>(
                        url, std::forward<ParsedArgs>(parsed_args)..., std::move(*opt)
                    );
                }
            }
        }
    };

    template <size_t idx, auto Arg0, auto... Args>
    [[nodiscard]] static constexpr auto get_arg() noexcept {
        if constexpr (idx == 0) {
            return Arg0;
        } else {
            return get_arg<idx - 1, Args...>();
        }
    }

    [[nodiscard]] static constexpr std::optional<StringView> str_component_parser(StringView str) {
        return str;
    }

public:
    template <
        const char* url,
        size_t url_idx,
        class ParsedComponentsTuple,
        size_t cp_idx,
        auto... CustomParsers>
    [[nodiscard]] static constexpr auto parse_url() {
        static_assert(url[0] == '/');
        constexpr StringView suffix{url + url_idx};
        if constexpr (suffix.empty()) {
            static_assert(cp_idx == sizeof...(CustomParsers), "Too many custom parsers specified");
            return ParsedUrl<url, ParsedComponentsTuple>{};
        } else {
            static_assert(suffix[0] == '/', "Invalid url");
            constexpr auto component = suffix.substring(1, suffix.find('/', 1));
            constexpr auto new_url_idx = url_idx + 1 + component.size();
            // Parse current component
            if constexpr (has_prefix(component, "{") and has_suffix(component, "}")) {
                // Parse variables
                if constexpr (component == "{i8}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<int8_t> (*)(StringView)>(str2num<int8_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{u8}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<uint8_t> (*)(StringView)>(str2num<uint8_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{i16}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<int16_t> (*)(StringView)>(str2num<int16_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{u16}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<uint16_t> (*)(StringView)>(str2num<uint16_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{i32}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<int32_t> (*)(StringView)>(str2num<int32_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{u32}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<uint32_t> (*)(StringView)>(str2num<uint32_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{i64}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<int64_t> (*)(StringView)>(str2num<int64_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{u64}") {
                    using NextComponent = VariableComponent<
                        static_cast<std::optional<uint64_t> (*)(StringView)>(str2num<uint64_t>)>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{string}") {
                    using NextComponent = VariableComponent<str_component_parser>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx,
                        CustomParsers...>();
                } else if constexpr (component == "{custom}") {
                    static_assert(
                        cp_idx < sizeof...(CustomParsers), "Too few custom parsers specified"
                    );
                    using NextComponent = VariableComponent<get_arg<cp_idx, CustomParsers...>()>;
                    return parse_url<
                        url,
                        new_url_idx,
                        decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                        cp_idx + 1,
                        CustomParsers...>();
                } else {
                    static_assert(
                        url_idx & 0,
                        "Unsupported variable component, check url "
                        "at position url_idx + 1"
                    );
                }
            } else {
                // Literal
                using NextComponent = LiteralComponent<url, url_idx + 1, component.size()>;
                return parse_url<
                    url,
                    new_url_idx,
                    decltype(tuple_cat(ParsedComponentsTuple{}, std::tuple<NextComponent>{})),
                    cp_idx,
                    CustomParsers...>();
            }
        }
    }
};

} // namespace detail

template <const char* url, auto... CustomParsers>
using UrlParser =
    decltype(detail::UrlParsing::parse_url<url, 0, std::tuple<>, 0, CustomParsers...>());

template <class ResponseT>
class UrlDispatcher {
    struct ReverseComparer {
        bool operator()(StringView a, StringView b) const {
            return std::lexicographical_compare(a.rbegin(), a.rend(), b.rbegin(), b.rend());
        }
    };

    // Number of '/' => url's literal prefix => url's literal suffix => vector
    // of (url pattern, handler)
    std::map<
        size_t,
        std::map<
            StringView,
            std::map<
                StringView,
                std::vector<
                    std::pair<StringView, std::function<std::optional<ResponseT>(StringView)>>>,
                ReverseComparer>>>
        handlers_;

    static constexpr size_t count_slash(StringView str) {
        size_t res = 0;
        for (auto c : str) {
            res += (c == '/');
        }
        return res;
    }

    static constexpr DebugLogger<false> debuglog{};

public:
    template <class... Params>
    static strongly_typed_function<ResponseT(Params...)>
    params_to_handler_type_impl(const std::tuple<Params...>&);

    template <class... PrefixParam, class... Param, class... SuffixParam>
    static strongly_typed_function<ResponseT(PrefixParam..., Param..., SuffixParam...)>
    handler_impl(const std::tuple<PrefixParam...>&, const std::tuple<Param...>&, const std::tuple<SuffixParam...>&);

    template <
        class PrefixParamTuple,
        class SuffixParamTuple,
        const char* url_pattern,
        auto... CustomParsers>
    using HandlerImpl = decltype(handler_impl(
        std::declval<PrefixParamTuple>(),
        std::declval<typename UrlParser<url_pattern, CustomParsers...>::HandlerArgsTuple>(),
        std::declval<SuffixParamTuple>()
    ));

    template <const char* url_pattern, auto... CustomParsers>
    using Handler = HandlerImpl<std::tuple<>, std::tuple<>, url_pattern, CustomParsers...>;

    template <const char* url_pattern, auto... CustomParsers>
    void add_handler(Handler<url_pattern, CustomParsers...> handler) {
        using UP = UrlParser<url_pattern, CustomParsers...>;
        constexpr size_t slashes = count_slash(url_pattern);
        handlers_[slashes][UP::literal_prefix][UP::literal_suffix].emplace_back(
            url_pattern,
            [handler = std::move(handler)](StringView url) -> std::optional<ResponseT> {
                auto args_tuple_opt = UP::try_parse(url);
                if (not args_tuple_opt) {
                    debuglog("                parsing failed");
                    return std::nullopt;
                }
                debuglog("                parsing succeeded");
                return std::apply(handler, std::move(*args_tuple_opt));
            }
        );
    }

    void debug_dump() const {
        if constexpr (decltype(debuglog)::is_enabled) {
            for (auto& [slashes, prefix_map] : handlers_) {
                debuglog("> slashes: ", slashes);
                for (auto& [prefix, suffix_map] : prefix_map) {
                    debuglog("    > prefix: ", prefix);
                    for (auto& [suffix, handlers_vec] : suffix_map) {
                        debuglog("        > suffix: ", suffix);
                        for (auto& [url_pattern, handler] : handlers_vec) {
                            debuglog("            > url pattern: ", url_pattern);
                        }
                    }
                }
            }
        }
    }

    /// Returns all pairs of url patterns against which there exist an url that will be matched
    /// to both and more than one handler has to be tried -- you can think of it like an
    /// ambiguity or a collision between two url patterns or handlers. It results in slower
    /// dispatch as all candidates matching the dispatched url must be tried. If the result is
    /// an empty vector, then url dispatch is the fastest possible, and this is what you should
    /// crave for when deciding url patterns.
    [[nodiscard]] std::vector<std::pair<StringView, StringView>> all_potential_collisions() const {
        auto foreach_handler = [](const typename decltype(handlers_)::mapped_type& prefix_map,
                                  auto&& callback) {
            for (auto& [prefix, suffix_map] : prefix_map) {
                for (auto& [suffix, handlers_vec] : suffix_map) {
                    for (auto& [url_pattern, handler] : handlers_vec) {
                        callback(prefix, suffix, url_pattern);
                    }
                }
            }
        };
        auto unordered_has_prefix = [](StringView a, StringView b) {
            return has_prefix(a, b) or has_prefix(b, a);
        };
        auto unordered_has_suffix = [](StringView a, StringView b) {
            return has_suffix(a, b) or has_suffix(b, a);
        };

        std::vector<std::pair<StringView, StringView>> res;
        for (auto& [slashes, pref_map] : handlers_) {
            std::optional<StringView> prev_url_a;
            foreach_handler(
                pref_map,
                [&,
                 &captured_pref_map =
                     pref_map](StringView pref_a, StringView suff_a, StringView url_a) {
                    // Single url_pattern with two handlers is also a collision
                    if (std::exchange(prev_url_a, url_a) == url_a) {
                        debuglog("collision: ", url_a, ' ', url_a);
                        res.emplace_back(url_a, url_a);
                    }

                    foreach_handler(
                        captured_pref_map,
                        [&](StringView pref_b, StringView suff_b, StringView url_b) {
                            // Consider each pair only once and skip pairs of the form
                            // (x, x)
                            if (url_a.data() >= url_b.data()) {
                                return;
                            }
                            if (unordered_has_prefix(pref_a, pref_b) and
                                unordered_has_suffix(suff_a, suff_b))
                            {
                                debuglog("collision: ", url_a, ' ', url_b);
                                res.emplace_back(url_a, url_b);
                            }
                        }
                    );
                }
            );
        }
        return res;
    }

    [[nodiscard]] std::optional<ResponseT> dispatch(StringView url) const {
        size_t slashes = count_slash(url);
        debuglog("trying slashes: ", slashes);
        auto slash_it = handlers_.find(slashes);
        if (slash_it == handlers_.end()) {
            return std::nullopt;
        }
        auto& prefix_map = slash_it->second;
        auto prefix_it = prefix_map.upper_bound(url);
        while (prefix_it != prefix_map.begin() and has_prefix(url, (--prefix_it)->first)) {
            debuglog("    trying prefix: ", prefix_it->first);
            auto& suffix_map = prefix_it->second;
            auto suffix_it = suffix_map.upper_bound(url);
            while (suffix_it != suffix_map.begin() and has_suffix(url, (--suffix_it)->first)) {
                debuglog("        trying suffix: ", suffix_it->first);
                auto& handlers = suffix_it->second;
                for (auto& [url_pattern, handler] : handlers) {
                    debuglog("            trying ", url_pattern);
                    if (auto res_opt = handler(url); res_opt.has_value()) {
                        return *res_opt;
                    }
                }
            }
        }
        return std::nullopt; // No handler found or all candidates failed at
                             // parsing
    }
};

} // namespace http
