include ../Makefile.config

.PHONY: all
all: sip gen-line

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

.PHONY: simlib
simlib:
	$(Q)$(MAKE) -C $@/

sip: sip.o commands.o default_checker_dump.o
sip: simlib/simlib.a
sip:
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@

gen-line: gen-line.o simlib/simlib.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@

default_checker_dump.c: default_checker.c
	$(Q)$(call P,GEN,$@)xxd -i $< > $@

simlib/simlib.a: simlib
	@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps sip gen-line
	$(Q)$(MAKE) clean -C simlib/
