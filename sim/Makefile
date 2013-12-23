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
INSTALL_DIR = $(shell pwd)/build/

# Rest
.PHONY: all install clean force-clean package help

all:
	make -C src/ $(MFLAGS)

install: all
	make install -C src/ INSTALL_DIR\=$(INSTALL_DIR) $(MFLAGS)

clean:
	make clean -C src/ $(MFLAGS)

force-clean: clean
	make force-clean -C src/ $(MFLAGS)

package: force-clean
	tar --exclude=build -czf sim.tar.gz *

help:
	echo "Nothing is here yet..."
