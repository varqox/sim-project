#include "../include/debug.hh"
#include "../include/defer.hh"
#include "../include/file_contents.hh"
#include "../include/unlinked_temporary_file.hh"

#include <gtest/gtest.h>

using std::string;

TEST(debug, throw_assert) {
	constexpr int var = 123;
	try {
		throw_assert(var == 123);
		throw_assert(var % 3 != 0);
		ADD_FAILURE();
	} catch (const std::runtime_error& e) {
		constexpr auto line = __LINE__;
		EXPECT_EQ(e.what(),
		          concat(__FILE__, ':', line - 3, ": ", __PRETTY_FUNCTION__,
		                 ": Assertion `var % 3 != 0` failed."));
	} catch (...) {
		ADD_FAILURE();
	}
}

template <class LoggingFunc>
static std::string intercept_logger(Logger& logger,
                                    LoggingFunc&& logging_func) {
	FileDescriptor fd {open_unlinked_tmp_file()};
	throw_assert(fd.is_open());
	std::unique_ptr<FILE, decltype(&fclose)> stream = {fdopen(fd, "r+"),
	                                                   fclose};
	throw_assert(stream);
	(void)fd.release(); // Now stream owns it

	FILE* logger_stream = stream.get();
	stream.reset(logger.exchange_log_stream(stream.release()));
	bool old_label = logger.label(false);
	Defer guard = [&] {
		logger.use(stream.release());
		logger.label(old_label);
	};

	logging_func();

	// Get logged data
	rewind(logger_stream);
	return get_file_contents(fileno(logger_stream));
}

TEST(debug, LOG_LINE_MARCO) {
	auto logged_line = intercept_logger(stdlog, [] { LOG_LINE; });
	EXPECT_EQ(concat_tostr(__FILE__, ':', __LINE__ - 1, '\n'), logged_line);
	auto logged_line2 = intercept_logger(stdlog, [] { LOG_LINE; });
	EXPECT_EQ(concat_tostr(__FILE__, ':', __LINE__ - 1, '\n'), logged_line2);
}

TEST(debug, THROW_MACRO) {
	try {
		THROW("a ", 1, -42, "c", '.', false, ";");
		ADD_FAILURE();
	} catch (const std::runtime_error& e) {
		constexpr auto line = __LINE__;
		EXPECT_EQ(e.what(),
		          concat("a ", 1, -42, "c", '.', false, "; (thrown at ",
		                 __FILE__, ':', line - 3, ')'));
	} catch (...) {
		ADD_FAILURE();
	}
}

TEST(debug, errmsg) {
	EXPECT_EQ(concat(" - ", EPERM, ": Operation not permitted"), errmsg(EPERM));
	EXPECT_EQ(concat(" - ", ENOENT, ": No such file or directory"),
	          errmsg(ENOENT));

	errno = EPERM;
	EXPECT_EQ(concat(" - ", EPERM, ": Operation not permitted"), errmsg());
	errno = ENOENT;
	EXPECT_EQ(concat(" - ", ENOENT, ": No such file or directory"), errmsg());
}

namespace {
constexpr auto foo_mark_line = __LINE__ + 5;
constexpr auto foo_thrown_msg = "abc";
} // namespace

static void foo(bool throw_exception) {
	STACK_UNWINDING_MARK;
	if (throw_exception)
		throw std::runtime_error(foo_thrown_msg);
}

static void leave_stack_unwinding_mark() {
	try {
		foo(true);
		ADD_FAILURE();
	} catch (...) {
		EXPECT_GT(::stack_unwinding::StackGuard::marks_collected.size(), 0);
	}
}

static void test_errlog_catch_and_stack_unwinding_mark_normal() {
	// Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
	leave_stack_unwinding_mark();

	// Check discarding earlier unwinding marks and retaining of the recent ones
	constexpr auto mark_line = __LINE__ + 2;
	try {
		STACK_UNWINDING_MARK;
		foo(true);
		ADD_FAILURE();
	} catch (const std::exception& e) {
		EXPECT_EQ(string(foo_thrown_msg), e.what());
		constexpr auto logging_line = __LINE__ + 1;
		auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(e); });
		EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
		                       ": Caught exception -> abc"
		                       "\nStack unwinding marks:"
		                       "\n[0] void foo(bool) at ",
		                       __FILE__, ':', foo_mark_line, "\n[1] ",
		                       __PRETTY_FUNCTION__, " at ", __FILE__, ':',
		                       mark_line, "\n"),
		          logged_catch);
	} catch (...) {
		ADD_FAILURE();
	}
}

static void test_errlog_catch_and_stack_unwinding_mark_normal_no_args() {
	// Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
	leave_stack_unwinding_mark();

	// Check discarding earlier unwinding marks and retaining of the recent ones
	constexpr auto mark_line = __LINE__ + 2;
	try {
		STACK_UNWINDING_MARK;
		foo(true);
		ADD_FAILURE();
	} catch (const std::exception& e) {
		EXPECT_EQ(string(foo_thrown_msg), e.what());
		constexpr auto logging_line = __LINE__ + 1;
		auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(); });
		EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
		                       ": Caught exception"
		                       "\nStack unwinding marks:"
		                       "\n[0] void foo(bool) at ",
		                       __FILE__, ':', foo_mark_line, "\n[1] ",
		                       __PRETTY_FUNCTION__, " at ", __FILE__, ':',
		                       mark_line, "\n"),
		          logged_catch);
	} catch (...) {
		ADD_FAILURE();
	}
}

static void test_errlog_catch_and_stack_unwinding_mark_ignoring_older_marks() {
	// Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
	leave_stack_unwinding_mark();

	if constexpr (/* DISABLES CODE */ (false)) {
		// Unfortunately, this test cannot be met as there is no way to get
		// information about current exception during stack unwinding
		try {
			throw std::exception();
		} catch (...) {
			constexpr auto logging_line = __LINE__ + 1;
			auto logged_catch =
			   intercept_logger(errlog, [&] { ERRLOG_CATCH(); });
			EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
			                       ": Caught exception\n"
			                       "Stack unwinding marks:\n"),
			          logged_catch);
		}
	}
}

static void test_errlog_catch_and_stack_unwinding_mark_ignoring_second_level() {
	// Second level stack unwinding marks should not be saved
	struct SecondLevel {
		~SecondLevel() {
			try {
				foo(true);
				ADD_FAILURE();
			} catch (...) {
			}
		}
	};

	// Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
	leave_stack_unwinding_mark();

	constexpr auto mark_line = __LINE__ + 2;
	try {
		STACK_UNWINDING_MARK;
		SecondLevel sec_lvl;
		foo(true);
		ADD_FAILURE();
	} catch (const std::exception& e) {
		EXPECT_EQ(string(foo_thrown_msg), e.what());
		constexpr auto logging_line = __LINE__ + 1;
		auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(e); });
		EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
		                       ": Caught exception -> abc"
		                       "\nStack unwinding marks:"
		                       "\n[0] void foo(bool) at ",
		                       __FILE__, ':', foo_mark_line, "\n[1] ",
		                       __PRETTY_FUNCTION__, " at ", __FILE__, ':',
		                       mark_line, "\n"),
		          logged_catch);
	}
}

TEST(debug, ERRLOG_CATCH_AND_STACK_UNWINDING_MARK_MACROS) {
	STACK_UNWINDING_MARK;
	{ STACK_UNWINDING_MARK; }

	EXPECT_NO_THROW(foo(false));

	test_errlog_catch_and_stack_unwinding_mark_ignoring_older_marks();
	test_errlog_catch_and_stack_unwinding_mark_normal();
	test_errlog_catch_and_stack_unwinding_mark_normal_no_args();
	test_errlog_catch_and_stack_unwinding_mark_ignoring_second_level();
}

TEST(debug_DeathTest, WONT_THROW_MACRO_fail) {
	ASSERT_DEATH(
	   {
		   std::vector<int> abc;
		   errlog("BUG");
		   WONT_THROW(abc.at(42));
	   },
	   "BUG");
}

TEST(debug, WONT_THROW_MACRO_lvalue_reference) {
	{
		int a = 42;
		WONT_THROW(++a) = 42;
		EXPECT_EQ(a, 42);
	}
	{
		int a = 42;
		int& x = WONT_THROW(++a);
		++a;
		EXPECT_EQ(x, 44);
	}
}

TEST(debug, WONT_THROW_MACRO_rvalue_reference) {
	auto ptr = std::make_unique<int>(162);
	std::unique_ptr<int> ptr2 = WONT_THROW(std::move(ptr));
	EXPECT_EQ((bool)ptr, false);
	EXPECT_EQ((bool)ptr2, true);
}

TEST(debug, WONT_THROW_MACRO_prvalue) {
	std::unique_ptr<int> ptr = WONT_THROW(std::make_unique<int>(162));
	EXPECT_EQ((bool)ptr, true);
}

TEST(debug, WONT_THROW_MACRO_rvalue) {
	int a = 4, b = 7;
	int c = WONT_THROW(a + b);
	EXPECT_EQ(a, 4);
	EXPECT_EQ(b, 7);
	EXPECT_EQ(c, 11);
}
