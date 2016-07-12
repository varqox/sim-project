include ../../Makefile.config

EXTRA_OPTIONS += -I '$(ROOT)/src/include' -isystem '$(ROOT)/src/include/others'

.PHONY: all
all: check
	@echo $(notdir $(shell pwd)):
	./check

check: check.o ../../src/lib/sim.a ../../src/simlib/simlib.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) $(LFLAGS) -o $@

../../src/lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../../src/lib/
endif

../../src/simlib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../../src/simlib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps tests/*.ans check
