include ../../Makefile.config

.PHONY: all
all: ../sim-server ../sim-server2

 $(eval $(call dependencies))

../sim-server: server.o connection.o http_request.o http_response.o form_validator.o
../sim-server: session.o template.o errors.o user.o
../sim-server: contest.o contest_utilities.o submissions.o files.o sim_base.o sim.o
../sim-server: ../lib/sim.a
../sim-server:
	$(LINK) -lmysqlcppconn -pthread

../sim-server2: server2.o connection.o http_request.o http_response.o form_validator.o
../sim-server2: session.o template.o errors.o user.o
../sim-server2: contest.o contest_utilities.o submissions.o files.o sim_base.o sim.o
../sim-server2: ../lib/sim.a
../sim-server2:
	$(LINK) -lmysqlcppconn -pthread

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

../simlib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../simlib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../sim-server
