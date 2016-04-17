include ../Makefile.config

.PHONY: all
all: ../simlib.a

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

.PHONY: sim http
sim http:
	$(Q)$(MAKE) -C $@/

sim/sim.o: sim
	@

http/http.o: http
	@

../simlib.a: string.o aho_corasick.o
../simlib.a: time.o filesystem.o STD_SHA_256.o process.o sandbox.o
../simlib.a: sandbox_checker_callback.o compile.o config_file.o
../simlib.a: logger.o
../simlib.a: sim/sim.o http/http.o
../simlib.a: random.o ncg.o sha.o sha3.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $^

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
	$(Q)$(MAKE) clean -C sim/
	$(Q)$(MAKE) clean -C http/
