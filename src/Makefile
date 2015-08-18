include ../Makefile.config

.PHONY: all
all: chmod-default Conver CppToHTML Judge_machine server lib setup-install

.PHONY: Conver CppToHTML Judge_machine server lib simlib
Conver Judge_machine server lib: simlib
Judge_machine server: lib
Conver CppToHTML Judge_machine server lib simlib:
	$(Q)$(MAKE) -C $@/

chmod-default: Algorithms/other/chmod-default.cpp
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@

setup-install: setup_install.cc lib/sim.a simlib/simlib.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@ -lmysqlcppconn

lib/sim.a: lib
	@

simlib/simlib.a: simlib
	@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps chmod-default sim-server setup-install
	$(Q)$(MAKE) clean -C lib/
	$(Q)$(MAKE) clean -C simlib/
	$(Q)$(MAKE) clean -C Conver/
	$(Q)$(MAKE) clean -C CppToHTML/
	$(Q)$(MAKE) clean -C Judge_machine/
	$(Q)$(MAKE) clean -C server/
