#pragma once

#include "common.hh"

#include <cstdint>
#include <optional>
#include <simlib/concat_tostr.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/sim/judge/test_report.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/time_format_conversions.hh>
#include <string>

namespace job_server::job_handlers {

class JudgeLogger : public sim::JudgeLogger {
    Logger& logger;
    bool final = false;
    bool after_final_score = false;
    std::optional<std::string> last_gid;

public:
    explicit JudgeLogger(Logger& logger_) : logger{logger_} {}

    JudgeLogger(const JudgeLogger&) = delete;
    JudgeLogger(JudgeLogger&&) = delete;
    JudgeLogger& operator=(const JudgeLogger&) = delete;
    JudgeLogger& operator=(JudgeLogger&&) = delete;
    ~JudgeLogger() = default;

    void begin(bool final_) override {
        final = final_;
        after_final_score = false;
        last_gid = std::nullopt;
        logger("Judging (", final ? "final" : "initial", "): {");
    }

    void test(
        StringView test_name,
        sim::JudgeReport::Test test_report,
        sim::judge::TestReport judge_test_report,
        uint64_t checker_mem_limit
    ) override {
        if (after_final_score) {
            auto gid = sim::Simfile::TestNameComparator::split(test_name).gid;
            if (gid != last_gid) {
                logger("");
            }
            last_gid = gid.to_string();
        }

        std::string line;
        back_insert(
            line,
            "  ",
            padded_string(test_name, 12, LEFT),
            ' ',
            padded_string(from_unsafe{::to_string(floor_to_10ms(test_report.runtime), false)}, 4),
            " / ",
            ::to_string(floor_to_10ms(test_report.time_limit), false),
            " s  ",
            test_report.memory_consumed >> 10,
            " / ",
            test_report.memory_limit >> 10,
            " KiB  Status: "
        );
        switch (test_report.status) {
        case sim::JudgeReport::Test::TLE: back_insert(line, "\033[1;33mTLE\033[m"); break;
        case sim::JudgeReport::Test::MLE: back_insert(line, "\033[1;33mMLE\033[m"); break;
        case sim::JudgeReport::Test::OLE: back_insert(line, "\033[1;33mOLE\033[m"); break;
        case sim::JudgeReport::Test::RTE: back_insert(line, "\033[1;31mRTE\033[m"); break;
        case sim::JudgeReport::Test::OK: back_insert(line, "\033[1;32mOK\033[m"); break;
        case sim::JudgeReport::Test::WA: back_insert(line, "\033[1;31mWA\033[m"); break;
        case sim::JudgeReport::Test::CHECKER_ERROR:
            back_insert(line, "\033[1;35mCHECKER ERROR\033[m");
            break;
        case sim::JudgeReport::Test::SKIPPED: back_insert(line, "\033[1;36mSKIPPED\033[m"); break;
        }
        if (!test_report.comment.empty()) {
            back_insert(line, " (", test_report.comment, ')');
        }

        back_insert(
            line,
            " [ CPU: ",
            ::to_string(judge_test_report.program.cpu_time, false),
            " RT: ",
            ::to_string(judge_test_report.program.runtime, false),
            " ]"
        );
        if (judge_test_report.checker) {
            back_insert(
                line,
                "  Checker: [ CPU: ",
                ::to_string(judge_test_report.checker->cpu_time, false),
                " RT: ",
                ::to_string(judge_test_report.checker->runtime, false),
                " ] ",
                judge_test_report.checker->peak_memory_in_bytes >> 10,
                " / ",
                checker_mem_limit >> 10,
                " KiB"
            );
        }
        logger(line);
    }

    void group_score(int64_t score, int64_t max_score, double score_ratio) override {
        logger("Score: ", score, " / ", max_score, " (ratio: ", ::to_string(score_ratio, 4), ')');
    }

    void final_score(int64_t total_score, int64_t max_score) override {
        after_final_score = true;
        if (final) {
            logger("Total score: ", total_score, " / ", max_score);
        }
    }

    void end() override { logger('}'); }
};

} // namespace job_server::job_handlers
