#pragma once

#include <initializer_list>
#include <simlib/sandbox/sandbox.hh>
#include <sys/wait.h>
#include <variant>

class OneOfSiCodes {
    std::initializer_list<int> si_codes;

public:
    OneOfSiCodes(std::initializer_list<int> si_codes) : si_codes(si_codes) {}

    [[nodiscard]] bool contains(int si_code) noexcept {
        // NOLINTNEXTLINE
        for (auto sc : si_codes) {
            if (sc == si_code) {
                return true;
            }
        }
        return false;
    }
};

#define ASSERT_RESULT_OK(res, si_code, si_status)                                           \
    for (auto _assert_result_ok_i : {0, 1})                                                 \
        if (_assert_result_ok_i == 0) {                                                     \
            auto _assert_result_ok_res = (res);                                             \
            if (std::holds_alternative<::sandbox::result::Ok>(_assert_result_ok_res)) {     \
                bool _assert_result_ok_passed = true;                                       \
                auto& _assert_result_ok_si =                                                \
                    std::get<::sandbox::result::Ok>(_assert_result_ok_res).si;              \
                [&](auto _si_code) {                                                        \
                    if constexpr (std::is_same_v<decltype(si_code), OneOfSiCodes>) {        \
                        EXPECT_TRUE(_si_code.contains(_assert_result_ok_si.code))           \
                            << (_assert_result_ok_passed = false, "")                       \
                            << _assert_result_ok_si.description();                          \
                        EXPECT_EQ(_assert_result_ok_si.status, si_status)                   \
                            << (_assert_result_ok_passed = false, "")                       \
                            << _assert_result_ok_si.description();                          \
                    } else {                                                                \
                        EXPECT_EQ(                                                          \
                            _assert_result_ok_si,                                           \
                            (::sandbox::Si{.code = _si_code, .status = (si_status)})        \
                        ) << (_assert_result_ok_passed = false, "")                         \
                          << _assert_result_ok_si.description();                            \
                    }                                                                       \
                }(si_code);                                                                 \
                if (_assert_result_ok_passed) {                                             \
                    break;                                                                  \
                }                                                                           \
            } else {                                                                        \
                ADD_FAILURE(                                                                \
                ) << "unexpected result = Error with description:\n  "                      \
                  << std::get<::sandbox::result::Error>(_assert_result_ok_res).description; \
            }                                                                               \
        } else                                                                              \
            FAIL()

#define ASSERT_RESULT_ERROR(res, err_description)                                                \
    for (auto _assert_result_error_i : {0, 1})                                                   \
        if (_assert_result_error_i == 0) {                                                       \
            auto _assert_result_error_res = (res);                                               \
            if (std::holds_alternative<::sandbox::result::Error>(_assert_result_error_res)) {    \
                bool _assert_result_error_passed = true;                                         \
                EXPECT_EQ(                                                                       \
                    std::get<::sandbox::result::Error>(_assert_result_error_res).description,    \
                    err_description                                                              \
                ) << (_assert_result_error_passed = false, "");                                  \
                if (_assert_result_error_passed) {                                               \
                    break;                                                                       \
                }                                                                                \
            } else {                                                                             \
                ADD_FAILURE(                                                                     \
                ) << "unexpected result = Ok with si:\n  Si{.code = "                            \
                  << std::get<::sandbox::result::Ok>(_assert_result_error_res).si.code           \
                  << ", .status = "                                                              \
                  << std::get<::sandbox::result::Ok>(_assert_result_error_res).si.status << '}'; \
            }                                                                                    \
        } else                                                                                   \
            FAIL()
