include ../../Makefile.config

.PHONY: all
all: ../judge-machine

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../judge-machine: main.o judge.o
../judge-machine: ../lib/sim.a ../simlib/simlib.a
../judge-machine:
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@ -lmysqlcppconn-static -lmysqlclient -lpthread -ldl

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
