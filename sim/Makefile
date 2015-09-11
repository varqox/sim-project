include Makefile.config

DESTDIR = build

.PHONY: all
all:
ifeq ($(MAKELEVEL), 0)
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
endif
	$(Q)$(MAKE) -C src/
ifeq ($(MAKELEVEL), 0)
	@printf "\033[;32mBuild finished\033[0m\n"
endif

.PHONY: install
install: all
	# Echo log
	@printf "DESTDIR = \033[01;34m$(abspath $(DESTDIR))\033[0m\n"

	# Installation
	$(MKDIR) $(abspath $(DESTDIR)/problems/)
	$(MKDIR) $(abspath $(DESTDIR)/solutions/)
	$(MKDIR) $(abspath $(DESTDIR)/submissions/)
	$(MKDIR) $(abspath $(DESTDIR)/public/)
	$(UPDATE) src/public src/sim-server src/server.conf src/conver src/judge-machine src/CTH $(abspath $(DESTDIR))

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
	src/setup-install $(abspath $(DESTDIR))

	# Set owner, group and permission bits
	# src/chmod-default $(abspath $(DESTDIR))
	chmod 0700 $(abspath $(DESTDIR)/.db.config) $(abspath $(DESTDIR)/solutions) $(abspath $(DESTDIR)/problems)
	chmod +x $(abspath $(DESTDIR)/sim-server) $(abspath $(DESTDIR)/conver) $(abspath $(DESTDIR)/judge-machine) $(abspath $(DESTDIR)/CTH) $(abspath $(DESTDIR)/proot)

	@printf "\033[;32mInstallation finished\033[0m\n"

.PHONY: reinstall
reinstall:
	$(RM) $(abspath $(DESTDIR)/.db.config)
	$(MAKE) install

.PHONY: run
run:
	cd $(DESTDIR) && ./judge-machine&
	cd $(DESTDIR) && ./sim-server&

.PHONY: clean
clean:
	$(Q)$(MAKE) clean -C src/

.PHONY: help
help:
	@echo "Nothing is here yet..."
