include ../../Makefile.config

.PHONY: all
all: ../job-server

$(eval $(call dependencies))

../job-server: main.o judge.o problem.o
../job-server: ../lib/sim.a ../lib/sqlite3.a
../job-server:
	$(LINK) -lsupc++ -lrt -larchive

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../job-server
