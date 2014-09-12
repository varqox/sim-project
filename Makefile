export

# Extra options to compile project with
EXTRA_OPTIONS = -O3

# Extra options to link project with
EXTRA_LD_OPTIONS = -s -O3

# Warnings options to compile project with
WARNING_OPTIONS = -Wall -Wextra -Wabi -Weffc++ -Wshadow -Wfloat-equal -Wno-unused-result

CFLAGS = $(WARNING_OPTIONS) $(EXTRA_OPTIONS) -c
CXXFLAGS = $(CFLAGS)
LFLAGS = $(WARNING_OPTIONS) $(EXTRA_LD_OPTIONS)

# Shell commands
MV = mv -f
CP = cp -rf
UPDATE = $(CP) -u
RM = rm -f
MKDIR = mkdir -p

# Installation settings
INSTALL_DIR := build
override INSTALL_DIR := $(shell readlink -f $(INSTALL_DIR))
ifneq ($(shell printf $(INSTALL_DIR) | tail -c 1), $(shell printf "/"))
override INSTALL_DIR := $(INSTALL_DIR)/
endif

# Rest
ROOT := $(shell pwd)
ifneq ($(shell printf $(ROOT) | tail -c 1), $(shell printf "/"))
ROOT := $(ROOT)/
else
ROOT := $(ROOT)
endif

ifeq ($(VERBOSE),1)
	Q :=
	P =
else
	Q := @
	P = printf "   $$(1)\t $$(subst $(ROOT),,$$(abspath $$(2)))\n";
	override MFLAGS += --no-print-directory -s
endif

PHONY := all
all:
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
	$(Q)$(MAKE) $(MFLAGS) -C src/cpp/
	@printf "\033[;32mBuild finished\033[0m\n"

PHONY += debug
debug: override CC += -DDEBUG
debug: override CXX += -DDEBUG
debug:
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
	$(Q)$(MAKE) $(MFLAGS) -C src/cpp/
	@printf "\033[;32mBuild finished\033[0m\n"

PHONY += hard-debug
hard-debug: override CC += -DDEBUG
hard-debug: override CXX += -DDEBUG
hard-debug: override CFLAGS = $(WARNING_OPTIONS) -g -c
hard-debug: override CXXFLAGS = $(WARNING_OPTIONS) -g -c
hard-debug: override LFLAGS = $(WARNING_OPTIONS) -g
hard-debug:
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
	$(Q)$(MAKE) $(MFLAGS) -C src/cpp/
	@printf "\033[;32mBuild finished\033[0m\n"

PHONY += install
install: all
	# Echo log
	@printf "\033[01;34m$(INSTALL_DIR)\033[0m\n"

	# Installation
	@if test `whoami` != "root"; then printf "\033[01;31mYou have to run it as root!\033[0m\n"; exit 1; fi
	$(MKDIR) $(INSTALL_DIR)
	$(UPDATE) src/public/ src/tasks/ $(INSTALL_DIR)
	$(MKDIR) $(INSTALL_DIR)judge/chroot/ $(INSTALL_DIR)php/ $(INSTALL_DIR)solutions/
	$(UPDATE) src/cpp/judge_machine src/cpp/CTH src/checkers/ $(INSTALL_DIR)judge/

	# Setup php and database
	$(UPDATE) src/php/ $(INSTALL_DIR)
	@bash -c 'if [ ! -e $(INSTALL_DIR)php/db.pass ]; then\
			echo Type your MySQL username \(for SIM\):; read mysql_username;\
			echo Type your password for $$mysql_username:; read -s mysql_password;\
			echo Type your database which SIM will use:; read db_name;\
			printf "$$mysql_username\n$$mysql_password\n$$db_name\n" > $(INSTALL_DIR)php/db.pass;\
		fi'
	echo | php -B '$$_SERVER["DOCUMENT_ROOT"]="$(INSTALL_DIR)public/";' -F $(INSTALL_DIR)php/db_setup.php
	$(RM) $(INSTALL_DIR)php/db_setup.php

	src/cpp/chmod-default $(INSTALL_DIR)
	chmod 0755 $(INSTALL_DIR)judge/CTH $(INSTALL_DIR)judge/judge_machine
	chown -R www-data:www-data $(INSTALL_DIR)
	chmod 0750 $(INSTALL_DIR)php/
	@grep 'ALL ALL = (root) NOPASSWD: $(INSTALL_DIR)judge/judge_machine' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(INSTALL_DIR)judge/judge_machine >> /etc/sudoers; fi
	@printf "<VirtualHost 127.2.2.2:80>\n	ServerName sim.localhost\n\n	DocumentRoot %s\n	<Directory />\n		Options FollowSymLinks\n		AllowOverride All\n	</Directory>\n	<Directory %s>\n		Options Indexes FollowSymLinks MultiViews\n		AllowOverride All\n		Order allow,deny\n		allow from all\n		Require all granted\n	</Directory>\n	CustomLog \$${APACHE_LOG_DIR}/access.log combined\n</VirtualHost>" $(INSTALL_DIR)public/ $(INSTALL_DIR)public/ > /etc/apache2/sites-available/sim.conf
	a2ensite sim
	-service apache2 reload

PHONY += reinstall
reinstall:
	$(RM) -r $(INSTALL_DIR)php
	$(MAKE) install

PHONY += clean
clean:
	$(Q)$(RM) `find src/ -regex '.*\.\(o\|dep-s\)$$'`

PHONY += mrproper
mrproper: clean
	$(Q)$(MAKE) $(MFLAGS) mrproper -C src/cpp/
	$(Q)$(RM) sim.tar.gz

PHONY += help
help:
	@echo "Nothing is here yet..."

.PHONY: $(PHONY)