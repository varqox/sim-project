include ../../Makefile.config

.PHONY: all
all: sim.o

$(eval $(call dependencies))

# Because source file is generated we may not generate dependencies
# automatically (the file may not be already created), that is why we have to
# do it manually
default_checker_dump.o: default_checker_dump.c.deps

default_checker_dump.c: default_checker.c Makefile
	$(Q)$(call P,GEN,$@) xxd -i $< > $@

sim.o: simfile.o compile.o checker.o judge_worker.o
sim.o: conver.o default_checker_dump.o
sim.o:
	$(Q)$(call P,LD,$@)$(LD) -r $^ -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
