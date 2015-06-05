include ../../Makefile.config

.PHONY: all
all: libsim.a

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

libsim.a: string.o time.o db.o filesystem.o STD_SHA_256.o process.o sandbox.o
libsim.a: sandbox_checker_callback.o compile.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $^

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps libsim.a
