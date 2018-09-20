#include "commands.h"
#include "compilation_cache.h"
#include "sip.h"
#include "sip_judge_logger.h"

#include <poll.h>
#include <simlib/optional.h>
#include <simlib/sim/conver.h>
#include <simlib/sim/problem_package.h>
#include <sys/inotify.h>

using sim::Simfile;
using std::string;

/// Returns default time limit specified in Sipfile in microseconds
static uint64_t default_time_limit() {
	if (access("Sipfile", F_OK) != 0)
		return 5e6; // 5 seconds

	ConfigFile sipfile;
	sipfile.addVars("default_time_limit");
	sipfile.loadConfigFromFile("Sipfile");
	auto def_tl = sipfile.getVar("default_time_limit");
	if (def_tl.isSet()) {
		if (not def_tl.isArray() and isReal(def_tl.asString())) {
			return def_tl.asDouble() * 1e6;
		} else {
			stdlog("\033[1;35mwarning\033[m: invalid time limit was set in the"
				" Sipfile");
		}
	}

	return 5e6; // 5 seconds
}

static Simfile construct_full_simfile() {
	sim::Conver conver;
	conver.setPackagePath(".");

	// Set Conver options
	sim::Conver::Options copts;
	copts.max_time_limit = default_time_limit();
	copts.reset_time_limits_using_model_solution = false;
	copts.ignore_simfile = false;
	copts.seek_for_new_tests = true;
	copts.reset_scoring = false;
	copts.require_statement = false;

	sim::Conver::ConstructionResult cr = conver.constructSimfile(copts);
	if (verbose) {
		stdlog(conver.getReport());
	} else {
		// Filter out ever line except warnings
		StringView report = conver.getReport();
		size_t line_beg = 0, line_end = report.find('\n');
		size_t next_warning = report.find("warning");
		while (next_warning != StringView::npos) {
			if (next_warning < line_end)
				stdlog(report.substring(line_beg, line_end));

			if (line_end == StringView::npos or line_end + 1 == report.size())
				break;

			line_beg = line_end + 1;
			line_end = report.find('\n', line_beg);
			if (next_warning < line_beg)
				next_warning = report.find(line_beg, "warning");
		}
	}

	// Ignore the need for the model solution to set the time limits - the
	// time limits will be set to max_time_limit
	// throw_assert(cr.status == sim::Conver::Status::COMPLETE);

#if __cplusplus > 201402L
#warning "Since C++17 we have guaranteed copy elision, so remove the 'std::move'"
#endif
	return std::move(cr.simfile);
}

static void update_time_limits(Simfile& sf, const sim::JudgeReport& jrep1,
	const sim::JudgeReport& jrep2)
{
	sim::Conver::finishConstructingSimfile(sf, jrep1, jrep2);

	// Dump the new time limits to Simfile

	// We do not check for the existence of the Simfile as it is necessary for
	// the Sip to run judging first
	std::string val("[");
	for (auto const& group : sf.tgroups) {
		if (group.tests.size())
			val += '\n';
		for (auto const& test : group.tests) {
			string line {concat_tostr(test.name, ' ',
				usecToSecStr(test.time_limit, 6))};

			if (test.memory_limit != sf.global_mem_limit)
				back_insert(line, ' ', test.memory_limit >> 20);

			back_insert(val, '\t', ConfigFile::escapeString(line), '\n');
		}
	}
	val += "]";

	replace_var_in_simfile(sf, "Simfile", getFileContents("Simfile"), "limits",
		val, false);
}

static inline bool is_subsequence(StringView sequence, StringView str) noexcept {
	if (sequence.empty())
		return true;

	size_t i = 0;
	for (char c : str)
		if (c == sequence[i] and ++i == sequence.size())
			return true;

	return false;
}

static AVLDictSet<StringView> parse_args_to_solutions(Simfile& simfile,
	ArgvParser args)
{
	if (args.size() == 0)
		return {};

	AVLDictSet<StringView> choosen_solutions;
	do {
		auto arg = args.extract_next();
		// If a path to solution was provided then choose it
		auto it = std::find(simfile.solutions.begin(), simfile.solutions.end(), arg);
		if (it != simfile.solutions.end()) {
			choosen_solutions.emplace(*it);
		// There is no solution with path equal to the provided path, so
		// choose all that contain arg as a subsequence
		} else {
			throw_assert(is_subsequence("abc", "a b c"));

			for (auto const& solution : simfile.solutions)
				if (is_subsequence(arg, solution))
					choosen_solutions.emplace(solution);
		}
	} while (args.size() > 0);

	return choosen_solutions;
}

static inline uint64_t timespec_to_usec(timespec t) noexcept {
	return t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

static inline bool isnt_space(int c) noexcept { return not isspace(c); };

static inline bool isnt_slash(char c) noexcept { return (c != '/'); };

// Returns whether the output was generated successfully
static bool gen_test_out(sim::JudgeWorker& jworker, CStringView input,
	CStringView output, uint64_t time_limit, uint64_t memory_limit,
	StringView pre_gen_comment)
{
	auto tmplog = stdlog(pre_gen_comment);
	tmplog.flush_no_nl();

	auto es = jworker.run_solution(input, output, time_limit, memory_limit);

	uint64_t runtime = timespec_to_usec(es.cpu_runtime);
	if (es.si.code == CLD_EXITED and es.si.status == 0 and runtime <= time_limit)
	{
		tmplog(" \033[1;32mdone\033[m in ",
			usecToSecStr(runtime, 2, false), " s");
		return true;

	} else if (runtime >= time_limit or
		timespec_to_usec(es.runtime) == time_limit)
	{
		tmplog(" \033[1;33mtime limit exceeded\033[m - ",
			usecToSecStr(runtime, 2, false), " s");
		return false;

	} else if (es.message == "Memory limit exceeded" or
		es.vm_peak > memory_limit)
	{
		tmplog(" \033[1;33mmemory limit exceeded\033[m - ",
			usecToSecStr(runtime, 2, false), " s");
		return false;

	} else {
		tmplog(" \033[1;31mruntime error\033[m - ",
			usecToSecStr(runtime, 2, false), " s (", es.message, ')');
		return false;
	}
}

// Returns whether all was generated successfully
static bool gen_impl(bool generate_inputs, bool ensure_all_tests_are_in_sipfile, bool delete_invalid_test_files)
{
	auto parse_test_range = [](StringView test_range, auto&& callback) {
		auto hypen = test_range.find('-');
		if (hypen == StringView::npos)
			return callback(test_range);

		if (hypen + 1 == test_range.size()) {
			errlog("\033[1;31mError\033[m: Invalid test range"
				" (trailing hypen): ", test_range);
			exit(1);
		}

		StringView begin = test_range.substring(0, hypen);
		StringView end = test_range.substring(hypen + 1);

		auto begin_test = Simfile::TestNameComparator::split(begin);
		auto end_test = Simfile::TestNameComparator::split(end);
		// The part before group id
		StringView prefix = begin.substring(0,
			begin.size() - begin_test.gid.size() - begin_test.tid.size());
		{
			// Allow e.g. test4a-5c
			StringView end_prefix = end.substring(0,
				end.size() - end_test.gid.size() - end_test.tid.size());
			if (not end_prefix.empty() and end_prefix != prefix) {
				errlog("\033[1;31mError\033[m: Invalid test range (test prefix"
					" of the end test is not empty and does not match the begin"
					" test prefix): ", test_range);
				exit(1);
			}
		}

		if (not isDigitNotGreaterThan<ULLONG_MAX>(begin_test.gid)) {
			errlog("\033[1;31mError\033[m: Invalid test range (group id is too"
				" big): ", test_range);
			exit(1);
		}

		unsigned long long begin_gid = strtoull(begin_test.gid);
		unsigned long long end_gid;

		if (end_test.gid.empty()) { // Allow e.g. 4a-d
			end_gid = begin_gid;
		} else if (not isDigitNotGreaterThan<ULLONG_MAX>(end_test.gid)) {
			errlog("\033[1;31mError\033[m: Invalid test range (group id is too"
				" big): ", test_range);
			exit(1);
		} else {
			end_gid = strtoull(end_test.gid);
		}

		string begin_tid = tolower(begin_test.tid.to_string());
		string end_tid = tolower(end_test.tid.to_string());

		// Allow e.g. 4-8c
		if (begin_tid.empty())
			begin_tid = end_tid;

		if (begin_tid.size() != end_tid.size()) {
			errlog("\033[1;31mError\033[m: Invalid test range (test IDs have"
				" different length): ", test_range);
			exit(1);
		}

		if (begin_tid > end_tid) {
			errlog("\033[1;31mError\033[m: Invalid test range (begin test ID is"
				" greater than end test ID): ", test_range);
			exit(1);
		}

		auto inc_tid = [](auto& tid) {
			if (tid.size == 0)
				return;

			++tid.back();
			for (int i = tid.size - 1; i > 0 and tid[i] > 'z'; --i) {
				tid[i] -= ('z' - 'a');
				++tid[i - 1];
			}
		};

		for (auto gid = begin_gid; gid <= end_gid; ++gid)
			for (InplaceBuff<8> tid(begin_tid); tid <= end_tid; inc_tid(tid))
				callback(concat(prefix, gid, tid));
	};

	// Collect tests specified in the Sipfile
	ConfigFile sipfile;
	sipfile.addVars("static", "gen");
	sipfile.loadConfigFromFile("Sipfile");
	auto static_tests_var = sipfile.getVar("static");
	auto gen_tests_var = sipfile.getVar("gen");

	AVLDictSet<InplaceBuff<16>> static_tests;

	if (static_tests_var.isSet()) {
		if (not static_tests_var.isArray()) {
			errlog("\033[1;31mError\033[m: Sipfile: 'static' has to be an array");
			return 1;
		}

		for (StringView entry : static_tests_var.asArray()) {
			entry.extractLeading(isspace);
			StringView test_range = entry.extractLeading(isnt_space);

			entry.extractLeading(isspace);
			if (not entry.empty())
				stdlog("\033[1;35mwarning\033[m: ignoring invalid suffix: `",
					entry, "` of the entry with test range: ", test_range);

			parse_test_range(test_range, [&](StringView test) {
				bool inserted = static_tests.emplace(test);
				if (not inserted) {
					errlog("\033[1;31mError\033[m: Sipfile: static test: ",
						test, " is specified in more than one test range");
					exit(1);
				}
			});
		}
	}

	struct GenTest {
		InplaceBuff<16> name;
		InplaceBuff<16> generator;
		InplaceBuff<32> generator_args;

		GenTest(StringView n, StringView g, StringView ga)
			: name(n), generator(g), generator_args(ga) {}
	};

	sim::PackageContents pc;
	pc.load_from_directory(".");
	pc.remove_with_prefix("utils/cache/");

	AVLDictSet<GenTest, MEMBER_COMPARATOR(GenTest, name)> gen_tests;

	if (gen_tests_var.isSet()) {
		if (not gen_tests_var.isArray()) {
			errlog("\033[1;31mError\033[m: Sipfile: 'static' has to be an array");
			return 1;
		}

		for (StringView entry : gen_tests_var.asArray()) {
			entry.extractLeading(isspace);
			StringView test_range = entry.extractLeading(isnt_space);

			entry.extractLeading(isspace);
			StringView specified_generator = entry.extractLeading(isnt_space);
			InplaceBuff<32> generator;
			// Infer which generator to use
			if (hasPrefix(specified_generator, "sh:")) {
				generator = specified_generator;
			} else if (pc.exists(specified_generator)) {
				generator = specified_generator;
			// } else if (pc.exists(concat("utils/", specified_generator))) {
			// 	generator = concat<32>("utils/", specified_generator);
			} else {
				// Scan utils for the generator as a subsequence
				bool found = false;
				pc.for_each_with_prefix("utils/", [&](StringView file) {
					if (is_subsequence(specified_generator, file) and
						sim::is_source(file))
					{
						if (found) {
							errlog("\033[1;31mError\033[m: Sipfile: specified"
								" generator '", specified_generator,
								"' matches more than one file: '", generator,
								"' and '", file, "'");
							exit(1);
						} else {
							generator = file;
							found = true;
						}
					}
				});

				// Scan whole package for the generator as a subsequence
				if (not found)
					pc.for_each_with_prefix("", [&](StringView file) {
						if (is_subsequence(specified_generator, file) and
							sim::is_source(file))
						{
							if (found) {
								errlog("\033[1;31mError\033[m: Sipfile:"
									" specified generator '",
									specified_generator,
									"' matches more than one file: '",
									generator, "' and '", file, "'");
								exit(1);
							} else {
								generator = file;
								found = true;
							}
						}
					});

				if (not found) {
					stdlog("\033[1;35mwarning\033[m: no file matches specified"
						" generator: '", specified_generator, "' - treating the"
						" specified generator as a shell command; to remove"
						" this warning prefix generator with sh: - e.g."
						" sh:echo");
					generator = concat<32>("sh:", specified_generator);
				}
			}

			entry.extractLeading(isspace);
			entry.extractTrailing(isspace);
			StringView generator_args = entry;

			parse_test_range(test_range, [&](StringView test) {
				bool inserted = gen_tests.emplace(test, generator,
					generator_args);
				if (not inserted) {
					errlog("\033[1;31mError\033[m: Sipfile: generated test: ",
						test, " is specified in more than one test range");
					exit(1);
				}

				if (static_tests.find(test)) {
					errlog("\033[1;31mError\033[m: Sipfile: generated test: ",
						test, " is also specified as static test");
					exit(1);
				}
			});
		}
	}

	pc.remove_with_prefix("utils/");

	struct Test {
		Optional<StringView> in, out;
		Test(StringView test_path) {
			if (hasSuffix(test_path, ".in"))
				in = test_path;
			else
				out = test_path;
		}
	};

	auto get_test_name = [](StringView test_path) {
		test_path = test_path.substring(0, test_path.rfind('.'));
		return test_path.extractTrailing(isnt_slash);
	};

	// Path of files that seem to be test inputs / outputs
	AVLDictMap<StringView, Test> test_files; // test name => test files
	pc.for_each_with_prefix("", [&](StringView file) {
		if (hasSuffix(file, ".in")) {
			auto test_name = get_test_name(file);
			auto it = test_files.find(test_name);
			if (it == nullptr)
				test_files.emplace(test_name, file);
			else if (it->second.in.has_value())
				stdlog("\033[1;35mwarning\033[m: input file of test ",
					it->first, "was found in more than one location: ",
					it->second.in.value(), " and ", file);
			else
				it->second.in = file;

		} else if (hasSuffix(file, ".out")) {
			auto test_name = get_test_name(file);
			auto it = test_files.find(test_name);
			if (it == nullptr)
				test_files.emplace(test_name, file);
			else if (it->second.out.has_value())
				stdlog("\033[1;35mwarning\033[m: output file of test ",
					it->first, "was found in more than one location: ",
					it->second.out.value(), " and ", file);
			else
				it->second.out = file;
		}
	});

	// Check for tests that are not specified as generated or static
	if (ensure_all_tests_are_in_sipfile or delete_invalid_test_files) {
		bool encountered_error = false;
		test_files.for_each([&](auto&& p) {
			if (not static_tests.find(p.first) and not gen_tests.find(p.first)) {
				if (p.second.in.has_value() and p.second.out.has_value()) {
					if (delete_invalid_test_files) {
						stdlog("\033[1;35mwarning\033[m: Test '", p.first,
							"' is neither specified as static nor as generated - deleting");
						remove(p.second.in.value().to_string());
						remove(p.second.out.value().to_string());
					} else {
						errlog("\033[1;31mError\033[m: Test '", p.first,
							"' is neither specified as static nor as generated");
						encountered_error = true;
					}
				} else {
					if (delete_invalid_test_files) {
						stdlog("\033[1;35mwarning\033[m: orphaned test file: ",
							p.second.in.value_or(""), p.second.out.value_or(""), " - deleting");
						if (p.second.in.has_value())
							remove(p.second.in.value().to_string());
						else
							remove(p.second.out.value().to_string());

					} else {
						stdlog("\033[1;35mwarning\033[m: orphaned test file: ",
							p.second.in.value_or(""), p.second.out.value_or(""));
					}
				}
			}
		});

		if (encountered_error)
			exit(1);
	}

	StringView in_dir, out_dir;
	if (isDirectory("tests/")) {
		in_dir = out_dir = "tests/";
	} else {
		mkdir("in/");
		in_dir = "in/";
		mkdir("out/");
		out_dir = "out/";
	}

	auto test_files_locations = [&](StringView test_name) {
		auto tf = test_files.find(test_name);
		InplaceBuff<32> input = concat<32>(in_dir, test_name, ".in");
		InplaceBuff<32> output = concat<32>(out_dir, test_name, ".out");
		if (tf != nullptr) {
			if (tf->second.in.has_value()) {
				input = tf->second.in.value();
				if (tf->second.out.has_value())
					output = tf->second.out.value();
				else
					output = concat<32>(substring(input, 0, input.size - 2),
						"out");
			} else {
				throw_assert(tf->second.out.has_value());
				output = tf->second.out.value();
				input = concat<32>(substring(output, 0, output.size - 2), "in");
			}
		}

		return std::make_pair(input, output);
	};

	// Touch the test files so that sim::Conver sees them
	static_tests.for_each([&](StringView test_name) {
		auto test = test_files_locations(test_name);
		CStringView output = test.second.to_cstr();
		putFileContents(output, "");
	});

	gen_tests.for_each([&](auto&& p) {
		auto test = test_files_locations(p.name);
		CStringView input = test.first.to_cstr(), output = test.second.to_cstr();
		putFileContents(output, "");
		if (generate_inputs)
			putFileContents(input, "");
	});

	Simfile simfile = construct_full_simfile();
	if (simfile.solutions.empty()) {
		errlog("\033[1;31mError\033[m: no main solution was found");
		return 1;
	}

	sim::JudgeWorker jworker;
	jworker.loadPackage(".", simfile.dump());

	CompilationCache::load_solution(jworker, simfile.solutions.front());

	bool generated_successfully = true;
	for (auto const& group : simfile.tgroups) {
		stdlog("--------------------------------");
		for (auto const& test : group.tests) {
			if (generate_inputs) {
				auto gen_test = gen_tests.find(test.name);
				if (gen_test == nullptr) { // Static test
					throw_assert(static_tests.find(test.name));
				} else { // Generated test
					// Generate input
					FileDescriptor gen_stderr(openUnlinkedTmpFile());
					if (gen_stderr == -1)
						THROW("openUnlinkedTmpFile()", errmsg());

					InplaceBuff<32> generator(hasPrefix(gen_test->generator,
						"sh:") ? substring(gen_test->generator, 3)
						: CompilationCache::compile(gen_test->generator));

					auto tmplog = stdlog("generating ", test.name, ".in");
					tmplog.flush_no_nl();

					auto es = Spawner::run("sh", {
						"sh",
						"-c",
						concat_tostr(generator, ' ', gen_test->generator_args)
					}, Spawner::Options(-1,
						FileDescriptor(test.in, O_WRONLY | O_CREAT | O_TRUNC),
						gen_stderr));

					uint64_t runtime = timespec_to_usec(es.cpu_runtime);
					if (es.si.code == CLD_EXITED and es.si.status == 0) {
						tmplog(" \033[1;32mdone\033[m in ",
							usecToSecStr(runtime, 2, false), " s");

					} else {
						tmplog(" \033[1;31mruntime error\033[m - ",
							usecToSecStr(runtime, 2, false), " s (", es.message,
							')');
						generated_successfully = false;
					}

					tmplog.flush();

					if (lseek(gen_stderr, 0, SEEK_SET) == -1)
						THROW("lseek()", errmsg());
					if (blast(gen_stderr, STDERR_FILENO) == -1)
						THROW("blast()", errmsg());
				}
			}

			// Gen output
			generated_successfully &= gen_test_out(jworker, test.in, test.out,
				test.time_limit, test.memory_limit,
				concat("generating ", test.name, ".out"));
		}
	}

	return generated_successfully;
}

namespace command {

int doc(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("\033[1;31mError\033[m: Simfile is missing. If you are sure that"
			" you are in the correct directory, running: `sip init . <name>`"
			" may help.");
		return 1;
	}

	mkdir_r("utils/latex");

	sim::PackageContents pc;
	pc.load_from_directory("doc/", true);

	std::vector<std::string> tex_files;

	pc.for_each_with_prefix("", [&](StringView file) {
		if (hasSuffix(file, ".tex"))
			tex_files.emplace_back(file.to_string());
	});

	auto compile_tex_file = [&](StringView file) {
		stdlog("\033[1mCompiling ", file, "\033[m");
		// It is necessary (essential) to run latex two times
		for (int i = 0; i < 1; ++i) {
			auto es = Spawner::run("pdflatex", {
				"pdflatex",
				"-output-dir=utils/latex",
				file.to_string()
			}, {-1, STDOUT_FILENO, STDERR_FILENO});
			if (es.si.code != CLD_EXITED or es.si.status != 0) {
				errlog("\033[1;31mCompilation failed.\033[m");
				// exit(1); // During watching it is not intended to stop on compilation error
			}
		}

		move(concat("utils/latex/", filename(file).withoutSuffix(3), "pdf").to_cstr(),
			concat(file.withoutSuffix(3), "pdf").to_cstr());
	};

	for (StringView file : tex_files)
		compile_tex_file(file);

	while (args.size())
		if (args.extract_next() == "watch") {
			FileDescriptor ino_fd(inotify_init());
			if (ino_fd == -1)
				THROW("inotify_init()", errmsg());

			AVLDictSet<CStringView> unwatched_files;
			for (CStringView file : tex_files)
				unwatched_files.emplace(file);

			AVLDictMap<FileDescriptor, CStringView> watched_files; // fd => file
			auto process_unwatched_files = [&] {
				for (auto it = unwatched_files.front(); it; it = unwatched_files.upper_bound(*it)) {
					CStringView file = *it;
					FileDescriptor fd(inotify_add_watch(ino_fd, file.data(),
						IN_MODIFY | IN_MOVE_SELF));
					if (fd == -1) {
						stdlog("\033[1;35mwarning\033[m: could not watch file ",
							file, ": inotify_add_watch()", errmsg());
						continue;
					}

					stdlog("\033[1mStarted watching ", file, "\033[m");

					// File is now watched
					watched_files.emplace(std::move(fd), file);
					unwatched_files.erase(file);
				}
			};

			// Inotify buffer
			// WARNING: this assumes that no directory is watched
			char inotify_buff[sizeof(inotify_event) * tex_files.size()];
			for (;;) {
				process_unwatched_files();
				// Wait for notification
				pollfd pfd = {ino_fd, POLLIN, 0};
				int rc = poll(&pfd, 1, -1);
				if (rc < 0)
					THROW("poll()", errmsg());

				ssize_t len = read(ino_fd, inotify_buff, sizeof(inotify_buff));
				if (len < 1) {
					stdlog("\033[1;35mwarning\033[m: read()", errmsg());
					continue;
				}

				struct inotify_event *event;
				// Process files for which an event occurred
				for (char *ptr = inotify_buff; ptr < inotify_buff + len;
					ptr += sizeof(inotify_event)) // WARNING: if you want to watch
					                              // directories, add + events->len
				{
					event = (struct inotify_event *) ptr;
					CStringView file = watched_files.find(event->wd)->second;
					auto unwatch_file = [&] {
						unwatched_files.emplace(file);
						watched_files.erase(event->wd);
					};

					// If file was moved, stop watching it
					if (event->mask & IN_MOVE_SELF) {
						if (inotify_rm_watch(ino_fd, event->wd))
							THROW("inotify_rm_watch()", errmsg());

						unwatch_file();

					// If file has disappeared, stop watching it
 					} else if (event->mask & IN_IGNORED) {
 						unwatch_file();
 					// Other (normal) event occurred
 					} else {
 						compile_tex_file(file);
 					}
				}
			}
		}

	return 0;
}

int genout(ArgvParser) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("\033[1;31mError\033[m: Simfile is missing. If you are sure that"
			" you are in the correct directory, running: `sip init . <name>`"
			" may help.");
		return 1;
	}

	if (not gen_impl(false, false, false)) {
		errlog("\033[1;31mError\033[m: Some tests' outputs were not generated"
			" successfully");
		return 1;
	}

	return 0;
}

int gentests(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("\033[1;31mError\033[m: Simfile is missing. If you are sure that"
			" you are in the correct directory, running: `sip init . <name>`"
			" may help.");
		return 1;
	}

	bool force = false;
	while (args.size()) {
		StringView arg = args.extract_next();
		if (arg == "force")
			force = true;
	}

	if (not gen_impl(true, true, force)) {
		errlog("\033[1;31mError\033[m: Some tests were not generated"
			" successfully");
		return 1;
	}

	return 0;
}

int prog(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("\033[1;31mError\033[m: Simfile is missing. If you are sure that"
			" you are in the correct directory, running: `sip init . <name>`"
			" may help.");
		return 1;
	}

	Simfile simfile = construct_full_simfile();
	if (simfile.solutions.empty()) {
		errlog("\033[1;31mError\033[m: no solution was found");
		return 1;
	}

	sim::JudgeWorker jworker;
	jworker.loadPackage(".", simfile.dump());

	AVLDictSet<StringView> solutions_to_compile;
	if (args.size() == 0) {
		for (auto const& sol : simfile.solutions)
			solutions_to_compile.emplace(sol);
	} else {
		solutions_to_compile = parse_args_to_solutions(simfile, args);
	}

	solutions_to_compile.for_each([&](StringView solution) {
		stdlog("\033[1;34m", solution, "\033[m:");
		if (CompilationCache::is_cached(solution))
			stdlog("Solution is already compiled.");

		CompilationCache::load_solution(jworker, solution);
	});

	return 0;
}

int test(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("\033[1;31mError\033[m: Simfile is missing. If you are sure that"
			" you are in the correct directory, running: `sip init . <name>`"
			" may help.");
		return 1;
	}

	Simfile simfile = construct_full_simfile();
	if (simfile.solutions.empty()) {
		errlog("\033[1;31mError\033[m: no solution was found");
		return 1;
	}

	sim::JudgeWorker jworker;
	jworker.loadPackage(".", simfile.dump());

	AVLDictSet<StringView> solutions_to_test;
	if (args.size() == 0)
		solutions_to_test.emplace(simfile.solutions.front());
	else
		solutions_to_test = parse_args_to_solutions(simfile, args);

	auto judge_solution = [&](StringView solution) {
		stdlog("\033[1;34m", solution, "\033[m: {");
		CompilationCache::load_solution(jworker, solution);
		auto jrep1 = jworker.judge(false, SipJudgeLogger());
		auto jrep2 = jworker.judge(true, SipJudgeLogger());
		stdlog('}');

		if (solution == simfile.solutions.front()) {
			auto tmplog = stdlog("Adjusting time limits...");
			tmplog.flush_no_nl();
			update_time_limits(simfile, jrep1, jrep2);
			jworker.loadPackage(".", simfile.dump());
			tmplog(" done");
		}
	};

	CompilationCache::load_checker(jworker, simfile.checker);

	// Run model solution first to adjust time limits
	if (solutions_to_test.find(simfile.solutions.front())) {
		auto def_tl = default_time_limit();
		for (auto& group : simfile.tgroups)
			for (auto& test : group.tests)
				test.time_limit = def_tl;

		jworker.loadPackage(".", simfile.dump());
		judge_solution(simfile.solutions.front());
		solutions_to_test.erase(simfile.solutions.front());
	}

	solutions_to_test.for_each(judge_solution);

	return 0;
}

} // namespace command
