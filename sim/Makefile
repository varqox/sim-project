# Installation settings
INSTALL_DIR = $(shell pwd)/build/

# Rest
.PHONY: all install clean dist-clean package help

all:
	make -C src/ $(MFLAGS)

install: all
	make install -C src/ INSTALL_DIR\=$(INSTALL_DIR) $(MFLAGS)

clean:
	make clean -C src/ $(MFLAGS)

dist-clean: clean
	make dist-clean -C src/ $(MFLAGS)

package: dist-clean
	tar --exclude=build -czf sim.tar.gz *

help:
	echo "Nothing is here yet..."
