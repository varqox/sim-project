include ../../Makefile.config

.PHONY: all
all: sim.o

 $(eval $(call dependencies))

sim.o: simfile.o
	$(Q)$(call P,LD,$@)$(LD) -r $^ -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
