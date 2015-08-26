include ../Makefile.config

.PHONY: all
all: ../simlib.a

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../simlib.a: string.o time.o filesystem.o STD_SHA_256.o process.o sandbox.o
../simlib.a: sandbox_checker_callback.o compile.o sim_problem.o config_file.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $^

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
