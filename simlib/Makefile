include $(PREFIX)Makefile.config

.PHONY: $(PREFIX)all
$(PREFIX)all: build-info
	@$(MAKE) $(PREFIX)gtest_main.a $(PREFIX)simlib.a
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
	$(PREFIX)src/syscall_name_32.cc \
	$(PREFIX)src/syscall_name_64.cc \
	$(PREFIX)src/time.cc \
	$(PREFIX)src/zip.cc

$(eval $(call load_dependencies, $(SIMLIB_SRCS)))
SIMLIB_OBJS := $(call SRCS_TO_OBJS, $(SIMLIB_SRCS))

$(PREFIX)simlib.a: $(SIMLIB_OBJS)
	$(MAKE_STATIC_LIB)

ifneq ($(wildcard /usr/include/x86_64-linux-gnu/asm/unistd_32.h),)
$(PREFIX)src/syscall_name_32.cc: UNISTD_32 = /usr/include/x86_64-linux-gnu/asm/unistd_32.h
else ifneq ($(wildcard /usr/include/x86-linux-gnu/asm/unistd_32.h),)
$(PREFIX)src/syscall_name_32.cc: UNISTD_32 = /usr/include/x86-linux-gnu/asm/unistd_32.h
else
$(PREFIX)src/syscall_name_32.cc: UNISTD_32 = /usr/include/asm/unistd_32.h
endif

$(PREFIX)src/syscall_name_32.cc: $(PREFIX)Makefile $(UNISTD_32)
	$(GEN)
	$(Q)echo "// WARNING: file generated automatically; changes will not be permanent." > $@
	$(Q)echo "#include \"../include/syscall_name.h\"" >> $@
	$(Q)echo "#include \"$(UNISTD_32)\"" >> $@
	$(Q)echo "" >> $@
	# 32 bit
	$(Q) [ -f $(UNISTD_32) ] || \
		{ echo "Error: $(UNISTD_32): No such file or directory"; $(RM) $@; exit 1; }
	$(Q)echo "SyscallNameSet x86_syscall_name {" >> $@
	$(Q)grep -E "^\s*#\s*define\s*__NR_" $(UNISTD_32) | \
		sed 's/#define\s*__NR_//' | \
		awk '{ printf "\t{"; if (2<=NF) printf $$2; for (i=3; i<=NF; ++i) printf " " $$i; print ", \"" $$1 "\"}," }' >> $@
	$(Q)echo "};" >> $@

ifneq ($(wildcard /usr/include/x86_64-linux-gnu/asm/unistd_64.h),)
$(PREFIX)src/syscall_name_64.cc: UNISTD_64 = /usr/include/x86_64-linux-gnu/asm/unistd_64.h
else ifneq ($(wildcard /usr/include/x86-linux-gnu/asm/unistd_64.h),)
$(PREFIX)src/syscall_name_64.cc: UNISTD_64 = /usr/include/x86-linux-gnu/asm/unistd_64.h
else
$(PREFIX)src/syscall_name_64.cc: UNISTD_64 = /usr/include/asm/unistd_64.h
endif

$(PREFIX)src/syscall_name_64.cc: $(PREFIX)Makefile $(UNISTD_64)
	$(GEN)
	$(Q)echo "// WARNING: file generated automatically; changes will not be permanent." > $@
	$(Q)echo "#include \"../include/syscall_name.h\"" >> $@
	$(Q)echo "#include \"$(UNISTD_64)\"" >> $@
	$(Q)echo "" >> $@
	# 64 bit
	$(Q)echo "SyscallNameSet x86_64_syscall_name {" >> $@
	$(Q) [ -f $(UNISTD_64) ] || \
		{ echo "Error: $(UNISTD_64): No such file or directory"; $(RM) $@; exit 1; }
	$(Q)grep -E "^\s*#\s*define\s*__NR_" $(UNISTD_64) | \
		sed 's/#define\s*__NR_//' | \
		awk '{ printf "\t{"; if (2<=NF) printf $$2; for (i=3; i<=NF; ++i) printf " " $$i; print ", \"" $$1 "\"}," }' >> $@
	$(Q)echo "};" >> $@

$(PREFIX)src/sim/default_checker_dump.c: $(PREFIX)src/sim/default_checker.c $(PREFIX)Makefile
	$(Q)$(call P,GEN,$@) xxd -i $< | sed 's@\w*default_checker_c@default_checker_c@g' > $@

SIMLIB_TEST_SRCS := \
	$(PREFIX)test/string.cc \
	$(PREFIX)test/filesystem.cc \
	$(PREFIX)test/config_file.cc \
	$(PREFIX)test/simfile.cc

$(eval $(call load_dependencies, $(SIMLIB_TEST_SRCS)))
SIMLIB_TEST_OBJS := $(call SRCS_TO_OBJS, $(SIMLIB_TEST_SRCS))

$(SIMLIB_TEST_OBJS): override EXTRA_CXX_FLAGS += -isystem '$(CURDIR)/$(PREFIX)googletest/googletest/include'

$(PREFIX)test/exec: $(SIMLIB_TEST_OBJS) $(PREFIX)simlib.a $(PREFIX)gtest_main.a
	$(LINK) -lrt -larchive -pthread

.PHONY: $(PREFIX)test
$(PREFIX)test: $(PREFIX)test/exec
	$(PREFIX)test/exec

.PHONY: $(PREFIX)clean
$(PREFIX)clean: OBJS := $(GOOGLETEST_OBJS) $(SIMLIB_OBJS) $(SIMLIB_TEST_OBJS)
$(PREFIX)clean:
	$(Q)$(RM) $(PREFIX)simlib.a $(PREFIX)gtest_main.a $(PREFIX)test/exec
	$(Q)$(RM) $(OBJS) $(OBJS:o=dwo)
	$(Q)find $(PREFIX)src $(PREFIX)test $(PREFIX)googletest -type f -name '*.deps' | xargs rm -f

.PHONY: $(PREFIX)help
$(PREFIX)help:
	@echo "Nothing is here yet..."
