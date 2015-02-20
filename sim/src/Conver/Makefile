include ../../Makefile.config

.PHONY: all
all: ../conver

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../conver: main.o other_functions.o compile.o
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../conver
