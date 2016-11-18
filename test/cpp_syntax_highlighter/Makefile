include ../../Makefile.config

EXTRA_CXX_FLAGS += -I '$(shell pwd)/../../src/include' -isystem '$(shell pwd)/../../src/include/others'

.PHONY: all
all: check
	@echo $(notdir $(shell pwd)):
	./check

$(eval $(call dependencies))

check: check.o ../../src/lib/sim.a
	$(LINK)

../../src/lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../../src/lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps tests/*.ans check
