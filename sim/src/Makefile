include ../Makefile.config

EXTRA_CXX_FLAGS += -I '$(shell pwd)/include' -isystem '$(shell pwd)/include/others'
EXTRA_LD_FLAGS += -L '$(shell pwd)/lib' -L '$(shell pwd)/lib/others'

.PHONY: all
all: lib killinstc setup-installation web-interface Job-server

$(eval $(call dependencies))

.PHONY: Job-server web-interface lib
Job-server web-interface: lib
Job-server web-interface lib:
	$(Q)$(MAKE) -C $@/

killinstc: killinstc.o lib/simlib.a
	$(LINK)

setup-installation: setup_installation.o lib/sim.a lib/sqlite3.a
	$(LINK) $(shell mysql_config --libs)

lib/simlib.a lib/sim.a lib/sqlite3.a: lib;

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps killinstc sim-server setup-installation
	$(Q)$(MAKE) clean -C lib/
	$(Q)$(MAKE) clean -C Job-server/
	$(Q)$(MAKE) clean -C web-interface/
