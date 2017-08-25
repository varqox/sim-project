include ../Makefile.config

.PHONY: all
all: lib killinstc setup-installation backup web-interface Job-server

$(eval $(call dependencies))

.PHONY: Job-server web-interface lib
Job-server web-interface: lib
Job-server web-interface lib:
	$(Q)$(MAKE) -C $@/

killinstc: killinstc.o lib/simlib.a
	$(LINK) -lsupc++

setup-installation: setup_installation.o lib/sim.a lib/sqlite3.a
	$(LINK) -lsupc++


backup: backup.o lib/sim.a lib/sqlite3.a
	$(LINK) -lsupc++ -lrt

lib/simlib.a lib/sim.a lib/sqlite3.a: lib;

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps killinstc sim-server setup-installation backup
	$(Q)$(MAKE) clean -C lib/
	$(Q)$(MAKE) clean -C Job-server/
	$(Q)$(MAKE) clean -C web-interface/
