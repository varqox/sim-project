include ../Makefile.config

EXTRA_CXX_FLAGS += -I '$(shell pwd)/../src/include' -isystem '$(shell pwd)/../src/include/others' -pthread

importer: importer.o ../src/lib/sim.a ../src/lib/sqlite3.a
	$(LINK) -lmysqlcppconn -ldl -pthread

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
