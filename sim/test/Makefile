include ../Makefile.config

EXTRA_CXX_FLAGS += -I '$(shell pwd)/../src/include' -isystem '$(shell pwd)/../src/include/others' -pthread

.PHONY: run
run: cth-run exec-run

.PHONY: build
build: cth-build exec

$(eval $(call dependencies))

.PHONY: cth-run
cth-run:
	$(Q)$(MAKE) -C cpp_syntax_highlighter/

.PHONY: cth-build
cth-build:
	$(Q)$(MAKE) -C cpp_syntax_highlighter/ build

.PHONY: exec-run
exec-run: exec
	./exec

exec: jobs.o
exec: ../src/lib/sim.a ../src/lib/gtest_main.a
exec:
	$(LINK)

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
	$(Q)$(MAKE) clean -C cpp_syntax_highlighter/
