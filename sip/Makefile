include Makefile.config

DESTDIR := /usr/local/bin

SIP_CXX_FLAGS := -I '$(CURDIR)/src/include'
SIP_LD_FLAGS := -L '$(CURDIR)/src/lib'

.PHONY: all
all: src/sip
	@printf "\033[32mBuild finished\033[0m\n"

$(eval $(call include_makefile, src/lib/simlib/Makefile))

.PHONY: test
test: test-sip test-simlib
	@printf "\033[1;32mAll tests passed\033[0m\n"

.PHONY: test-simlib
test-simlib: src/lib/simlib/test

.PHONY: test-sip
test-sip:

.PHONY: install
install: $(filter-out install, $(MAKECMDGOALS))
	@ #   ^ install always have to be executed at the end (but before run)
	# Echo log
	@printf "DESTDIR = \033[01;34m$(abspath $(DESTDIR))\033[0m\n"

	# Installation
	$(UPDATE) src/sip $(abspath $(DESTDIR))

	# Set owner, group and permission bits
	chmod +x $(abspath $(DESTDIR)/sip)

	@printf "\033[;32mInstallation finished\033[0m\n"

.PHONY: reinstall
reinstall: install

.PHONY: uninstall
uninstall:
	# Delete installed files
	$(RM) $(abspath $(DESTDIR)/sip) $(abspath $(DESTDIR)/proot)



ifeq ($(shell uname -m), x86_64)
src/proot_dump.c: bin/proot-x86_64 Makefile
	$(Q)$(call P,GEN,$@) xxd -i $< | sed 's@\w*proot_x86_64@proot_dump@g' > $@
else
src/proot_dump.c: bin/proot-x86 Makefile
	$(Q)$(call P,GEN,$@) xxd -i $< | sed 's@\w*proot_x86@proot_dump@g' > $@
endif

SIP_SRCS := \
	src/commands.cc \
	src/compilation_cache.cc \
	src/main.cc \
	src/proot_dump.c \
	src/sip_package.cc \
	src/sipfile.cc

$(eval $(call load_dependencies, $(SIP_SRCS)))
SIP_OBJS := $(call SRCS_TO_OBJS, $(SIP_SRCS))

src/sip: $(SIP_OBJS) src/lib/simlib/simlib.a
	$(LINK) -lsupc++ -lrt -larchive -lseccomp

# SIP_OBJS := $(SIP_OBJS)
SIP_EXECS := src/sip

$(SIP_OBJS): override EXTRA_CXX_FLAGS += $(SIP_CXX_FLAGS)
$(SIP_EXECS): private override EXTRA_LD_FLAGS += $(SIP_LD_FLAGS)

.PHONY: clean
clean: OBJS := $(SIP_OBJS)
clean: src/lib/simlib/clean
	$(Q)$(RM) $(SIP_EXECS) $(OBJS) $(OBJS:o=dwo)
	$(Q)find src -type f -name '*.deps' | xargs rm -f

.PHONY: help
help:
	@echo "Nothing is here yet..."
