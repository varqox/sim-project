include Makefile.config

DESTDIR = build

.PHONY: build
build:
ifeq ($(MAKELEVEL), 0)
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
endif
	$(Q)$(MAKE) -C src/
ifeq ($(MAKELEVEL), 0)
	@printf "\033[32mBuild finished\033[0m\n"
endif

.PHONY: test
test: build
ifeq ($(MAKELEVEL), 0)
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
endif
	$(Q)$(MAKE) -C test/
ifeq ($(MAKELEVEL), 0)
	@printf "\033[1;32mAll tests passed\033[0m\n"
endif

.PHONY: install
install: build
	# Echo log
	@printf "DESTDIR = \033[01;34m$(abspath $(DESTDIR))\033[0m\n"

	# Installation
	$(MKDIR) $(abspath $(DESTDIR)/problems/)
	$(MKDIR) $(abspath $(DESTDIR)/solutions/)
	$(MKDIR) $(abspath $(DESTDIR)/public/)
	$(MKDIR) $(abspath $(DESTDIR)/files/)
	$(MKDIR) $(abspath $(DESTDIR)/logs/)
	$(UPDATE) src/public src/sim-server src/sim-server2 src/server.conf src/conver src/judge-machine $(abspath $(DESTDIR))

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
			printf "$$mysql_username\n$$mysql_password\n$$db_name\n$$user_host\n" > $(abspath $(DESTDIR)/.db.config);\
		fi'

	# Set up install
	src/setup-installation $(abspath $(DESTDIR)) $(SETUP_INSTALL_FLAGS)

	# Set owner, group and permission bits
	chmod 0700 $(abspath $(DESTDIR)/.db.config) $(abspath $(DESTDIR)/solutions) $(abspath $(DESTDIR)/problems)
	chmod +x $(abspath $(DESTDIR)/sim-server) $(abspath $(DESTDIR)/conver) $(abspath $(DESTDIR)/judge-machine) $(abspath $(DESTDIR)/proot)

	@printf "\033[;32mInstallation finished\033[0m\n"

.PHONY: reinstall
reinstall: SETUP_INSTALL_FLAGS += --drop-tables
reinstall: build
	# Kill sim-server and judge-machine
	src/killinstc $(abspath $(DESTDIR)/sim-server)
	src/killinstc $(abspath $(DESTDIR)/judge-machine)

	# Delete files
	$(RM) -r $(abspath $(DESTDIR)/)
	$(MAKE) install

.PHONY: uninstall
uninstall: SETUP_INSTALL_FLAGS += --only-drop-tables
uninstall:
	# Kill sim-server and judge-machine
	src/killinstc $(abspath $(DESTDIR)/sim-server)
	src/killinstc $(abspath $(DESTDIR)/sim-server2)
	src/killinstc $(abspath $(DESTDIR)/judge-machine)

	# Delete files and database tables
	src/setup-installation $(abspath $(DESTDIR)) $(SETUP_INSTALL_FLAGS)
	$(RM) -r $(abspath $(DESTDIR)/)

.PHONY: run
run: $(filter-out run, $(MAKECMDGOALS))
	@ # ^ run always have to be executed at the end

	# Kill sim-server and judge-machine
	src/killinstc $(abspath $(DESTDIR)/sim-server)
	src/killinstc $(abspath $(DESTDIR)/sim-server2)
	src/killinstc $(abspath $(DESTDIR)/judge-machine)

	# Run
	$(abspath $(DESTDIR)/judge-machine)&
	$(abspath $(DESTDIR)/sim-server)&
	# $(abspath $(DESTDIR)/sim-server2)&

.PHONY: clean
clean:
	$(Q)$(MAKE) clean -C src/
	$(Q)$(MAKE) clean -C test/

.PHONY: help
help:
	@echo "Nothing is here yet..."
