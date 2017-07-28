include ../../Makefile.config

.PHONY: all
all: sim.a simlib.a gtest_main.a sqlite3.a

$(eval $(call dependencies))

.PHONY: simlib
simlib: override EXTRA_CXX_FLAGS = $(EXTERNAL_EXTRA_LD_FLAGS)
simlib: override EXTRA_LD_FLAGS = $(EXTERNAL_EXTRA_CXX_FLAGS)
simlib: FORCE
	$(Q)$(MAKE) -C simlib/

# Exclude additional compilation flags to be used on simlib
simlib.a: simlib
	$(Q)ln -s simlib/$@ $@ 2>&1 | true

gtest_main.a: simlib
	$(Q)ln -s simlib/$@ $@ 2>&1 | true

sim.a: mysql.o cpp_syntax_highlighter.o simlib.a
	$(RM) $@ # Delete archive because if it has already existed then it will
	         # contain simlib, but ADDLIB does not replace existing ones so
	         # after appending simlib.a it will result in duplications
	$(Q)$(call P,AR,$@)$(AR) cr $@ \
		$(filter-out $(lastword $^), $^) # Do not append simlib.a to archive -
		                                 # it has to be done in the other way
	printf "OPEN sim.a\nADDLIB simlib.a\nSAVE\nEND" | $(AR) -M

sqlite3.c: sqlite/sqlite3.c

sqlite3.a: override EXTRA_C_FLAGS += -w -DSQLITE_ENABLE_FTS5 -DSQLITE_THREADSAFE=2
sqlite3.a: sqlite3.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $?

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps *.a
	$(Q)$(MAKE) clean -C simlib/
