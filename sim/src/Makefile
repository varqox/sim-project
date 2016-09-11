include ../Makefile.config

OLD_EXTRA_CXX_FLAGS := $(EXTRA_CXX_FLAGS)
OLD_EXTRA_LD_FLAGS := $(EXTRA_LD_FLAGS)
EXTRA_CXX_FLAGS += -I '$(shell pwd)/include' -isystem '$(shell pwd)/include/others'
EXTRA_LD_FLAGS += -L '$(shell pwd)/lib' -L '$(shell pwd)/lib/others'

.PHONY: all
all: killinstc Judge_machine server lib setup-installation simlib

$(eval $(call dependencies))

# Disable flags to be run on simlib
simlib: override EXTRA_CXX_FLAGS = $(OLD_EXTRA_LD_FLAGS)
simlib: override EXTRA_LD_FLAGS = $(OLD_EXTRA_CXX_FLAGS)
simlib:
	$(Q)$(MAKE) -C $@/

.PHONY: Judge_machine server lib simlib
Judge_machine server lib: simlib
Judge_machine server: lib
Judge_machine server lib:
	$(Q)$(MAKE) -C $@/

killinstc: killinstc.o simlib/simlib.a
	$(LINK)

setup-installation: setup_installation.o lib/sim.a
	$(LINK) -lmysqlcppconn

simlib/simlib.a: simlib;
lib/sim.a: lib;

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps killinstc sim-server setup-installation
	$(Q)$(MAKE) clean -C lib/
	$(Q)$(MAKE) clean -C simlib/
	$(Q)$(MAKE) clean -C Judge_machine/
	$(Q)$(MAKE) clean -C server/
