include ../Makefile.config

.PHONY: all
all: ../simlib.a

$(eval $(call dependencies))

.PHONY: sim http
sim http:
	$(Q)$(MAKE) -C $@/

sim/sim.o: sim
	@

http/http.o: http
	@

../simlib.a: string.o aho_corasick.o
../simlib.a: config_file.o logger.o
../simlib.a: time.o filesystem.o process.o
../simlib.a: spawner.o sandbox.o compile.o
../simlib.a: sim/sim.o http/http.o
../simlib.a: random.o ncg.o sha.o sha3.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $?

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
	$(Q)$(MAKE) clean -C sim/
	$(Q)$(MAKE) clean -C http/
