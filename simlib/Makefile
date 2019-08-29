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
	$(PREFIX)src/debug.cc \
	$(PREFIX)src/filesystem.cc \
	$(PREFIX)src/http/response.cc \
	$(PREFIX)src/http/server.cc \
	$(PREFIX)src/libarchive_zip.cc \
	$(PREFIX)src/logger.cc \
	$(PREFIX)src/mysql.cc \
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
	$(PREFIX)src/string.cc \
	$(PREFIX)src/time.cc \
))

$(eval $(call add_generated_target, $(PREFIX)src/sim/default_checker_dump.c, \
	xxd -i $$< | sed 's@\w*default_checker_c@default_checker_c@g' > $$@, \
	$(PREFIX)src/sim/default_checker.c $(PREFIX)Makefile \
))

define SIMLIB_TEST_FLAGS =
INTERNAL_EXTRA_CXX_FLAGS = -isystem '$(CURDIR)/$(PREFIX)googletest/googletest/include'
INTERNAL_EXTRA_LD_FLAGS = -lrt -pthread -lseccomp -lzip
endef

$(eval $(call add_executable, $(PREFIX)test/exec, $(SIMLIB_TEST_FLAGS), \
	$(PREFIX)gtest_main.a \
	$(PREFIX)simlib.a \
	$(PREFIX)test/avl_dict.cc \
	$(PREFIX)test/config_file.cc \
	$(PREFIX)test/conver.cc \
	$(PREFIX)test/filesystem.cc \
	$(PREFIX)test/sandbox.cc \
	$(PREFIX)test/simfile.cc \
))

.PHONY: $(PREFIX)test
$(PREFIX)test: $(PREFIX)test/exec
	$(PREFIX)test/exec

.PHONY: $(PREFIX)format
$(PREFIX)format: $(shell find $(PREFIX)include $(PREFIX)src $(PREFIX)test $(PREFIX)doc | grep -E '\.(cc?|h)$$' | grep -vE '^($(PREFIX)src/sha3.c|$(PREFIX)src/sim/default_checker_dump.c)$$' | sed 's/$$/-make-format/')
