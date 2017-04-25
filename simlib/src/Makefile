include ../Makefile.config

.PHONY: all
all: ../simlib.a

$(eval $(call dependencies))

# Because source file is generated we may not generate dependencies
# automatically (the file may not be already created), that is why we have to
# do it manually
syscall_name_32.o: syscall_name_32.cc.deps
syscall_name_64.o: syscall_name_64.cc.deps

syscall_name_32.cc: UNISTD_32 = /usr/include/asm/unistd_32.h
syscall_name_32.cc: Makefile $(UNISTD_32)
	$(GEN)
	$(Q)echo "// WARNING: file generated automatically; changes will not be permanent." > $@
	$(Q)echo "#include \"../include/syscall_name.h\"" >> $@
	$(Q)echo "#include \"$(UNISTD_32)\"" >> $@
	$(Q)echo "" >> $@
	# 32 bit
	$(Q) [ -f $(UNISTD_32) ] || \
		{ echo "Error: $(UNISTD_32): No such file or directory"; $(RM) $@; exit 1; }
	$(Q)echo "SyscallNameSet x86_syscall_name {" >> $@
	$(Q)grep -E "^\s*#\s*define\s*__NR_" $(UNISTD_32) | \
		sed 's/#define\s*__NR_//' | \
		awk '{ printf "\t{"; if (2<=NF) printf $$2; for (i=3; i<=NF; ++i) printf " " $$i; print ", \"" $$1 "\"}," }' >> $@
	$(Q)echo "};" >> $@

syscall_name_64.cc: UNISTD_64 = /usr/include/asm/unistd_64.h
syscall_name_64.cc: Makefile $(UNISTD_64)
	$(GEN)
	$(Q)echo "// WARNING: file generated automatically; changes will not be permanent." > $@
	$(Q)echo "#include \"../include/syscall_name.h\"" >> $@
	$(Q)echo "#include \"$(UNISTD_64)\"" >> $@
	$(Q)echo "" >> $@
	# 64 bit
	$(Q)echo "SyscallNameSet x86_64_syscall_name {" >> $@
	$(Q) [ -f $(UNISTD_64) ] || \
		{ echo "Error: $(UNISTD_64): No such file or directory"; $(RM) $@; exit 1; }
	$(Q)grep -E "^\s*#\s*define\s*__NR_" $(UNISTD_64) | \
		sed 's/#define\s*__NR_//' | \
		awk '{ printf "\t{"; if (2<=NF) printf $$2; for (i=3; i<=NF; ++i) printf " " $$i; print ", \"" $$1 "\"}," }' >> $@
	$(Q)echo "};" >> $@

.PHONY: sim http
sim http:
	$(Q)$(MAKE) -C $@/

sim/sim.o: sim
	@

http/http.o: http
	@

../simlib.a: string.o debug.o aho_corasick.o
../simlib.a: config_file.o logger.o
../simlib.a: time.o filesystem.o process.o
../simlib.a: spawner.o sandbox.o syscall_name_32.o syscall_name_64.o
../simlib.a: random.o sha.o sha3.o
../simlib.a: mysql.o
../simlib.a: sim/sim.o http/http.o
../simlib.a:
	$(Q)$(call P,AR,$@)$(AR) cr $@ $?

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps syscall_name_32.cc syscall_name_64.cc
	$(Q)$(MAKE) clean -C sim/
	$(Q)$(MAKE) clean -C http/
