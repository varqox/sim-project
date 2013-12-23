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
	$(MKDIR) $(abspath $(DESTDIR)/judge/chroot/) $(abspath $(DESTDIR)/solutions/)
	$(UPDATE) src/judge_machine src/CTH src/checkers/ $(abspath $(DESTDIR)/judge/)

	# Setup php and database
#	@bash -c 'if [ ! -e $(abspath $(DESTDIR)/php/db.pass) ]; then\
			echo Type your MySQL username \(for SIM\):; read mysql_username;\
			echo Type your password for $$mysql_username:; read -s mysql_password;\
			echo Type your database which SIM will use:; read db_name;\
			printf "$$mysql_username\n$$mysql_password\n$$db_name\n" > $(abspath $(DESTDIR)/php/db.pass);\
		fi'
#	echo | php -B '$$_SERVER["DOCUMENT_ROOT"]="$(abspath $(DESTDIR)/public/)";' -F $(abspath $(DESTDIR)/php/db_setup.php)
#	$(RM) $(abspath $(DESTDIR)/php/db_setup.php)

	# Right owner, group and permission bits
	src/chmod-default $(DESTDIR)
	chmod 0755 $(abspath $(DESTDIR)/judge/CTH) $(abspath $(DESTDIR)/judge/judge_machine)
	chown -R www-data:www-data $(DESTDIR)

	# Add judge_machine to sudoers
	@grep 'ALL ALL = (root) NOPASSWD: $(abspath $(DESTDIR)/judge/judge_machine)' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(abspath $(DESTDIR)/judge/judge_machine) >> /etc/sudoers; fi

.PHONY: clean
clean:
	$(Q)$(MAKE) clean -C src/

.PHONY: help
help:
	@echo "Nothing is here yet..."
