#pragma once

#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <string>

int recursive_readlink(std::string path, std::string& res);

inline std::string recursive_readlink_throw(std::string path) {
    std::string res;
    if (recursive_readlink(std::move(path), res)) {
        THROW("recursive_readlink()", errmsg());
    }
    return res;
}
