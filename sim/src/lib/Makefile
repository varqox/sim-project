include ../../Makefile.config

.PHONY: all
all: sim.a

$(eval $(call dependencies))

sim.a: db.o cpp_syntax_highlighter.o ../simlib/simlib.a
	$(RM) $@ # Delete archive because if it has already existed then it will
	         # contain simlib, but ADDLIB does not replace existing ones so
	         # after appending simlib.a it will result in duplications
	$(Q)$(call P,AR,$@)$(AR) cr $@ \
		$(filter-out $(lastword $^), $^) # Do not append simlib.a to archive -
		                                 # it has to be done in the other way
	echo "OPEN sim.a\nADDLIB ../simlib/simlib.a\nSAVE\nEND" | $(AR) -M

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps sim.a
