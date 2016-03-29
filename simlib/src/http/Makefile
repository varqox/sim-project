include ../../Makefile.config

.PHONY: all
all: http.o

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

http.o: server.o
	$(Q)$(call P,LD,$@)$(LD) -r $^ -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps

