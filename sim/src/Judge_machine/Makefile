include ../../Makefile.config

.PHONY: all
all: ../judge-machine

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../judge-machine: main.o judge.o
../judge-machine: ../lib/libsim.a
../judge-machine:
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@ -lmysqlcppconn

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../judge-machine
