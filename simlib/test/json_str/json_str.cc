#include "simlib/json_str/json_str.hh"

#include <gtest/gtest.h>

// NOLINTNEXTLINE
TEST(json_str, empty_obj) {
    json_str::Object obj;
    EXPECT_EQ(std::move(obj).into_str(), "{}");
}

// NOLINTNEXTLINE
TEST(json_str, single_prop_obj) {
    json_str::Object obj;
    obj.prop("name", "value");
    EXPECT_EQ(std::move(obj).into_str(), "{\"name\":\"value\"}");
}

// NOLINTNEXTLINE
TEST(json_str, empty_arr) {
    json_str::Array arr;
    EXPECT_EQ(std::move(arr).into_str(), "[]");
}

// NOLINTNEXTLINE
TEST(json_str, single_elem_arr) {
    json_str::Array arr;
    arr.val("test test");
    EXPECT_EQ(std::move(arr).into_str(), "[\"test test\"]");
}

// NOLINTNEXTLINE
TEST(json_str, integration) {
    json_str::Object obj;
    obj.prop("test", 42);
    obj.prop_arr("foo", [&](auto& arr) {
        arr.val(42);
        arr.val(true);
        arr.val_arr([&](auto& arr) {
            arr.val(1);
            arr.val(2);
            arr.val(std::nullopt);
            arr.val(std::optional{3});
            arr.val(nullptr);
        });
        arr.val_obj([&](auto& obj) {
            obj.prop("abc", "xd");
            obj.prop("hohoho", 42);
        });
        arr.val("hohoho");
        arr.val("xd\n\rxd");
    });
    EXPECT_EQ(
        std::move(obj).into_str(),
        "{\"test\":42,\"foo\":[42,true,[1,2,null,3,null],{\"abc\":\"xd\",\"hohoho\":42},"
        "\"hohoho\",\"xd\\n\\u000dxd\"]}");
}
