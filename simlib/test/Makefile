include ../Makefile.config

EXTRA_CXX_FLAGS += -isystem '$(shell pwd)/../googletest/include' -pthread

.PHONY: run
run: exec
	./exec

$(eval $(call dependencies))

exec: config_file.o simfile.o
exec: ../gtest_main.a ../simlib.a
exec:
	$(LINK)

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps exec
