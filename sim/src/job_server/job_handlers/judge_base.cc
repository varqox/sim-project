#include "judge_base.hh"

#include <sim/constants.hh>
#include <simlib/enum_val.hh>

namespace job_handlers {

JudgeBase::JudgeBase() {
	jworker_.checker_time_limit = CHECKER_TIME_LIMIT;
	jworker_.checker_memory_limit = CHECKER_MEMORY_LIMIT;
	jworker_.score_cut_lambda = SCORE_CUT_LAMBDA;
}

sim::SolutionLanguage JudgeBase::to_sol_lang(SubmissionLanguage lang) {
	STACK_UNWINDING_MARK;

	switch (lang) {
	case SubmissionLanguage::C11: return sim::SolutionLanguage::C11;
	case SubmissionLanguage::CPP11: return sim::SolutionLanguage::CPP11;
	case SubmissionLanguage::CPP14: return sim::SolutionLanguage::CPP14;
	case SubmissionLanguage::CPP17: return sim::SolutionLanguage::CPP17;
	case SubmissionLanguage::PASCAL: return sim::SolutionLanguage::PASCAL;
	}

	THROW("Invalid Language: ", (int)EnumVal(lang).int_val());
}

InplaceBuff<65536> JudgeBase::construct_report(const sim::JudgeReport& jr,
                                               bool final) {
	STACK_UNWINDING_MARK;

	using sim::JudgeReport;

	InplaceBuff<65536> report;
	if (jr.groups.empty()) {
		return report;
	}

	// clang-format off
	report.append("<table class=\"table\">"
	                  "<thead>"
	                      "<tr>"
	                          "<th class=\"test\">Test</th>"
	                          "<th class=\"result\">Result</th>"
	                          "<th class=\"time\">Time [s]</th>"
	                          "<th class=\"memory\">Memory [KiB]</th>"
	                          "<th class=\"points\">Score</th>"
	                      "</tr>"
	                  "</thead>"
	                  "<tbody>");
	// clang-format on

	auto append_normal_columns = [&](const JudgeReport::Test& test) {
		STACK_UNWINDING_MARK;

		auto as_td_string = [](JudgeReport::Test::Status s) {
			switch (s) {
			case JudgeReport::Test::OK:
				return "<td class=\"status green\">OK</td>";
			case JudgeReport::Test::WA:
				return "<td class=\"status red\">Wrong answer</td>";
			case JudgeReport::Test::TLE:
				return "<td class=\"status yellow\">Time limit exceeded</td>";
			case JudgeReport::Test::MLE:
				return "<td class=\"status yellow\">Memory limit exceeded</td>";
			case JudgeReport::Test::RTE:
				return "<td class=\"status intense-red\">Runtime error</td>";
			case JudgeReport::Test::CHECKER_ERROR:
				return "<td class=\"status blue\">Checker error</td>";
			case JudgeReport::Test::SKIPPED:
				return "<td class=\"status\">Pending</td>";
			}

			throw_assert(false); // We shouldn't get here
		};

		report.append("<td>", html_escape(test.name), "</td>",
		              as_td_string(test.status), "<td>");

		if (test.status == JudgeReport::Test::SKIPPED) {
			report.append('?');
		} else {
			report.append(to_string(floor_to_10ms(test.runtime), false));
		}

		report.append(" / ", to_string(floor_to_10ms(test.time_limit), false),
		              "</td><td>");

		if (test.status == JudgeReport::Test::SKIPPED) {
			report.append('?');
		} else {
			report.append(test.memory_consumed >> 10);
		}

		report.append(" / ", test.memory_limit >> 10, "</td>");
	};

	bool there_are_comments = false;
	for (auto&& group : jr.groups) {
		throw_assert(!group.tests.empty());
		// First row
		report.append("<tr>");
		append_normal_columns(group.tests[0]);
		report.append(R"(<td class="groupscore" rowspan=")", group.tests.size(),
		              "\">", group.score, " / ", group.max_score, "</td></tr>");
		// Other rows
		std::for_each(group.tests.begin() + 1, group.tests.end(),
		              [&](const JudgeReport::Test& test) {
			              report.append("<tr>");
			              append_normal_columns(test);
			              report.append("</tr>");
		              });

		for (auto&& test : group.tests) {
			there_are_comments |= !test.comment.empty();
		}
	}

	report.append("</tbody></table>");

	// Tests comments
	if (there_are_comments) {
		report.append("<ul class=\"tests-comments\">");
		for (auto&& group : jr.groups) {
			for (auto&& test : group.tests) {
				if (!test.comment.empty()) {
					report.append("<li><span class=\"test-id\">",
					              html_escape(test.name), "</span>",
					              html_escape(test.comment), "</li>");
				}
			}
		}

		report.append("</ul>");
	}

	return report;
}

SubmissionStatus JudgeBase::calc_status(const sim::JudgeReport& jr) {
	STACK_UNWINDING_MARK;
	using sim::JudgeReport;

	// Check for judge errors
	for (auto&& group : jr.groups) {
		for (auto&& test : group.tests) {
			if (test.status == JudgeReport::Test::CHECKER_ERROR) {
				return SubmissionStatus::JUDGE_ERROR;
			}
		}
	}

	for (auto&& group : jr.groups) {
		for (auto&& test : group.tests) {
			switch (test.status) {
			case JudgeReport::Test::OK:
			case JudgeReport::Test::SKIPPED: continue;
			case JudgeReport::Test::WA: return SubmissionStatus::WA;
			case JudgeReport::Test::TLE: return SubmissionStatus::TLE;
			case JudgeReport::Test::MLE: return SubmissionStatus::MLE;
			case JudgeReport::Test::RTE: return SubmissionStatus::RTE;
			case JudgeReport::Test::CHECKER_ERROR:
				throw_assert(
				   false); // This should be handled in the above loops
			}
		}
	}

	return SubmissionStatus::OK;
}

void JudgeBase::load_problem_package(FilePath problem_pkg_path) {
	STACK_UNWINDING_MARK;
	if (failed()) {
		return;
	}

	auto tmplog = job_log("Loading problem package...");
	tmplog.flush_no_nl();
	jworker_.load_package(problem_pkg_path, std::nullopt);
	tmplog(" done.");
}

template <class MethodPtr>
std::optional<std::string>
JudgeBase::compile_solution_impl(FilePath solution_path,
                                 sim::SolutionLanguage lang,
                                 MethodPtr compile_method) {
	STACK_UNWINDING_MARK;
	if (failed()) {
		return std::nullopt;
	}

	auto tmplog = job_log("Compiling solution...");
	tmplog.flush_no_nl();

	std::string compilation_errors;
	if ((jworker_.*compile_method)(
	       solution_path, lang, SOLUTION_COMPILATION_TIME_LIMIT,
	       &compilation_errors, COMPILATION_ERRORS_MAX_LENGTH, PROOT_PATH))
	{
		tmplog(" failed:\n", compilation_errors);
		return compilation_errors;
	}

	tmplog(" done.");
	return std::nullopt;
}

std::optional<std::string>
JudgeBase::compile_solution(FilePath solution_path,
                            sim::SolutionLanguage lang) {
	STACK_UNWINDING_MARK;
	return compile_solution_impl(solution_path, lang,
	                             &sim::JudgeWorker::compile_solution);
}

std::optional<std::string>
JudgeBase::compile_solution_from_problem_package(FilePath solution_path,
                                                 sim::SolutionLanguage lang) {
	STACK_UNWINDING_MARK;
	return compile_solution_impl(
	   solution_path, lang, &sim::JudgeWorker::compile_solution_from_package);
}

std::optional<std::string> JudgeBase::compile_checker() {
	STACK_UNWINDING_MARK;
	if (failed()) {
		return std::nullopt;
	}

	auto tmplog = job_log("Compiling checker...");
	tmplog.flush_no_nl();

	std::string compilation_errors;
	if (jworker_.compile_checker(SOLUTION_COMPILATION_TIME_LIMIT,
	                             &compilation_errors,
	                             COMPILATION_ERRORS_MAX_LENGTH, PROOT_PATH))
	{
		tmplog(" failed:\n", compilation_errors);
		return compilation_errors;
	}

	tmplog(" done.");
	return std::nullopt;
}

} // namespace job_handlers
