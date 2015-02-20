include ../Makefile.config

.PHONY: all
all: chmod-default sim-server Conver CppToHTML Judge_machine

.PHONY: Conver CppToHTML Judge_machine
Conver CppToHTML Judge_machine:
	$(Q)$(MAKE) -C $@/

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

sim-server: server.cpp
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@ -lpthread

chmod-default: Algorithms/other/chmod-default.cpp
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps chmod-default sim-server
	$(Q)$(MAKE) clean -C Conver/
	$(Q)$(MAKE) clean -C CppToHTML/
	$(Q)$(MAKE) clean -C Judge_machine/
