#pragma once

#include <simlib/avl_dict.h>
#include <simlib/sim/judge_worker.h>

class SipJudgeLogger : public sim::JudgeLogger {
	template <class... Args>
	auto log(Args&&... args) {
		return DoubleAppender<decltype(log_)>(stdlog, log_,
		                                      std::forward<Args>(args)...);
	}

	AVLDictMap<EnumVal<sim::JudgeReport::Test::Status>, uint> statistics_;
	bool final_;

	template <class Func>
	void log_test(StringView test_name, sim::JudgeReport::Test test_report,
	              Sandbox::ExitStat es, Func&& func) {
		++statistics_[test_report.status];
		auto tmplog = log(
		   "  ", paddedString(test_name, 8, LEFT), ' ',
		   paddedString(intentionalUnsafeStringView(
		                   toString(floor_to_10ms(test_report.runtime), false)),
		                4),
		   " / ", toString(floor_to_10ms(test_report.time_limit), false), " s ",
		   paddedString(intentionalUnsafeStringView(
		                   humanizeFileSize(test_report.memory_consumed)),
		                7),
		   " / ", humanizeFileSize(test_report.memory_limit), "  ");
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
			tmplog(
			   "\033[1;35mCHECKER ERROR\033[m (Running \033[1;32mOK\033[m)");
			break;
		case sim::JudgeReport::Test::SKIPPED:
			tmplog("\033[1;36mSKIPPED\033[m");
			break;
		}

		// Rest
		tmplog(" [ RT: ", toString(floor_to_10ms(es.runtime), false), " ]");

		func(tmplog);
	}

public:
	void begin(bool final) override { final_ = final; }

	void test(StringView test_name, sim::JudgeReport::Test test_report,
	          Sandbox::ExitStat es) override {
		log_test(test_name, test_report, es, [](auto&) {});
	}

	void test(StringView test_name, sim::JudgeReport::Test test_report,
	          Sandbox::ExitStat es, Sandbox::ExitStat checker_es,
	          Optional<uint64_t> checker_mem_limit,
	          StringView checker_error_str) override {
		log_test(test_name, test_report, es, [&](auto& tmplog) {
			tmplog("  Checker: ");

			// Checker status
			if (test_report.status == sim::JudgeReport::Test::OK)
				tmplog("\033[1;32mOK\033[m ", test_report.comment);
			else if (test_report.status == sim::JudgeReport::Test::WA)
				tmplog("\033[1;31mWA\033[m ", test_report.comment);
			else if (test_report.status ==
			         sim::JudgeReport::Test::CHECKER_ERROR)
				tmplog("\033[1;35mERROR\033[m ", checker_error_str);
			else
				return; // Checker was not run

			tmplog(
			   " [ RT: ", toString(floor_to_10ms(checker_es.runtime), false),
			   " ] ", humanizeFileSize(checker_es.vm_peak));
			if (checker_mem_limit.has_value())
				tmplog(" / ", humanizeFileSize(checker_mem_limit.value()));
		});
	}

	void group_score(int64_t score, int64_t max_score, double) override {
		log("Score: ", score, " / ", max_score);
	}

	void final_score(int64_t total_score, int64_t max_score) override {
		if (final_ and (total_score != 0 or max_score != 0))
			log("Total score: ", total_score, " / ", max_score);
	}

	void end() override {
		if (final_) {
			log("------------------");
			using S = sim::JudgeReport::Test::Status;

			statistics_.for_each([&](auto&& p) {
				auto no = paddedString(
				   intentionalUnsafeStringView(toStr(p.second)), 4);
				switch (p.first) {
				case S::OK: log("\033[1;32mOK\033[m            ", no); break;
				case S::WA: log("\033[1;31mWA\033[m            ", no); break;
				case S::TLE: log("\033[1;33mTLE\033[m           ", no); break;
				case S::MLE: log("\033[1;33mMLE\033[m           ", no); break;
				case S::RTE: log("\033[1;31mRTE\033[m           ", no); break;
				case S::CHECKER_ERROR:
					log("\033[1;35mCHECKER_ERROR\033[m ", no);
					break;
				case S::SKIPPED:
					log("\033[1;36mSKIPPED\033[m       ", no);
					break;
				}
			});
			log("------------------");
		}
	}
};
