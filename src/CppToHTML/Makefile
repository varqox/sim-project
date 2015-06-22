include ../../Makefile.config

.PHONY: all
all: ../CTH

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../CTH: main.o coloring.o aho.o
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../CTH
