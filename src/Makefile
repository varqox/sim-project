# Compile commands
CC = gcc
CXX = g++
LINK = g++
CFLAGS = -O3
CXXFLAGS = -O3 -c
LFLAGS = -s -O3

# Shell commands
MV = mv -f
CP = cp -f -r
UPDATE = $(CP) -u
RM = rm -f
MKDIR = mkdir -p

# Installation settings
INSTALL_DIR =
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
.PHONY: all install clean force-clean package help

all:
	make -C judge $(MFLAGS)

install: all
	# Echo log
	@printf "\033[01;34m$(INSTALL_DIR)\033[00m\n"
	@printf "\033[01;34m$(OUTPUT_DIR)\033[00m\n"
	# Installation
	@if test `whoami` != "root"; then echo You have to run it as root!; exit 1; fi
	$(MKDIR) $(OUTPUT_DIR)
	$(UPDATE) files/ $(OUTPUT_DIR)
	$(UPDATE) kit/ $(OUTPUT_DIR)
	$(UPDATE) reports/ $(OUTPUT_DIR)
	$(UPDATE) solutions/ $(OUTPUT_DIR)
	$(UPDATE) tasks/ $(OUTPUT_DIR)
	$(MKDIR) $(OUTPUT_DIR)/judge/chroot/ $(OUTPUT_DIR)/judge/queue/ $(OUTPUT_DIR)/judge/checkers/
	$(UPDATE) judge/judge_machine judge/.htaccess judge/CTH judge/checkers $(OUTPUT_DIR)/judge/
	$(CP) *.php $(OUTPUT_DIR)
	chmod -R 0777 $(OUTPUT_DIR)
	chmod -R 0755 $(OUTPUT_DIR)/judge/chroot $(OUTPUT_DIR)/judge/judge_machine  $(OUTPUT_DIR)/kit/
	grep 'ALL ALL = (root) NOPASSWD: $(OUTPUT_DIR)judge/judge_machine' /etc/sudoers > /dev/null; if test $$? != 0; then printf "ALL ALL = (root) NOPASSWD: %s\n" $(OUTPUT_DIR)judge/judge_machine >> /etc/sudoers; fi
	printf "<VirtualHost 127.2.2.2:80>\n	ServerAdmin webmaster@sim.localhost\n	ServerName sim.localhost\n\n	DocumentRoot %s/\n	<Directory />\n		Options FollowSymLinks\n		AllowOverride All\n	</Directory>\n	<Directory %s/>\n		Options Indexes FollowSymLinks MultiViews\n		AllowOverride All\n		Order allow,deny\n		allow from all\n		Require all granted\n	</Directory>\n	CustomLog \$${APACHE_LOG_DIR}/access.log combined\n</VirtualHost>" $(OUTPUT_DIR) $(OUTPUT_DIR) > /etc/apache2/sites-available/sim.conf
	a2ensite sim
	echo service apache2 reload
	@sh -c 'service apache2 reload; exit 0'

clean:
	$(RM) *.o
	make clean -C judge $(MFLAGS)

force-clean: clean
	make force-clean -C judge $(MFLAGS)
	$(RM) sim.tar.gz