include ../../Makefile.config

.PHONY: all
all: sim.a simlib.a

$(eval $(call dependencies))


# Exclude additional compilation flags to be used on simlib
simlib.a: override EXTRA_CXX_FLAGS = $(EXTERNAL_EXTRA_LD_FLAGS)
simlib.a: override EXTRA_LD_FLAGS = $(EXTERNAL_EXTRA_CXX_FLAGS)
simlib.a: FORCE
	$(Q)$(MAKE) -C simlib/
	$(Q)ln -s simlib/simlib.a $@ 2>&1 | true

sim.a: db.o cpp_syntax_highlighter.o simlib.a
	$(RM) $@ # Delete archive because if it has already existed then it will
	         # contain simlib, but ADDLIB does not replace existing ones so
	         # after appending simlib.a it will result in duplications
	$(Q)$(call P,AR,$@)$(AR) cr $@ \
		$(filter-out $(lastword $^), $^) # Do not append simlib.a to archive -
		                                 # it has to be done in the other way
	echo "OPEN sim.a\nADDLIB simlib.a\nSAVE\nEND" | $(AR) -M

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps sim.a simlib.a
	$(Q)$(MAKE) clean -C simlib/
