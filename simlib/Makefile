include $(PREFIX)makefile-utils/Makefile.config

.PHONY: $(PREFIX)all
$(PREFIX)all: $(PREFIX)gtest_main.a $(PREFIX)simlib.a
	@printf "\033[32mBuild finished\033[0m\n"

define GOOGLETEST_FLAGS =
INTERNAL_EXTRA_CXX_FLAGS = -isystem '$(CURDIR)/$(PREFIX)googletest/googletest/include' -I '$(CURDIR)/$(PREFIX)googletest/googletest' -pthread
endef

$(PREFIX)simlib.a $(PREFIX)test/exec: override CXXSTD_FLAG = -std=c++17

$(eval $(call add_static_library, $(PREFIX)gtest_main.a, $(GOOGLETEST_FLAGS), \
	$(PREFIX)googletest/googletest/src/gtest-all.cc \
	$(PREFIX)googletest/googletest/src/gtest_main.cc \
))

$(eval $(call add_static_library, $(PREFIX)simlib.a,, \
	$(PREFIX)src/aho_corasick.cc \
	$(PREFIX)src/config_file.cc \
	$(PREFIX)src/file_contents.cc \
	$(PREFIX)src/file_manip.cc \
	$(PREFIX)src/http/response.cc \
	$(PREFIX)src/http/server.cc \
	$(PREFIX)src/humanize.cc \
	$(PREFIX)src/libarchive_zip.cc \
	$(PREFIX)src/logger.cc \
	$(PREFIX)src/path.cc \
	$(PREFIX)src/proc_stat_file_contents.cc \
	$(PREFIX)src/process.cc \
	$(PREFIX)src/random.cc \
	$(PREFIX)src/sandbox.cc \
	$(PREFIX)src/sha.cc \
	$(PREFIX)src/sha3.c \
	$(PREFIX)src/sim/checker.cc \
	$(PREFIX)src/sim/compile.cc \
	$(PREFIX)src/sim/conver.cc \
	$(PREFIX)src/sim/default_checker_dump.c \
	$(PREFIX)src/sim/judge_worker.cc \
	$(PREFIX)src/sim/problem_package.cc \
	$(PREFIX)src/sim/simfile.cc \
	$(PREFIX)src/spawner.cc \
	$(PREFIX)src/string_compare.cc \
	$(PREFIX)src/temporary_directory.cc \
	$(PREFIX)src/time.cc \
	$(PREFIX)src/unlinked_temporary_file.cc \
	$(PREFIX)src/working_directory.cc \
))

$(eval $(call add_generated_target, $(PREFIX)src/sim/default_checker_dump.c, \
	xxd -i $$< | sed 's@\w*default_checker_c@default_checker_c@g' > $$@, \
	$(PREFIX)src/sim/default_checker.c $(PREFIX)Makefile \
))

define SIMLIB_TEST_FLAGS =
INTERNAL_EXTRA_CXX_FLAGS = -isystem '$(CURDIR)/$(PREFIX)googletest/googletest/include' -isystem '$(CURDIR)/$(PREFIX)googletest/googlemock/include'
INTERNAL_EXTRA_LD_FLAGS = -lrt -pthread -lseccomp -lzip
endef

$(eval $(call add_executable, $(PREFIX)test/exec, $(SIMLIB_TEST_FLAGS), \
	$(PREFIX)gtest_main.a \
	$(PREFIX)simlib.a \
	$(PREFIX)test/argv_parser.cc \
	$(PREFIX)test/avl_dict.cc \
	$(PREFIX)test/call_in_destructor.cc \
	$(PREFIX)test/concat.cc \
	$(PREFIX)test/concat_common.cc \
	$(PREFIX)test/concat_tostr.cc \
	$(PREFIX)test/config_file.cc \
	$(PREFIX)test/conver.cc \
	$(PREFIX)test/debug.cc \
	$(PREFIX)test/defer.cc \
	$(PREFIX)test/defer.cc \
	$(PREFIX)test/directory.cc \
	$(PREFIX)test/enum_val.cc \
	$(PREFIX)test/file_contents.cc \
	$(PREFIX)test/file_descriptor.cc \
	$(PREFIX)test/file_info.cc \
	$(PREFIX)test/file_manip.cc \
	$(PREFIX)test/file_path.cc \
	$(PREFIX)test/http/response.cc \
	$(PREFIX)test/humanize.cc \
	$(PREFIX)test/inplace_array.cc \
	$(PREFIX)test/inplace_buff.cc \
	$(PREFIX)test/libzip.cc \
	$(PREFIX)test/logger.cc \
	$(PREFIX)test/memory.cc \
	$(PREFIX)test/mysql.cc \
	$(PREFIX)test/opened_temporary_file.cc \
	$(PREFIX)test/path.cc \
	$(PREFIX)test/proc_stat_file_contents.cc \
	$(PREFIX)test/process.cc \
	$(PREFIX)test/random.cc \
	$(PREFIX)test/request_uri_parser.cc \
	$(PREFIX)test/sandbox.cc \
	$(PREFIX)test/sha.cc \
	$(PREFIX)test/shared_function.cc \
	$(PREFIX)test/signal_blocking.cc \
	$(PREFIX)test/sim/problem_package.cc \
	$(PREFIX)test/simfile.cc \
	$(PREFIX)test/simple_parser.cc \
	$(PREFIX)test/spawner.cc \
	$(PREFIX)test/string_compare.cc \
	$(PREFIX)test/string_traits.cc \
	$(PREFIX)test/string_transform.cc \
	$(PREFIX)test/string_view.cc \
	$(PREFIX)test/temporary_directory.cc \
	$(PREFIX)test/temporary_file.cc \
	$(PREFIX)test/time.cc \
	$(PREFIX)test/to_string.cc \
	$(PREFIX)test/unlinked_temporary_file.cc \
	$(PREFIX)test/utilities.cc \
	$(PREFIX)test/working_directory.cc \
))

.PHONY: $(PREFIX)test
$(PREFIX)test: $(PREFIX)test/exec
	$(PREFIX)test/exec

.PHONY: $(PREFIX)format
$(PREFIX)format: $(shell find $(PREFIX)include $(PREFIX)src $(PREFIX)test $(PREFIX)doc | grep -E '\.(cc?|hh?)$$' | grep -vE '^($(PREFIX)src/sha3.c|$(PREFIX)src/sim/default_checker_dump.c)$$' | sed 's/$$/-make-format/')
