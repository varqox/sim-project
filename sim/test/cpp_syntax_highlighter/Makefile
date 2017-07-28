include ../../Makefile.config

.PHONY: run
run: build
	@echo $(notdir $(shell pwd)):
	./check

.PHONY: build
build: check

$(eval $(call dependencies))

check: check.o ../../src/lib/sim.a
	$(LINK) -lrt

../../src/lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../../src/lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps tests/*.ans check
