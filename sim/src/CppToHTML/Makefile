include ../../Makefile.config

.PHONY: all
all: ../CTH

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../CTH: main.o coloring.o aho.o
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../CTH
