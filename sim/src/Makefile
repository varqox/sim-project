# Compile commands
ifeq ($(shell clang > /dev/null 2> /dev/null; echo $$?), $(shell echo 1))
export CC = clang
else
export CC = gcc
endif
ifeq ($(shell clang++ > /dev/null 2> /dev/null; echo $$?), $(shell echo 1))
export CXX = clang++
export LINK = clang++
else
export CXX = g++
export LINK = g++
endif

export CFLAGS = -Wall -W -Wabi -Weffc++ -Wformat -Wshadow -Wsequence-point -Wuninitialized -Wfloat-equal -Wno-unused-result -O3 -c
export CXXFLAGS = -Wall -W -Wabi -Weffc++ -Wformat -Wshadow -Wsequence-point -Wuninitialized -Wfloat-equal -Wno-unused-result -O3 -c
export LFLAGS = -Wall -W -Wabi -Weffc++ -Wformat -Wshadow -Wsequence-point -Wuninitialized -Wfloat-equal -Wno-unused-result -s -O3

# Shell commands
export MV = mv -f
export CP = cp -f -r
export UPDATE = $(CP) -u
export RM = rm -f
export MKDIR = mkdir -p

# Installation settings
INSTALL_DIR = $(shell pwd)/../build/
ifneq ($(shell printf "%c" $(INSTALL_DIR)), $(shell printf "/"))
TMP_OUTPUT_DIR = $(shell pwd)/$(INSTALL_DIR)
else
TMP_OUTPUT_DIR = $(INSTALL_DIR)
endif
ifneq ($(shell printf $(INSTALL_DIR) | tail -c 1), $(shell printf "/"))
OUTPUT_DIR=$(TMP_OUTPUT_DIR)/
else
OUTPUT_DIR=$(TMP_OUTPUT_DIR)
endif

# Rest
.PHONY: all install clean dist-clean package help

all: chmod-default
	make -C judge $(MFLAGS)

chmod-default: chmod-default.cpp
	$(LINK) $< $(LFLAGS) -o $@

install: all
	# Echo log
	@printf "\033[01;34m$(INSTALL_DIR)\033[00m\n"
	@printf "\033[01;34m$(OUTPUT_DIR)\033[00m\n"
	# Installation
	@if test `whoami` != "root"; then echo You have to run it as root!; exit 1; fi
	$(MKDIR) $(OUTPUT_DIR)
	$(UPDATE) public/ solutions/ tasks/ $(OUTPUT_DIR)
	# $(UPDATE) solutions/ $(OUTPUT_DIR)
	# $(UPDATE) tasks/ $(OUTPUT_DIR)
	$(MKDIR) $(OUTPUT_DIR)/judge/chroot/ $(OUTPUT_DIR)/judge/queue/
	$(UPDATE) judge/judge_machine judge/CTH judge/checkers/ $(OUTPUT_DIR)/judge/
	# chmod -R 0755 $(OUTPUT_DIR)
	./chmod-default $(OUTPUT_DIR)
	chmod 0755 $(OUTPUT_DIR)/judge/CTH $(OUTPUT_DIR)/judge/judge_machine
	chown -R www-data:www-data $(OUTPUT_DIR)
	grep 'ALL ALL = (root) NOPASSWD: $(OUTPUT_DIR)judge/judge_machine' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(OUTPUT_DIR)judge/judge_machine >> /etc/sudoers; fi
	printf "<VirtualHost 127.2.2.2:80>\n	ServerAdmin webmaster@sim.localhost\n	ServerName sim.localhost\n\n	DocumentRoot %s\n	<Directory />\n		Options FollowSymLinks\n		AllowOverride All\n	</Directory>\n	<Directory %s>\n		Options Indexes FollowSymLinks MultiViews\n		AllowOverride All\n		Order allow,deny\n		allow from all\n		Require all granted\n	</Directory>\n	CustomLog \$${APACHE_LOG_DIR}/access.log combined\n</VirtualHost>" $(OUTPUT_DIR)/public/ $(OUTPUT_DIR)/public/ > /etc/apache2/sites-available/sim.conf
	a2ensite sim
	echo service apache2 reload
	@sh -c 'service apache2 reload; exit 0'

clean:
	$(RM) *.o chmod-default
	make clean -C judge $(MFLAGS)

dist-clean: clean
	make dist-clean -C judge $(MFLAGS)
	$(RM) sim.tar.gz