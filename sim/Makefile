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
INSTALL_DIR = build
OUTPUT_DIR := $(shell readlink -f $(INSTALL_DIR))
ifneq ($(shell printf $(OUTPUT_DIR) | tail -c 1), $(shell printf "/"))
OUTPUT_DIR := $(OUTPUT_DIR)/
else
OUTPUT_DIR := $(OUTPUT_DIR)
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
	P_CC :=
	P_CXX :=
	P_LINK :=
else
	P_CC := printf "   CC\t $$(subst $(ROOT),,$$(abspath $$@))\n";
	P_CXX := printf "   CXX\t $$(subst $(ROOT),,$$(abspath $$@))\n";
	P_LINK := printf "   LINK\t $$(subst $(ROOT),,$$(abspath $$@))\n";
	Q := @
	MFLAGS += -s
endif

PHONY := all
all: src/chmod-default
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
	@printf "\033[01;34m$(OUTPUT_DIR)\033[0m\n"

	# Installation
	@if test `whoami` != "root"; then printf "\033[01;31mYou have to run it as root!\033[0m\n"; exit 1; fi
	$(MKDIR) $(OUTPUT_DIR)
	$(UPDATE) src/public/ src/solutions/ src/tasks/ $(OUTPUT_DIR)
	$(MKDIR) $(OUTPUT_DIR)judge/chroot/ $(OUTPUT_DIR)php/
	$(UPDATE) src/judge/judge_machine src/judge/CTH src/judge/checkers/ $(OUTPUT_DIR)judge/

	# Setup php and database
	$(UPDATE) `find src/php/* ! -name db.php` $(OUTPUT_DIR)php
	@bash -c 'if [ -a $(OUTPUT_DIR)php/db.php ]; then\
			db_php=`head -n5 $(OUTPUT_DIR)php/db.php`;\
		else\
			echo Type your MySQL username \(for SIM\):; read mysql_username;\
			echo Type your password for $$mysql_username:; read -s mysql_password;\
			echo Type your database which SIM will use:; read db_name;\
			db_php=`printf "<?php\ndefine('"'"'mysql_host'"'"', '"'"'localhost'"'"');\ndefine('"'"'mysql_username'"'"', '"'"'$$mysql_username'"'"');\ndefine('"'"'mysql_password'"'"', '"'"'$$mysql_password'"'"');\ndefine('"'"'database'"'"', '"'"'$$db_name'"'"');"`;\
		fi;\
		echo "$$db_php" > $(OUTPUT_DIR)php/db.php; tail -n+6 src/php/db.php >> $(OUTPUT_DIR)php/db.php'
	php $(OUTPUT_DIR)php/db_setup.php
	$(RM) $(OUTPUT_DIR)php/db_setup.php

	# chmod -R 0755 $(OUTPUT_DIR)
	src/chmod-default $(OUTPUT_DIR)
	chmod 0755 $(OUTPUT_DIR)judge/CTH $(OUTPUT_DIR)judge/judge_machine
	chown -R www-data:www-data $(OUTPUT_DIR)
	@grep 'ALL ALL = (root) NOPASSWD: $(OUTPUT_DIR)judge/judge_machine' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(OUTPUT_DIR)judge/judge_machine >> /etc/sudoers; fi
	@printf "<VirtualHost 127.2.2.2:80>\n	ServerAdmin webmaster@sim.localhost\n	ServerName sim.localhost\n\n	DocumentRoot %s\n	<Directory />\n		Options FollowSymLinks\n		AllowOverride All\n	</Directory>\n	<Directory %s>\n		Options Indexes FollowSymLinks MultiViews\n		AllowOverride All\n		Order allow,deny\n		allow from all\n		Require all granted\n	</Directory>\n	CustomLog \$${APACHE_LOG_DIR}/access.log combined\n</VirtualHost>" $(OUTPUT_DIR)public/ $(OUTPUT_DIR)public/ > /etc/apache2/sites-available/sim.conf
	a2ensite sim
	-service apache2 reload

PHONY += reinstall
reinstall:
	$(RM) -r $(OUTPUT_DIR)/php
	$(MAKE) $(MFLAGS) intall

PHONY += clean
clean:
	$(Q)$(RM) `find -regex '.*\.\(o\|mkdep\)'` src/chmod-default

PHONY += mrproper
mrproper: clean
	$(Q)$(MAKE) $(MFLAGS) mrproper -C src/judge/
	$(Q)$(RM) sim.tar.gz

PHONY += help
help:
	@echo "Nothing is here yet..."

.PHONY: $(PHONY)
