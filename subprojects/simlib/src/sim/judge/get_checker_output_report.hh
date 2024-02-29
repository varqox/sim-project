#pragma once

#include <cassert>
#include <cstdint>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/simple_parser.hh>
#include <simlib/string_transform.hh>
#include <simlib/utilities.hh>
#include <string>

namespace sim::judge {

struct CheckerOutputReport {
    enum class Status {
        CheckerError,
        WrongAnswer,
        OK,
    } status;
    std::string comment;
    double score; // in range [0, 1]
};

inline CheckerOutputReport
get_checker_output_report(FileDescriptor checker_output_fd, uint64_t max_comment_len) {
    auto checker_output =
        get_file_contents(checker_output_fd, 0, static_cast<off_t>(max_comment_len) + 32);
    SimpleParser parser(checker_output);

    auto line1 = parser.extract_next('\n');
    if (!is_one_of(line1, "OK", "WRONG")) {
        return {
            .status = CheckerOutputReport::Status::CheckerError,
            .comment = R"(Checker error: invalid first line (expected "OK" or "WRONG"))",
            .score = 0,
        };
    }

    auto line2 = parser.extract_next('\n');
    CheckerOutputReport::Status status;
    double score;
    if (line1 == "OK") {
        status = CheckerOutputReport::Status::OK;
        if (!line2.empty()) {
            auto res = str2num<double>(line2, 0, 100);
            if (!res) {
                return {
                    .status = CheckerOutputReport::Status::CheckerError,
                    .comment = "Checker error: invalid second line (expected real number in range "
                               "[0, 100] or empty line)",
                    .score = 0,
                };
            }
            score = *res * 0.01;
        } else {
            score = 1;
        }
    } else {
        status = CheckerOutputReport::Status::WrongAnswer;
        score = 0;
    }

    // Leave only the comment
    checker_output.erase(checker_output.begin(), checker_output.end() - parser.size());
    // Remove trailing whitespace
    while (!checker_output.empty() && is_space(checker_output.back())) {
        checker_output.pop_back();
    }
    // Trim the comment if necessary
    if (checker_output.size() > max_comment_len) {
        checker_output.resize(max_comment_len);
    }

    return {
        .status = status,
        .comment = std::move(checker_output),
        .score = score,
    };
}

} // namespace sim::judge
