include ../Makefile.config

.PHONY: all
all: chmod-default Conver CppToHTML Judge_machine server lib setup_install

.PHONY: Conver CppToHTML Judge_machine server lib
Conver CppToHTML Judge_machine server: lib
Conver CppToHTML Judge_machine server lib:
	$(Q)$(MAKE) -C $@/

chmod-default: Algorithms/other/chmod-default.cpp
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

setup_install: setup_install.cpp lib/libsim.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@ -lmysqlcppconn

lib/libsim.a: lib

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps chmod-default sim-server
	$(Q)$(MAKE) clean -C lib/
	$(Q)$(MAKE) clean -C Conver/
	$(Q)$(MAKE) clean -C CppToHTML/
	$(Q)$(MAKE) clean -C Judge_machine/
	$(Q)$(MAKE) clean -C server/
