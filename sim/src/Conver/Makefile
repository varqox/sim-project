include ../../Makefile.config

.PHONY: all
all: ../conver

 $(eval $(call dependencies))

../conver: conver.o validate_package.o convert_package.o set_limits.o
../conver: default_checker_dump.o
../conver: ../simlib/simlib.a
../conver:
	$(LINK)

# Because source file is generated - that is why we can generate dependencies
# now and why we have to do it manually
default_checker_dump.o: default_checker_dump.c.deps

default_checker_dump.c: default_checker.c
	$(Q)$(call P,GEN,$@)xxd -i $< > $@

../simlib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../simlib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../conver default_checker_dump.c
