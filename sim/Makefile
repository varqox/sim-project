include subprojects/simlib/makefile-utils/Makefile.config

DESTDIR := sim

MYSQL_CONFIG := $(shell which mariadb_config 2> /dev/null || which mysql_config 2> /dev/null)

define SIM_FLAGS =
INTERNAL_EXTRA_CXX_FLAGS = -I '$(CURDIR)/include' -I '$(CURDIR)/subprojects/simlib/include' $(shell $(MYSQL_CONFIG) --include)
INTERNAL_EXTRA_LD_FLAGS = -L '$(CURDIR)/src' $(shell $(MYSQL_CONFIG) --libs) -lsupc++ -pthread -lrt -lzip -lseccomp
endef

.PHONY: all
all: src/setup-installation src/backup src/sim-merger src/job-server src/sim-server src/sim-upgrader src/manage
	@printf "\033[32mBuild finished\033[0m\n"

$(eval $(call include_makefile, subprojects/simlib/Makefile))

.PHONY: test
test: test-sim test-simlib
	@printf "\033[1;32mAll tests passed\033[0m\n"

.PHONY: test-simlib
test-simlib: subprojects/simlib/test

.PHONY: test-sim
test-sim: test/exec
	test/exec

.PHONY: install
install: $(filter-out install run, $(MAKECMDGOALS))
	@ #   ^ install always have to be executed at the end (but before run)
	# Echo log
	@printf "DESTDIR = \033[01;34m$(abspath $(DESTDIR))\033[0m\n"

	# Installation
	$(MKDIR) $(abspath $(DESTDIR))
	chmod 0700 $(abspath $(DESTDIR))
	$(MKDIR) $(abspath $(DESTDIR)/static/)
	$(UPDATE) src/static $(abspath $(DESTDIR))
	$(MKDIR) $(abspath $(DESTDIR)/internal_files/)
	$(MKDIR) $(abspath $(DESTDIR)/logs/)
	$(MKDIR) $(abspath $(DESTDIR)/bin/)
	$(UPDATE) src/sim-server src/job-server src/backup src/sim-merger $(abspath $(DESTDIR)/bin/)
	$(UPDATE) src/manage $(abspath $(DESTDIR))
	# Do not override the config if it already exists
	$(UPDATE) -n src/sim.conf $(abspath $(DESTDIR))
	# Install PRoot
ifeq ($(shell uname -m), x86_64)
	$(UPDATE) bin/proot-x86_64 $(abspath $(DESTDIR)/proot)
else
	$(UPDATE) bin/proot-x86 $(abspath $(DESTDIR)/proot)
endif
	# Set up install
	src/setup-installation $(abspath $(DESTDIR)) $(SETUP_INSTALL_FLAGS)

	@printf "\033[;32mInstallation finished\033[0m\n"

.PHONY: kill
kill: $(filter-out kill run reinstall uninstall, $(MAKECMDGOALS))
	$(abspath $(DESTDIR)/manage) kill

.PHONY: reinstall
reinstall: override SETUP_INSTALL_FLAGS += --drop-tables
reinstall: $(filter-out reinstall run, $(MAKECMDGOALS)) kill
	@ #     ^ reinstall always have to be executed at the end (but before run)
	# Delete files
	$(RM) -r $(abspath $(DESTDIR)/)
	$(MAKE) install

.PHONY: uninstall
uninstall: override SETUP_INSTALL_FLAGS += --only-drop-tables
uninstall: kill
	# Delete files and database tables
	src/setup-installation $(abspath $(DESTDIR)) $(SETUP_INSTALL_FLAGS)
	$(RM) -r $(abspath $(DESTDIR)/)

.PHONY: run
run: $(filter-out run, $(MAKECMDGOALS))
	$(abspath $(DESTDIR)/manage) -b start
	@printf "\033[;32mRunning finished\033[0m\n"

$(eval $(call add_static_library, src/sim.a, $(SIM_FLAGS), \
	src/sim/contest_files/permissions.cc \
	src/sim/contests/permissions.cc \
	src/sim/cpp_syntax_highlighter.cc \
	src/sim/jobs/jobs.cc \
	src/sim/mysql/mysql.cc \
	src/sim/problems/permissions.cc \
	src/sim/random.cc \
	src/sim/submissions/update_final.cc \
))

$(eval $(call add_executable, src/job-server, $(SIM_FLAGS), \
	src/job_server/dispatcher.cc \
	src/job_server/job_handlers/add_or_reupload_problem__judge_main_solution_base.cc \
	src/job_server/job_handlers/add_or_reupload_problem_base.cc \
	src/job_server/job_handlers/add_problem.cc \
	src/job_server/job_handlers/change_problem_statement.cc \
	src/job_server/job_handlers/delete_contest.cc \
	src/job_server/job_handlers/delete_contest_problem.cc \
	src/job_server/job_handlers/delete_contest_round.cc \
	src/job_server/job_handlers/delete_internal_file.cc \
	src/job_server/job_handlers/delete_problem.cc \
	src/job_server/job_handlers/delete_user.cc \
	src/job_server/job_handlers/job_handler.cc \
	src/job_server/job_handlers/judge_base.cc \
	src/job_server/job_handlers/judge_or_rejudge.cc \
	src/job_server/job_handlers/merge_problems.cc \
	src/job_server/job_handlers/merge_users.cc \
	src/job_server/job_handlers/reselect_final_submissions_in_contest_problem.cc \
	src/job_server/job_handlers/reset_problem_time_limits.cc \
	src/job_server/job_handlers/reset_time_limits_in_problem_package_base.cc \
	src/job_server/job_handlers/reupload_problem.cc \
	src/job_server/main.cc \
	src/sim.a \
	subprojects/simlib/simlib.a \
))

$(eval $(call add_executable, src/sim-merger, $(SIM_FLAGS), \
	src/sim.a \
	src/sim_merger/sim_merger.cc \
	subprojects/simlib/simlib.a \
))

$(eval $(call add_executable, src/sim-upgrader, $(SIM_FLAGS), \
	src/sim.a \
	src/sim_upgrader.cc \
	subprojects/simlib/simlib.a \
))

$(eval $(call add_executable, src/setup-installation, $(SIM_FLAGS), \
	src/setup_installation.cc \
	src/sim.a \
	subprojects/simlib/simlib.a \
))

$(eval $(call add_executable, src/backup, $(SIM_FLAGS), \
	src/backup.cc \
	src/sim.a \
	subprojects/simlib/simlib.a \
))

$(eval $(call add_executable, src/sim-server, $(SIM_FLAGS), \
	src/sim.a \
	src/web_server/http/cookies.cc \
	src/web_server/http/request.cc \
	src/web_server/http/response.cc \
	src/web_server/old/api.cc \
	src/web_server/old/contest_entry_token_api.cc \
	src/web_server/old/contest_files.cc \
	src/web_server/old/contest_files_api.cc \
	src/web_server/old/contest_users_api.cc \
	src/web_server/old/contests.cc \
	src/web_server/old/contests_api.cc \
	src/web_server/old/jobs.cc \
	src/web_server/old/jobs_api.cc \
	src/web_server/old/problems.cc \
	src/web_server/old/problems_api.cc \
	src/web_server/old/session.cc \
	src/web_server/old/sim.cc \
	src/web_server/old/submissions.cc \
	src/web_server/old/submissions_api.cc \
	src/web_server/old/template.cc \
	src/web_server/old/users.cc \
	src/web_server/old/users_api.cc \
	src/web_server/server/connection.cc \
	src/web_server/server/server.cc \
	src/web_server/web_worker/context.cc \
	src/web_server/web_worker/web_worker.cc \
	subprojects/simlib/simlib.a \
))

$(eval $(call add_executable, src/manage, $(SIM_FLAGS), \
	src/manage.cc \
	subprojects/simlib/simlib.a \
))

$(eval $(call add_executable, test/exec, $(SIM_FLAGS), \
	src/sim.a \
	subprojects/simlib/gtest_main.a \
	subprojects/simlib/simlib.a \
	test/sim/cpp_syntax_highlighter.cc \
	test/sim/jobs/jobs.cc \
))

.PHONY: format
format:
	python3 format.py .
