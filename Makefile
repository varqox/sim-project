include src/lib/simlib/makefile-utils/Makefile.config

DESTDIR := build

MYSQL_CONFIG := $(shell which mariadb_config 2> /dev/null || which mysql_config 2> /dev/null)

define SIM_FLAGS =
INTERNAL_EXTRA_CXX_FLAGS = -I '$(CURDIR)/src/include' -isystem '$(CURDIR)/src/include/others' $(shell $(MYSQL_CONFIG) --include)
INTERNAL_EXTRA_LD_FLAGS = -L '$(CURDIR)/src/lib' -L '$(CURDIR)/src/lib/others' $(shell $(MYSQL_CONFIG) --libs) -lsupc++ -pthread -lrt -lzip -lseccomp
endef

.PHONY: all
all: src/killinstc src/setup-installation src/backup src/job-server src/sim-server src/sim-merger src/sim-upgrader
	@printf "\033[32mBuild finished\033[0m\n"

$(eval $(call include_makefile, src/lib/simlib/Makefile))

.PHONY: test
test: test-sim test-simlib
	@printf "\033[1;32mAll tests passed\033[0m\n"

.PHONY: test-simlib
test-simlib: src/lib/simlib/test

.PHONY: test-sim
test-sim: test/exec test/cpp_syntax_highlighter/check
	test/exec
	@echo cpp_syntax_highlighter:
	test/cpp_syntax_highlighter/check

.PHONY: install
install: $(filter-out install run, $(MAKECMDGOALS))
	@ #   ^ install always have to be executed at the end (but before run)
	# Echo log
	@printf "DESTDIR = \033[01;34m$(abspath $(DESTDIR))\033[0m\n"

	# Installation
	$(MKDIR) $(abspath $(DESTDIR)/static/)
	$(MKDIR) $(abspath $(DESTDIR)/internal_files/)
	$(MKDIR) $(abspath $(DESTDIR)/logs/)
	$(UPDATE) src/static src/sim-server src/job-server src/backup src/sim-merger $(abspath $(DESTDIR))
	# Do not override the config if it already exists
	$(UPDATE) -n src/sim.conf $(abspath $(DESTDIR))
	# $(UPDATE) src/static src/sim-server src/sim-server2 src/sim.conf src/job-server src/backup $(abspath $(DESTDIR))

	# Install PRoot
ifeq ($(shell uname -m), x86_64)
	$(UPDATE) bin/proot-x86_64 $(abspath $(DESTDIR)/proot)
else
	$(UPDATE) bin/proot-x86 $(abspath $(DESTDIR)/proot)
endif

	# Set database pass
	@bash -c 'if [ ! -e $(abspath $(DESTDIR)/.db.config) ]; then\
			echo Type your MySQL username \(for SIM\):; read mysql_username;\
			echo Type your password for $$mysql_username:; read -s mysql_password;\
			echo Type your database which SIM will use:; read db_name;\
			echo Type your user_host:; read user_host;\
			printf "user: \"$$mysql_username\"\npassword: \"$$mysql_password\"\ndb: \"$$db_name\"\nhost: \"$$user_host\"\n" > $(abspath $(DESTDIR)/.db.config);\
		fi'

	# Set up install
	src/setup-installation $(abspath $(DESTDIR)) $(SETUP_INSTALL_FLAGS)

	# Set owner, group and permission bits
	chmod 0700 $(abspath $(DESTDIR)/.db.config) $(abspath $(DESTDIR)/internal_files)
	chmod +x $(abspath $(DESTDIR)/sim-server) $(abspath $(DESTDIR)/job-server) $(abspath $(DESTDIR)/proot)

	@printf "\033[;32mInstallation finished\033[0m\n"

.PHONY: kill
kill: $(filter-out kill run reinstall uninstall, $(MAKECMDGOALS))
	# Kill sim-server and job-server
	src/killinstc --kill-after=3 $(abspath $(DESTDIR)/sim-server) \
		$(abspath $(DESTDIR)/job-server)
		# $(abspath $(DESTDIR)/sim-server2) $(abspath $(DESTDIR)/job-server)

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
	@ # ^ run target always have to be executed at the end
	$(abspath $(DESTDIR)/job-server)&
	$(abspath $(DESTDIR)/sim-server)&
	# $(abspath $(DESTDIR)/sim-server2)&
	@printf "\033[;32mRunning finished\033[0m\n"

$(eval $(call add_static_library, src/lib/sim.a, $(SIM_FLAGS), \
	src/lib/cpp_syntax_highlighter.cc \
	src/lib/jobs.cc \
	src/lib/mysql.cc \
	src/lib/random.cc \
	src/lib/submission.cc \
))

define SQLITE_FLAGS =
INTERNAL_EXTRA_C_FLAGS = -w -DSQLITE_ENABLE_FTS5 -DSQLITE_THREADSAFE=2 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_DEPRECATED \
	-DSQLITE_OMIT_PROGRESS_CALLBACK -DSQLITE_OMIT_SHARED_CACHE \
	-DSQLITE_LIKE_DOESNT_MATCH_BLOBS -DSQLITE_DEFAULT_MEMSTATUS=0 \
	# -DSQLITE_ENABLE_API_ARMOR
endef

src/lib/sqlite3.c: src/lib/sqlite/sqlite3.c # It is a symlink
$(eval $(call add_static_library, src/lib/sqlite3.a, $(SQLITE_FLAGS), \
	src/lib/sqlite3.c \
))

$(eval $(call add_executable, src/job-server, $(SIM_FLAGS), \
	src/job_server/dispatcher.cc \
	src/job_server/job_handlers/add_or_reupload_problem__judge_model_solution_base.cc \
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
	src/job_server/job_handlers/reselect_final_submissions_in_contest_problem.cc \
	src/job_server/job_handlers/reset_problem_time_limits.cc \
	src/job_server/job_handlers/reset_time_limits_in_problem_package_base.cc \
	src/job_server/job_handlers/reupload_problem.cc \
	src/job_server/main.cc \
	src/lib/sim.a \
	src/lib/simlib/simlib.a \
))

$(eval $(call add_executable, src/sim-merger, $(SIM_FLAGS), \
	src/lib/sim.a \
	src/lib/simlib/simlib.a \
	src/sim_merger/sim_merger.cc \
))

$(eval $(call add_executable, src/sim-upgrader, $(SIM_FLAGS), \
	src/lib/sim.a \
	src/lib/simlib/simlib.a \
	src/sim_upgrader.cc \
))

$(eval $(call add_executable, src/killinstc, $(SIM_FLAGS), \
	src/killinstc.cc \
	src/lib/simlib/simlib.a \
))

$(eval $(call add_executable, src/setup-installation, $(SIM_FLAGS), \
	src/lib/sim.a \
	src/lib/simlib/simlib.a \
	src/setup_installation.cc \
))

$(eval $(call add_executable, src/backup, $(SIM_FLAGS), \
	src/backup.cc \
	src/lib/sim.a \
	src/lib/simlib/simlib.a \
))

# Technique used to force browsers to always keep up-to-date version of the files below
src/web_interface/template.o: override INTERNAL_EXTRA_CXX_FLAGS += '-DSTYLES_CSS_HASH="$(shell printf '%x' $$(stat -c '%Y' src/static/kit/styles.css))"'
src/web_interface/template.o: override INTERNAL_EXTRA_CXX_FLAGS += '-DJQUERY_JS_HASH="$(shell printf '%x' $$(stat -c '%Y' src/static/kit/jquery.js))"'
src/web_interface/template.o: override INTERNAL_EXTRA_CXX_FLAGS += '-DSCRIPTS_JS_HASH="$(shell printf '%x' $$(stat -c '%Y' src/static/kit/scripts.js))"'

$(eval $(call add_executable, src/sim-server, $(SIM_FLAGS), \
	src/lib/sim.a \
	src/lib/simlib/simlib.a \
	src/web_interface/api.cc \
	src/web_interface/connection.cc \
	src/web_interface/contest_entry_token_api.cc \
	src/web_interface/contest_users_api.cc \
	src/web_interface/contests.cc \
	src/web_interface/contests_api.cc \
	src/web_interface/contest_files.cc \
	src/web_interface/contest_files_api.cc \
	src/web_interface/http_request.cc \
	src/web_interface/http_response.cc \
	src/web_interface/jobs.cc \
	src/web_interface/jobs_api.cc \
	src/web_interface/problems.cc \
	src/web_interface/problems_api.cc \
	src/web_interface/server.cc \
	src/web_interface/session.cc \
	src/web_interface/sim.cc \
	src/web_interface/submissions.cc \
	src/web_interface/submissions_api.cc \
	src/web_interface/template.cc \
	src/web_interface/users.cc \
	src/web_interface/users_api.cc \
))

$(eval $(call add_executable, test/exec, $(SIM_FLAGS), \
	src/lib/sim.a \
	src/lib/simlib/gtest_main.a \
	src/lib/simlib/simlib.a \
	test/jobs.cc \
))

BUILD_ARTIFACTS += $(wildcard test/cpp_syntax_highlighter/tests/*.ans)
$(eval $(call add_executable, test/cpp_syntax_highlighter/check, $(SIM_FLAGS), \
	src/lib/sim.a \
	src/lib/simlib/simlib.a \
	test/cpp_syntax_highlighter/check.cc \
))

.PHONY: format
format: src/lib/simlib/format
format: $(shell find bin scripts src test | grep -E '\.(cc?|hh?)$$' | grep -vE '^(src/lib/simlib/.*|src/include/others/.*|src/lib/sqlite(/.*|3.c))$$' | sed 's/$$/-make-format/')
