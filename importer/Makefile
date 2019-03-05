include ../Makefile.config

MYSQL_CONFIG := $(shell which mariadb_config 2> /dev/null || which mysql_config 2> /dev/null)

IMPORTER_CXX_FLAGS := -I '$(CURDIR)/../src/include' -isystem '$(CURDIR)/../src/include/others' $(shell $(MYSQL_CONFIG) --include)
IMPORTER_LD_FLAGS := -L '$(CURDIR)/../src/lib' -L '$(CURDIR)/../src/lib/others' $(shell $(MYSQL_CONFIG) --libs)

all: importer

IMPORTER_SRCS := \
	importer.cc

$(eval $(call load_dependencies, $(IMPORTER_SRCS)))
IMPORTER_OBJS := $(call SRCS_TO_OBJS, $(IMPORTER_SRCS))

importer: $(IMPORTER_OBJS) ../src/lib/sim.a ../src/lib/simlib/simlib.a
	$(LINK) -lsupc++ -lrt -lzip

../src/lib/sim.a ../src/lib/simlib/simlib.a:
	$(MAKE) -C .. src/lib/sim.a src/lib/simlib/simlib.a

IMPORTER_EXECS := importer

$(IMPORTER_OBJS): override EXTRA_CXX_FLAGS += $(IMPORTER_CXX_FLAGS)
$(IMPORTER_EXECS): private override EXTRA_LD_FLAGS += $(IMPORTER_LD_FLAGS)

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps importer
