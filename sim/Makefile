include Makefile.config

DESTDIR = build

.PHONY: all
all: build-info
	@$(MAKE) src
ifeq ($(MAKELEVEL), 0)
	@echo "\033[32mBuild finished\033[0m"
endif

.PHONY: build-info
build-info:
ifeq ($(MAKELEVEL), 0)
	@echo "DEBUG: $(DEBUG)"
	@echo "CC -> $(CC)"
	@echo "CXX -> $(CXX)"
endif

.PHONY: src
src: build-info
	$(Q)$(MAKE) -C $@

.PHONY: test
test: test-sim test-simlib
	@echo "\033[1;32mAll tests passed\033[0m"

.PHONY: build-test
build-test: build-test-sim build-test-simlib

.PHONY: test-sim
test-sim: src
	$(Q)$(MAKE) -C test/

.PHONY: build-test-sim
build-test-sim: src
	$(Q)$(MAKE) -C test/ build

.PHONY: test-simlib
test-simlib: src
	$(Q)$(MAKE) -C src/lib/simlib/ test

.PHONY: build-test-simlib
build-test-simlib: src
	$(Q)$(MAKE) -C src/lib/simlib/ build-test

.PHONY: importer
importer: src
	$(Q)$(MAKE) -C importer/

.PHONY: install
install: $(filter-out install run, $(MAKECMDGOALS))
	@ #   ^ install always have to be executed at the end (but before run)
	# Echo log
	@echo "DESTDIR = \033[01;34m$(abspath $(DESTDIR))\033[0m"

	# Installation
	$(MKDIR) $(abspath $(DESTDIR)/problems/)
	$(MKDIR) $(abspath $(DESTDIR)/solutions/)
	$(MKDIR) $(abspath $(DESTDIR)/static/)
	$(MKDIR) $(abspath $(DESTDIR)/files/)
	$(MKDIR) $(abspath $(DESTDIR)/jobs_files/)
	$(MKDIR) $(abspath $(DESTDIR)/logs/)
	$(UPDATE) src/static src/sim-server src/server.conf src/job-server src/backup $(abspath $(DESTDIR))
	# $(UPDATE) src/static src/sim-server src/sim-server2 src/server.conf src/job-server src/backup $(abspath $(DESTDIR))

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
	chmod 0700 $(abspath $(DESTDIR)/.db.config) $(abspath $(DESTDIR)/solutions) $(abspath $(DESTDIR)/problems)
	chmod +x $(abspath $(DESTDIR)/sim-server) $(abspath $(DESTDIR)/job-server) $(abspath $(DESTDIR)/proot)

	@echo "\033[;32mInstallation finished\033[0m"

.PHONY: reinstall
reinstall: SETUP_INSTALL_FLAGS += --drop-tables
reinstall: $(filter-out reinstall run, $(MAKECMDGOALS))
	@ #     ^ reinstall always have to be executed at the end (but before run)
	# Kill sim-server and job-server
	src/killinstc $(abspath $(DESTDIR)/sim-server)
	src/killinstc $(abspath $(DESTDIR)/job-server)

	# Delete files
	$(RM) -r $(abspath $(DESTDIR)/)
	$(MAKE) install

.PHONY: uninstall
uninstall: SETUP_INSTALL_FLAGS += --only-drop-tables
uninstall:
	# Kill sim-server and job-server
	src/killinstc $(abspath $(DESTDIR)/sim-server)
	# src/killinstc $(abspath $(DESTDIR)/sim-server2)
	src/killinstc $(abspath $(DESTDIR)/job-server)

	# Delete files and database tables
	src/setup-installation $(abspath $(DESTDIR)) $(SETUP_INSTALL_FLAGS)
	$(RM) -r $(abspath $(DESTDIR)/)

.PHONY: run
run: $(filter-out run, $(MAKECMDGOALS))
	@ # ^ run always have to be executed at the end

	src/killinstc --kill-after=3 $(abspath $(DESTDIR)/sim-server) \
		$(abspath $(DESTDIR)/job-server)
		# $(abspath $(DESTDIR)/sim-server2) $(abspath $(DESTDIR)/job-server)

	# Run
	$(abspath $(DESTDIR)/job-server)&
	$(abspath $(DESTDIR)/sim-server)&
	# $(abspath $(DESTDIR)/sim-server2)&
	@echo "\033[;32mRunning finished\033[0m"

.PHONY: clean
clean:
	$(Q)$(MAKE) clean -C src/
	$(Q)$(MAKE) clean -C test/
	$(Q)$(MAKE) clean -C importer/

.PHONY: help
help:
	@echo "Nothing is here yet..."
