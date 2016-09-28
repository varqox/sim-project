include ../Makefile.config

.PHONY: all
all: ../simlib.a

$(eval $(call dependencies))

# Because source file is generated we may not generate dependencies
# automatically (the file may not be already created), that is why we have to
# do it manually
syscall_name.o: syscall_name.cc.deps

syscall_name.cc: Makefile
	$(Q)echo "// File generated automatically, so changes will not be permanent" > $@
	$(Q)echo "#include \"../include/syscall_name.h\"" >> $@
	$(Q)echo "" >> $@
	# 32 bit
	$(Q)echo "SyscallNameSet x86_syscall_name {" >> $@
	$(Q)grep __NR_ /usr/include/asm/unistd_32.h | sed 's/#define\s*__NR_//' | \
		awk '{ printf "\t{"; if (2<=NF) printf $$2; for (i=3; i<=NF; ++i) printf " " $$i; print ", \"" $$1 "\"}," }' >> $@
	$(Q)echo "};" >> $@
	$(Q)echo "" >> $@
	# 64 bit
	$(Q)echo "SyscallNameSet x86_64_syscall_name {" >> $@
	$(Q)grep __NR_ /usr/include/asm/unistd_64.h | sed 's/#define\s*__NR_//' | \
		awk '{ printf "\t{"; if (2<=NF) printf $$2; for (i=3; i<=NF; ++i) printf " " $$i; print ", \"" $$1 "\"}," }' >> $@
	$(Q)echo "};" >> $@

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
../simlib.a: spawner.o sandbox.o syscall_name.o
../simlib.a: random.o ncg.o sha.o sha3.o
../simlib.a: sim/sim.o http/http.o
../simlib.a:
	$(Q)$(call P,AR,$@)$(AR) cr $@ $?

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
	$(Q)$(MAKE) clean -C sim/
	$(Q)$(MAKE) clean -C http/
