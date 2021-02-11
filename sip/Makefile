include subprojects/simlib/makefile-utils/Makefile.config

DESTDIR := /usr/local/bin

define SIP_FLAGS =
INTERNAL_EXTRA_CXX_FLAGS := -I '$(CURDIR)/subprojects/simlib/include' -I '$(CURDIR)/'
INTERNAL_EXTRA_LD_FLAGS := -lsupc++ -lrt -lzip -lz -lseccomp -pthread
endef

.PHONY: all
all: sip
	@printf "\033[32mBuild finished\033[0m\n"

.PHONY: static
static: sip-static

$(eval $(call include_makefile, subprojects/simlib/Makefile))

.PHONY: test
test: test-sip test-simlib
	@printf "\033[1;32mAll tests passed\033[0m\n"

.PHONY: test-simlib
test-simlib: subprojects/simlib/test

.PHONY: test-sip
test-sip: sip
	test/run_sip_tests.py ./sip

.PHONY: install
install: $(filter-out install, $(MAKECMDGOALS))
	@ #   ^ install always have to be executed at the end (but before run)
	# Echo log
	@printf "DESTDIR = \033[01;34m$(abspath $(DESTDIR))\033[0m\n"

	# Installation
	$(UPDATE) sip $(abspath $(DESTDIR))

	# Set owner, group and permission bits
	chmod +x $(abspath $(DESTDIR)/sip)

	@printf "\033[;32mInstallation finished\033[0m\n"

.PHONY: reinstall
reinstall: install

.PHONY: uninstall
uninstall:
	# Delete installed files
	$(RM) $(abspath $(DESTDIR)/sip)

ifeq ($(shell uname -m), x86_64)
$(eval $(call add_generated_target, src/proot_dump.c,\
	xxd -i $$< | sed 's@\w*proot_x86_64@proot_dump@g' > $$@, \
	bin/proot-x86_64 \
	Makefile \
))
else
$(eval $(call add_generated_target, src/proot_dump.c,\
	xxd -i $$< | sed 's@\w*proot_x86@proot_dump@g' > $$@, \
	bin/proot-x86 Makefile
))
endif

$(eval $(call add_generated_target, templates/checker.cc.dump.c, \
	xxd -i $$< | sed 's@\w*checker_cc@checker_cc@g' > $$@, \
	templates/checker.cc Makefile \
))

$(eval $(call add_generated_target, templates/interactive_checker.cc.dump.c, \
	xxd -i $$< | sed 's@\w*interactive_checker_cc@interactive_checker_cc@g' > $$@, \
	templates/interactive_checker.cc Makefile \
))

$(eval $(call add_generated_target, templates/sim_statement.cls.dump.c, \
	xxd -i $$< | sed 's@\w*sim_statement_cls@sim_statement_cls@g' > $$@, \
	templates/sim-statement.cls Makefile \
))

$(eval $(call add_generated_target, templates/statement.tex.dump.c, \
	xxd -i $$< | sed 's@\w*statement_tex@statement_tex@g' > $$@, \
	templates/statement.tex Makefile \
))

$(eval $(call add_generated_target, templates/gen.cc.dump.c, \
	xxd -i $$< | sed 's@\w*gen_cc@gen_cc@g' > $$@, \
	templates/gen.cc Makefile \
))

$(eval $(call add_generated_target, src/git_commit.cc, \
	src/gen_git_commit_cc.py > $$@, \
	src/gen_git_commit_cc.py \
	.git/logs/HEAD \
	Makefile \
))

SIP_SRCS := \
	src/commands/checker.cc \
	src/commands/clean.cc \
	src/commands/devclean.cc \
	src/commands/devzip.cc \
	src/commands/doc.cc \
	src/commands/docwatch.cc \
	src/commands/gen.cc \
	src/commands/genin.cc \
	src/commands/genout.cc \
	src/commands/help.cc \
	src/commands/init.cc \
	src/commands/interactive.cc \
	src/commands/label.cc \
	src/commands/main_sol.cc \
	src/commands/mem.cc \
	src/commands/name.cc \
	src/commands/prog.cc \
	src/commands/regen.cc \
	src/commands/regenin.cc \
	src/commands/reseed.cc \
	src/commands/save.cc \
	src/commands/statement.cc \
	src/commands/template.cc \
	src/commands/test.cc \
	src/commands/unset.cc \
	src/commands/version.cc \
	src/commands/zip.cc \
	src/compilation_cache.cc \
	src/git_commit.cc \
	src/main.cc \
	src/proot_dump.c \
	src/sip_package.cc \
	src/sipfile.cc \
	src/templates/templates.cc \
	src/tests_files.cc \
	src/utils.cc \
	subprojects/simlib/simlib.a \
	templates/checker.cc.dump.c \
	templates/gen.cc.dump.c \
	templates/interactive_checker.cc.dump.c \
	templates/sim_statement.cls.dump.c \
	templates/statement.tex.dump.c \

$(eval $(call add_executable, sip, $(SIP_FLAGS), $(SIP_SRCS)))

define SIP_STATIC_FLAGS =
INTERNAL_EXTRA_LD_FLAGS += -static -lzip -lseccomp -lrt -lz -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
endef

$(eval $(call add_executable, sip-static, $(SIP_FLAGS) $(SIP_STATIC_FLAGS), $(SIP_SRCS)))

.PHONY: format
format:
	python3 format.py .
