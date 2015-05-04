include ../../Makefile.config

.PHONY: all
all: ../conver

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
-include default_checker_dump.c.deps
endif

../conver: conver.o validate_package.o convert_package.o set_limits.o
../conver: default_checker_dump.o
../conver: ../lib/libsim.a
../conver:
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

default_checker_dump.c: default_checker.c
	$(Q)$(call P,GEN,$@)xxd -i $< > $@

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../conver default_checker_dump.c
