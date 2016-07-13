include ../Makefile.config

EXTRA_OPTIONS += -I '$(ROOT)/src/include' -isystem '$(ROOT)/src/include/others'
EXTRA_LD_OPTIONS += -L '$(ROOT)/src/lib' -L '$(ROOT)/src/lib/others'

.PHONY: all
all: killinstc Conver Judge_machine server lib setup-installation

.PHONY: Conver Judge_machine server lib simlib
Conver Judge_machine server lib: simlib
Judge_machine server: lib
Conver Judge_machine server lib simlib:
	$(Q)$(MAKE) -C $@/

killinstc: killinstc.cc simlib/simlib.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) $(LFLAGS) -o $@

setup-installation: setup_installation.cc lib/sim.a simlib/simlib.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) $(LFLAGS) -o $@ -lmysqlcppconn

lib/sim.a: lib
	@

simlib/simlib.a: simlib
	@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps killinstc sim-server setup-installation
	$(Q)$(MAKE) clean -C lib/
	$(Q)$(MAKE) clean -C simlib/
	$(Q)$(MAKE) clean -C Conver/
	$(Q)$(MAKE) clean -C Judge_machine/
	$(Q)$(MAKE) clean -C server/
