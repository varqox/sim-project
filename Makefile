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
	$(MKDIR) $(INSTALL_DIR)/problems/
	$(UPDATE) src/public/ $(INSTALL_DIR)
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

	# Right owner, group and permission bits
	src/cpp/chmod-default $(INSTALL_DIR)
	chmod 0755 $(INSTALL_DIR)judge/CTH $(INSTALL_DIR)judge/judge_machine
	chown -R www-data:www-data $(INSTALL_DIR)
	chmod 0750 $(INSTALL_DIR)php/

	# Add judge_machine to sudoers
	@grep 'ALL ALL = (root) NOPASSWD: $(INSTALL_DIR)judge/judge_machine' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(INSTALL_DIR)judge/judge_machine >> /etc/sudoers; fi

	# Configure nginx
	@echo "server {" > /etc/nginx/sites-available/sim
	@echo "    listen 127.2.2.2:80 default; ## listen for ipv4" >> /etc/nginx/sites-available/sim
	@echo "    # listen [::]:80 default ipv6only=on; ## listen for ipv6" >> /etc/nginx/sites-available/sim
	@echo "" >> /etc/nginx/sites-available/sim
	@echo "    # server_name localhost;" >> /etc/nginx/sites-available/sim
	@echo "    server_name_in_redirect off;" >> /etc/nginx/sites-available/sim
	@echo "" >> /etc/nginx/sites-available/sim
	@echo "    charset utf-8;" >> /etc/nginx/sites-available/sim
	@echo "" >> /etc/nginx/sites-available/sim
	@echo "    access_log "'$(INSTALL_DIR)'"access.log;" >> /etc/nginx/sites-available/sim
	@echo "    error_log "'$(INSTALL_DIR)'"error.log;" >> /etc/nginx/sites-available/sim
	@echo "" >> /etc/nginx/sites-available/sim
	@echo "    root "'$(INSTALL_DIR)'"public/;" >> /etc/nginx/sites-available/sim
	@echo "    index index.php index.html index.htm;" >> /etc/nginx/sites-available/sim
	@echo "" >> /etc/nginx/sites-available/sim
	@echo "    location / {" >> /etc/nginx/sites-available/sim
	@echo "        try_files \$$uri \$$uri/ =404;" >> /etc/nginx/sites-available/sim
	@echo "    }" >> /etc/nginx/sites-available/sim
	@echo "" >> /etc/nginx/sites-available/sim
	@echo "    location ~ \\.php\$$ {" >> /etc/nginx/sites-available/sim
	@echo "        try_files \$$uri =404;" >> /etc/nginx/sites-available/sim
	@echo "        include /etc/nginx/fastcgi_params;" >> /etc/nginx/sites-available/sim
	@echo "        fastcgi_split_path_info ^(.+\\.php)(/.+)\$$;" >> /etc/nginx/sites-available/sim
	@echo "        fastcgi_pass "$(shell var=`grep "listen =" /etc/php5/fpm/pool.d/www.conf | tail -c+10`; first=`echo $${var} | head -c1`; if [ "$$first" = "/" ]; then echo "unix:$$var"; else echo $$var; fi)";"  >> /etc/nginx/sites-available/sim
	@echo "        fastcgi_index index.php;" >> /etc/nginx/sites-available/sim
	@echo "        fastcgi_param SCRIPT_FILENAME \$$document_root\$$fastcgi_script_name;" >> /etc/nginx/sites-available/sim
	@echo "    }" >> /etc/nginx/sites-available/sim
	@echo "}" >> /etc/nginx/sites-available/sim
	ln -fs /etc/nginx/sites-available/sim /etc/nginx/sites-enabled/
	-nginx -s reload

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
