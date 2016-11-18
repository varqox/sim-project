include ../Makefile.config

EXTRA_CXX_FLAGS += -I '$(shell pwd)/include' -isystem '$(shell pwd)/include/others'
EXTRA_LD_FLAGS += -L '$(shell pwd)/lib' -L '$(shell pwd)/lib/others'

.PHONY: all
all: lib killinstc setup-installation web-interface Judge_machine

$(eval $(call dependencies))

.PHONY: Judge_machine web-interface lib
Judge_machine web-interface: lib
Judge_machine web-interface lib:
	$(Q)$(MAKE) -C $@/

killinstc: killinstc.o lib/simlib.a
	$(LINK)

setup-installation: setup_installation.o lib/sim.a
	$(LINK) -lmysqlcppconn

lib/simlib.a lib/sim.a: lib;

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps killinstc sim-server setup-installation
	$(Q)$(MAKE) clean -C lib/
	$(Q)$(MAKE) clean -C Judge_machine/
	$(Q)$(MAKE) clean -C web-interface/
