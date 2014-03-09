export
# Installation settings
# INSTALL_DIR is set in 'src/Makefile'
INSTALL_DIR =

# Rest
.PHONY: all install clean mrproper package help

all:
	make -C src/

install: all
	make install -C src/

clean:
	make clean -C src/

mrproper: clean
	make mrproper -C src/

package: mrproper
	tar --exclude=build -czf sim.tar.gz *

help:
	echo "Nothing is here yet..."
