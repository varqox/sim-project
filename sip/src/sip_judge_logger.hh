#pragma once

#include "simlib/enum_val.hh"
#include "simlib/humanize.hh"
#include "simlib/sim/judge_worker.hh"
#include "simlib/sim/simfile.hh"
#include "simlib/string_traits.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"
#include "simlib/time.hh"

#include <cstddef>
#include <cstdint>
#include <map>

extern bool sip_verbose;

class SipJudgeLogger : public sim::JudgeLogger {
    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    auto log(Args&&... args) {
        return DoubleAppender<decltype(log_)>(stdlog, log_, std::forward<Args>(args)...);
    }

    size_t test_name_max_len_ = 0;
    std::map<EnumVal<sim::JudgeReport::Test::Status>, uint> status_count_;
    bool final_ = false;
    std::optional<std::pair<int64_t, int64_t>> total_and_max_score_;

    static std::string mem_to_str(uint64_t bytes, bool pad = true) {
        auto str = humanize_file_size(bytes);
        // Padding
        if (pad) {
            str = padded_string(str, 8, RIGHT).to_string();
        }
        if (not sip_verbose) {
            // Shorten unit suffixes
            if (has_suffix(str, "iB")) {
                str.erase(str.end() - 2, str.end());
            } else if (has_suffix(str, "bytes")) {
                str.erase(str.end() - 4, str.end());
            } else if (has_suffix(str, "byte")) {
                str.erase(str.end() - 3, str.end());
            }

            // Add coloring
            if (str.size() >= 2 and str.end()[-2] == ' ') {
                auto c = str.back();
                str.resize(str.size() - 2);
                str += "\033[2m";
                str += to_lower(c);
                str += "\033[m";
            }
        }
        return str;
    }

    template <class Func>
    void log_test(
        StringView test_name, const sim::JudgeReport::Test& test_report, Sandbox::ExitStat es,
        Func&& func) {
        assert(test_name.size() <= test_name_max_len_);
        ++status_count_[test_report.status];
        auto tmplog =
            log(' ', padded_string(test_name, test_name_max_len_, LEFT), "  ",
                padded_string(
                    intentional_unsafe_string_view(
                        to_string(floor_to_10ms(test_report.runtime), false)),
                    4),
                " / ", to_string(floor_to_10ms(test_report.time_limit), false),
                "\033[2ms\033[m ", mem_to_str(test_report.memory_consumed), " / ",
                mem_to_str(test_report.memory_limit, false), ' ');
        // Status
        switch (test_report.status) {
        case sim::JudgeReport::Test::TLE: tmplog("\033[1;33mTLE\033[m"); break;
        case sim::JudgeReport::Test::MLE: tmplog("\033[1;33mMLE\033[m"); break;
        case sim::JudgeReport::Test::RTE:
            tmplog("\033[1;31mRTE\033[m (", es.message, ')');
            break;
        case sim::JudgeReport::Test::OK: tmplog("\033[1;32mOK\033[m"); break;
        case sim::JudgeReport::Test::WA: tmplog("\033[1;31mWA\033[m"); break;
        case sim::JudgeReport::Test::CHECKER_ERROR:
            tmplog("\033[1;35mCHECKER ERROR\033[m (Running \033[1;32mOK\033[m)");
            break;
        case sim::JudgeReport::Test::SKIPPED: tmplog("\033[1;36mSKIPPED\033[m"); break;
        }

        // Rest
        if (test_report.status != sim::JudgeReport::Test::OK or sip_verbose) {
            tmplog(" \033[2m[RT: ", to_string(floor_to_10ms(es.runtime), false), "]\033[m");
        }

        func(tmplog);
    }

public:
    explicit SipJudgeLogger(const sim::Simfile& simfile) {
        for (auto const& tg : simfile.tgroups) {
            for (auto const& test : tg.tests) {
                test_name_max_len_ = std::max(test_name_max_len_, test.name.size());
            }
        }
    }

    void begin(bool final) override { final_ = final; }

    void test(StringView test_name, sim::JudgeReport::Test test_report, Sandbox::ExitStat es)
        override {
        log_test(test_name, test_report, es, [](auto& /*unused*/) {});
    }

    void test(
        StringView test_name, sim::JudgeReport::Test test_report, Sandbox::ExitStat es,
        Sandbox::ExitStat checker_es, std::optional<uint64_t> checker_mem_limit,
        StringView checker_error_str) override {
        log_test(test_name, test_report, es, [&](auto& tmplog) {
            // Checker status
            if (test_report.status == sim::JudgeReport::Test::OK) {
                if (sip_verbose) {
                    tmplog(" Checker: \033[1;32mOK\033[m");
                }
                tmplog(' ', test_report.comment);
            } else if (test_report.status == sim::JudgeReport::Test::WA) {
                if (sip_verbose) {
                    tmplog(" Checker: \033[1;31mWA\033[m");
                }
                tmplog(' ', test_report.comment);
            } else if (test_report.status == sim::JudgeReport::Test::CHECKER_ERROR) {
                tmplog(" Checker: \033[1;35mERROR\033[m ", checker_error_str);
            } else {
                return; // Checker was not run
            }

            if (test_report.status == sim::JudgeReport::Test::CHECKER_ERROR or sip_verbose) {
                tmplog(
                    " \033[2m[RT: ", to_string(floor_to_10ms(checker_es.runtime), false),
                    "]\033[m ", mem_to_str(checker_es.vm_peak));
                if (checker_mem_limit.has_value()) {
                    tmplog(" / ", mem_to_str(checker_mem_limit.value(), false));
                }
            }
        });
    }

    void group_score(int64_t score, int64_t max_score, double /*score_ratio*/) override {
        log("Score: ", score, " / ", max_score);
    }

    void final_score(int64_t total_score, int64_t max_score) override {
        if (final_ and (total_score != 0 or max_score != 0)) {
            total_and_max_score_ = {total_score, max_score};
        }
    }

    void end() override {
        if (final_) {
            log("------------------");
            using S = sim::JudgeReport::Test::Status;

            for (auto const& [status, count] : status_count_) {
                auto num = padded_string(intentional_unsafe_string_view(to_string(count)), 4);
                switch (status) {
                case S::OK: log("\033[1;32mOK\033[m            ", num); break;
                case S::WA: log("\033[1;31mWA\033[m            ", num); break;
                case S::TLE: log("\033[1;33mTLE\033[m           ", num); break;
                case S::MLE: log("\033[1;33mMLE\033[m           ", num); break;
                case S::RTE: log("\033[1;31mRTE\033[m           ", num); break;
                case S::CHECKER_ERROR: log("\033[1;35mCHECKER_ERROR\033[m ", num); break;
                case S::SKIPPED: log("\033[1;36mSKIPPED\033[m       ", num); break;
                }
            }
            log("------------------");
            if (total_and_max_score_) {
                auto const& [total_score, max_score] = *total_and_max_score_;
                log("Total score: \033[1m", total_score, " / ", max_score, "\033[m");
            }
        }
    }
};
