include ../Makefile.config

EXTRA_CXX_FLAGS += -isystem '$(shell pwd)/../googletest/googletest/include' -pthread

.PHONY: run
run: build
	./exec

.PHONY: build
build: exec

$(eval $(call dependencies))

exec: string.o config_file.o simfile.o filesystem.o
exec: ../gtest_main.a ../simlib.a
exec:
	$(LINK) -lrt -larchive

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps exec
