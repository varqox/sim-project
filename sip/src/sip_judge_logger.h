#pragma once

#include <simlib/sim/judge_worker.h>

class SipJudgeLogger : public sim::JudgeLogger {
	template<class... Args>
	auto log(Args&&... args) {
		return DoubleAppender<decltype(log_)>(stdlog, log_,
			std::forward<Args>(args)...);
	}

	int64_t total_score_;
	int64_t max_total_score_;
	AVLDictMap<EnumVal<sim::JudgeReport::Test::Status>, uint> statistics_;
	bool final_;

	template<class Func>
	void log_test(StringView test_name, sim::JudgeReport::Test test_report,
		Sandbox::ExitStat es, Func&& func)
	{
		auto tmplog = log("  ", paddedString(test_name, 8, LEFT),
			paddedString(intentionalUnsafeStringView(
				usecToSecStr(test_report.runtime, 2, false)), 4),
			" / ", usecToSecStr(test_report.time_limit, 2, false),
			" s ",
			paddedString(intentionalUnsafeStringView(
				humanizeFileSize(test_report.memory_consumed)), 7),
			" / ", humanizeFileSize(test_report.memory_limit), "  ");
		// Status
		switch (test_report.status) {
		case sim::JudgeReport::Test::TLE: tmplog("\033[1;33mTLE\033[m");
			break;
		case sim::JudgeReport::Test::MLE: tmplog("\033[1;33mMLE\033[m");
			break;
		case sim::JudgeReport::Test::RTE:
			tmplog("\033[1;31mRTE\033[m (", es.message, ')');
			break;
		case sim::JudgeReport::Test::OK:
			tmplog("\033[1;32mOK\033[m");
			break;
		case sim::JudgeReport::Test::WA:
			tmplog("\033[1;31mWA\033[m");
			break;
		case sim::JudgeReport::Test::CHECKER_ERROR:
			tmplog("\033[1;35mCHECKER ERROR\033[m (Running \033[1;32mOK\033[m)");
			break;
		}

		// Rest
		// tmplog(" [ CPU: ", timespec_to_str(es.cpu_runtime, 2, false),
			// " R: ", timespec_to_str(es.runtime, 2, false), " ]");
		tmplog(" [ RT: ", timespec_to_str(es.runtime, 2, false), " ]");

		func(tmplog);
	}

public:
	void begin(StringView, bool final) override {
		total_score_ = max_total_score_ = 0;
		final_ = final;
	}

	void test(StringView test_name, sim::JudgeReport::Test test_report,
		Sandbox::ExitStat es) override
	{
		++statistics_[test_report.status];
		log_test(test_name, test_report, es, [](auto&){});
	}

	void test(StringView test_name, sim::JudgeReport::Test test_report,
		Sandbox::ExitStat es, Sandbox::ExitStat checker_es, uint64_t checker_mem_limit, StringView checker_error_str) override
	{
		++statistics_[test_report.status];
		log_test(test_name, test_report, es, [&](auto& tmplog) {
			tmplog("  Checker: ");

			// Checker status
			if (test_report.status == sim::JudgeReport::Test::OK)
				tmplog("\033[1;32mOK\033[m ", test_report.comment);
			else if (test_report.status == sim::JudgeReport::Test::WA)
				tmplog("\033[1;31mWA\033[m ", test_report.comment);
			else if (test_report.status == sim::JudgeReport::Test::CHECKER_ERROR)
				tmplog("\033[1;35mERROR\033[m ", checker_error_str);
			else
				return; // Checker was not run


			tmplog(" [ RT: ", timespec_to_str(checker_es.runtime, 2, false), " ] ",
				humanizeFileSize(checker_es.vm_peak), " / ",
				humanizeFileSize(checker_mem_limit));
		});
	}

	void group_score(int64_t score, int64_t max_score, double) override {
		total_score_ += score;
		if (max_score > 0)
			max_total_score_ += max_score;

		log("Score: ", score, " / ", max_score);
	}

	void end() override {
		if (final_) {
			log("Total score: ", total_score_, " / ", max_total_score_);
			log("------------------");
			using S = sim::JudgeReport::Test::Status;
			statistics_.for_each([&](auto&& p) {
				switch (p.first) {
				case S::OK: log("\033[1;32mOK\033[m\t\t", p.second); break;
				case S::WA: log("\033[1;31mWA\033[m\t\t", p.second); break;
				case S::TLE: log("\033[1;33mTLE\033[m\t\t", p.second); break;
				case S::MLE: log("\033[1;33mMLE\033[m\t\t", p.second); break;
				case S::RTE: log("\033[1;31mRTE\033[m\t\t", p.second); break;
				case S::CHECKER_ERROR:
					log("\033[1;35mCHECKER_ERROR\033[m\t", p.second);
					break;
				}
			});
			log("------------------");
		}
	}
};
