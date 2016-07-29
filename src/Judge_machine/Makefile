include ../../Makefile.config

.PHONY: all
all: ../judge-machine

$(eval $(call dependencies))

../judge-machine: main.o judge.o
../judge-machine: ../lib/sim.a
../judge-machine:
	$(LINK) -lmysqlcppconn

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

../simlib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../simlib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../judge-machine
