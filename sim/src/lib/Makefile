include ../../Makefile.config

.PHONY: all
all: sim.a

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

sim.a: db.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $^

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps sim.a
