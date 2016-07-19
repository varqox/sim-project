include ../../Makefile.config

.PHONY: all
all: sim.a

$(eval $(call dependencies))

sim.a: db.o cpp_syntax_highlighter.o ../simlib/simlib.a
	$(Q)$(call P,AR,$@)$(AR) cr $@ $^
	echo "OPEN sim.a\nADDLIB ../simlib/simlib.a\nSAVE\nEND" | ar -M

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps sim.a
