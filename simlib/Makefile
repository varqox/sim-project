include $(PREFIX)Makefile.config

.PHONY: $(PREFIX)all
$(PREFIX)all: $(PREFIX)gtest_main.a $(PREFIX)simlib.a
	@printf "\033[32mBuild finished\033[0m\n"

GOOGLETEST_SRCS := \
	$(PREFIX)googletest/googletest/src/gtest-all.cc \
	$(PREFIX)googletest/googletest/src/gtest_main.cc

$(eval $(call load_dependencies, $(GOOGLETEST_SRCS)))
GOOGLETEST_OBJS := $(call SRCS_TO_OBJS, $(GOOGLETEST_SRCS))

$(PREFIX)gtest_main.a: $(GOOGLETEST_OBJS)
	$(MAKE_STATIC_LIB)

$(GOOGLETEST_OBJS): override EXTRA_CXX_FLAGS += -isystem '$(CURDIR)/$(PREFIX)googletest/googletest/include' -I '$(CURDIR)/$(PREFIX)googletest/googletest' -pthread

SIMLIB_SRCS := \
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
	$(PREFIX)src/time.cc

$(eval $(call load_dependencies, $(SIMLIB_SRCS)))
SIMLIB_OBJS := $(call SRCS_TO_OBJS, $(SIMLIB_SRCS))

$(PREFIX)simlib.a: $(SIMLIB_OBJS)
	$(MAKE_STATIC_LIB)

$(PREFIX)src/sim/default_checker_dump.c: $(PREFIX)src/sim/default_checker.c $(PREFIX)Makefile
	$(Q)$(call P,GEN,$@) xxd -i $< | sed 's@\w*default_checker_c@default_checker_c@g' > $@

SIMLIB_TEST_SRCS := \
	$(PREFIX)test/avl_dict.cc \
	$(PREFIX)test/config_file.cc \
	$(PREFIX)test/filesystem.cc \
	$(PREFIX)test/sandbox.cc \
	$(PREFIX)test/simfile.cc \
	$(PREFIX)test/conver.cc

$(eval $(call load_dependencies, $(SIMLIB_TEST_SRCS)))
SIMLIB_TEST_OBJS := $(call SRCS_TO_OBJS, $(SIMLIB_TEST_SRCS))

$(SIMLIB_TEST_OBJS): override EXTRA_CXX_FLAGS += -isystem '$(CURDIR)/$(PREFIX)googletest/googletest/include'

$(PREFIX)test/exec: $(SIMLIB_TEST_OBJS) $(PREFIX)simlib.a $(PREFIX)gtest_main.a
	$(LINK) -lrt -pthread -lseccomp -lzip

.PHONY: $(PREFIX)test
$(PREFIX)test: $(PREFIX)test/exec
	$(PREFIX)test/exec

.PHONY: $(PREFIX)format
$(PREFIX)format: $(shell find $(PREFIX)include $(PREFIX)src $(PREFIX)test $(PREFIX)doc | grep -E '\.(cc?|h)$$' | grep -vE '^($(PREFIX)src/sha3.c|$(PREFIX)src/sim/default_checker_dump.c)$$' | sed 's/$$/-make-format/')

%-make-format: %
	$(Q)$(call P,FORMAT,$*) \
		clang-format -i $*

.PHONY: $(PREFIX)clean
$(PREFIX)clean: OBJS := $(GOOGLETEST_OBJS) $(SIMLIB_OBJS) $(SIMLIB_TEST_OBJS)
$(PREFIX)clean:
	$(Q)$(RM) $(PREFIX)simlib.a $(PREFIX)gtest_main.a $(PREFIX)test/exec
	$(Q)$(RM) $(OBJS) $(OBJS:o=dwo)
	$(Q)find $(PREFIX)src $(PREFIX)test $(PREFIX)googletest -type f -name '*.deps' | xargs rm -f

.PHONY: $(PREFIX)help
$(PREFIX)help:
	@echo "Nothing is here yet..."
