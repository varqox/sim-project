include ../../Makefile.config

.PHONY: all
all: libsim.a

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

libsim.a: string.o time.o db.o STD_SHA_256.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $^

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps libsim.a
