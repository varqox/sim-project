#pragma once

#include <simlib/sandbox/sandbox.hh>
#include <variant>

#define ASSERT_RESULT_OK(res, si_code, si_status)                                               \
    for (auto _assert_result_ok_i : {0, 1})                                                     \
        if (_assert_result_ok_i == 0) {                                                         \
            auto _assert_result_ok_res = (res);                                                 \
            if (std::holds_alternative<::sandbox::result::Ok>(_assert_result_ok_res)) {         \
                bool _assert_result_ok_passed = true;                                           \
                auto& _assert_result_ok_si =                                                    \
                    std::get<::sandbox::result::Ok>(_assert_result_ok_res).si;                  \
                EXPECT_EQ(                                                                      \
                    _assert_result_ok_si, (::sandbox::Si{.code = si_code, .status = si_status}) \
                ) << (_assert_result_ok_passed = false, "")                                     \
                  << _assert_result_ok_si.description();                                        \
                if (_assert_result_ok_passed) {                                                 \
                    break;                                                                      \
                }                                                                               \
            } else {                                                                            \
                ADD_FAILURE(                                                                    \
                ) << "unexpected result = Error with description:\n  "                          \
                  << std::get<::sandbox::result::Error>(_assert_result_ok_res).description;     \
            }                                                                                   \
        } else                                                                                  \
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
