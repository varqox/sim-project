export
# Compile commands
ifeq ($(shell clang > /dev/null 2> /dev/null; echo $$?), $(shell echo 1))
CC := clang
else
CC := gcc
endif
ifeq ($(shell clang++ > /dev/null 2> /dev/null; echo $$?), $(shell echo 1))
CXX := clang++
LINK := clang++
else
CXX := g++
LINK := g++
endif

CFLAGS = -Wall -Wextra -Wabi -Weffc++ -Wshadow -Wfloat-equal -Wno-unused-result -O3 -c
CXXFLAGS := $(CFLAGS)
LFLAGS = -Wall -Wextra -Wabi -Weffc++ -Wshadow -Wfloat-equal -Wno-unused-result -s -O3

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
	MFLAGS += -s
endif

PHONY := all
all: src/chmod-default
	$(Q)$(MAKE) $(MFLAGS) -C src/judge/ dep-s
	$(Q)$(MAKE) $(MFLAGS) -C src/judge/
	@printf "\033[;32mBuild finished\033[0m\n"

PHONY += debug
debug: CXX += -DDEBUG
debug: CC += -DDEBUG
debug: all

src/chmod-default: src/chmod-default.cpp
ifeq ($(VERBOSE),1)
	$(Q)$(LINK) $< $(LFLAGS) -o $@
else
	$(Q)printf "   LINK\t $(subst $(ROOT),,$(abspath $@))\n";\
		$(LINK) $< $(LFLAGS) -o $@
endif

PHONY += install
install: all
	# Echo log
	@printf "\033[01;34m$(INSTALL_DIR)\033[0m\n"

	# Installation
	@if test `whoami` != "root"; then printf "\033[01;31mYou have to run it as root!\033[0m\n"; exit 1; fi
	$(MKDIR) $(INSTALL_DIR)
	$(UPDATE) src/public/ src/solutions/ src/tasks/ $(INSTALL_DIR)
	$(MKDIR) $(INSTALL_DIR)judge/chroot/ $(INSTALL_DIR)php/
	$(UPDATE) src/judge/judge_machine src/judge/CTH src/judge/checkers/ $(INSTALL_DIR)judge/

	# Setup php and database
	$(UPDATE) `find src/php/* ! -name db.php` $(INSTALL_DIR)php
	@bash -c 'if [ -a $(INSTALL_DIR)php/db.php ]; then\
			db_php=`head -n5 $(INSTALL_DIR)php/db.php`;\
		else\
			echo Type your MySQL username \(for SIM\):; read mysql_username;\
			echo Type your password for $$mysql_username:; read -s mysql_password;\
			echo Type your database which SIM will use:; read db_name;\
			db_php=`printf "<?php\ndefine('"'"'mysql_host'"'"', '"'"'localhost'"'"');\ndefine('"'"'mysql_username'"'"', '"'"'$$mysql_username'"'"');\ndefine('"'"'mysql_password'"'"', '"'"'$$mysql_password'"'"');\ndefine('"'"'database'"'"', '"'"'$$db_name'"'"');"`;\
		fi;\
		echo "$$db_php" > $(INSTALL_DIR)php/db.php; tail -n+6 src/php/db.php >> $(INSTALL_DIR)php/db.php'
	php $(INSTALL_DIR)php/db_setup.php
	$(RM) $(INSTALL_DIR)php/db_setup.php

	# chmod -R 0755 $(INSTALL_DIR)
	src/chmod-default $(INSTALL_DIR)
	chmod 0755 $(INSTALL_DIR)judge/CTH $(INSTALL_DIR)judge/judge_machine
	chown -R www-data:www-data $(INSTALL_DIR)
	@grep 'ALL ALL = (root) NOPASSWD: $(INSTALL_DIR)judge/judge_machine' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(INSTALL_DIR)judge/judge_machine >> /etc/sudoers; fi
	@printf "<VirtualHost 127.2.2.2:80>\n	ServerAdmin webmaster@sim.localhost\n	ServerName sim.localhost\n\n	DocumentRoot %s\n	<Directory />\n		Options FollowSymLinks\n		AllowOverride All\n	</Directory>\n	<Directory %s>\n		Options Indexes FollowSymLinks MultiViews\n		AllowOverride All\n		Order allow,deny\n		allow from all\n		Require all granted\n	</Directory>\n	CustomLog \$${APACHE_LOG_DIR}/access.log combined\n</VirtualHost>" $(INSTALL_DIR)public/ $(INSTALL_DIR)public/ > /etc/apache2/sites-available/sim.conf
	a2ensite sim
	-service apache2 reload

PHONY += reinstall
reinstall:
	$(RM) -r $(INSTALL_DIR)/php
	$(MAKE) $(MFLAGS) intall

PHONY += clean
clean:
	$(Q)$(RM) `find -regex '.*\.\(o\|dep-s\)$$'` src/chmod-default

PHONY += mrproper
mrproper: clean
	$(Q)$(MAKE) $(MFLAGS) mrproper -C src/judge/
	$(Q)$(RM) sim.tar.gz

PHONY += help
help:
	@echo "Nothing is here yet..."

.PHONY: $(PHONY)
