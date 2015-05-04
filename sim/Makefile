include Makefile.config

DESTDIR = build

.PHONY: all
all:
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
	$(Q)$(MAKE) -C src/
	@printf "\033[;32mBuild finished\033[0m\n"

.PHONY: install
install: all
	# Echo log
	@printf "\033[01;34m$(abspath $(DESTDIR))\033[0m\n"

	# Installation
	@if test `whoami` != "root"; then printf "\033[01;31mYou have to run it as root!\033[0m\n"; exit 1; fi
	$(MKDIR) $(abspath $(DESTDIR)/problems/)
	$(MKDIR) $(abspath $(DESTDIR)/judge/chroot/)
	$(MKDIR) $(abspath $(DESTDIR)/solutions/)
	$(MKDIR) $(abspath $(DESTDIR)/public/)
	$(UPDATE) src/public $(abspath $(DESTDIR))
	$(UPDATE) src/judge_machine $(abspath $(DESTDIR)/judge/)
	$(UPDATE) src/sim-server src/conver $(abspath $(DESTDIR))

	# Set database pass
	@bash -c 'if [ ! -e $(abspath $(DESTDIR)/db.config) ]; then\
			echo Type your MySQL username \(for SIM\):; read mysql_username;\
			echo Type your password for $$mysql_username:; read -s mysql_password;\
			echo Type your database which SIM will use:; read db_name;\
			echo Type your user_host:; read user_host;\
			printf "$$mysql_username\n$$mysql_password\n$$db_name\n$$user_host\n" > $(abspath $(DESTDIR)/db.config);\
		fi'

	# Set up install
	- src/setup_install $(abspath $(DESTDIR))

	# Right owner, group and permission bits
	- useradd -M -r -s /usr/sbin/nologin sim
	src/chmod-default $(abspath $(DESTDIR))
	chmod 0700 $(abspath $(DESTDIR)/db.config)
	chmod +x $(abspath $(DESTDIR)/judge/CTH) $(abspath $(DESTDIR)/judge/judge_machine) $(abspath $(DESTDIR)/sim-server) $(abspath $(DESTDIR)/conver)
	chown -R sim:sim $(abspath $(DESTDIR))

	# Add judge_machine to /etc/sudoers
	@grep 'ALL ALL = (root) NOPASSWD: $(abspath $(DESTDIR)/judge/judge_machine)' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(abspath $(DESTDIR)/judge/judge_machine) >> /etc/sudoers; fi
	@printf "\033[;32mInstallation finished\033[0m\n"

.PHONY: reinstall
reinstall:
	$(RM) $(abspath $(DESTDIR)/db.config)
	$(MAKE) install

.PHONY: clean
clean:
	$(Q)$(MAKE) clean -C src/

.PHONY: help
help:
	@echo "Nothing is here yet..."
