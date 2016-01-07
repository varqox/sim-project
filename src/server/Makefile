include ../../Makefile.config

.PHONY: all
all: ../sim-server ../sim-server2

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../sim-server: server.o connection.o http_request.o http_response.o form_validator.o
../sim-server: sim.o sim_template.o sim_errors.o
../sim-server: sim_session.o sim_user.o sim_contest.o sim_contest_utility.o
../sim-server: ../lib/sim.a ../simlib/simlib.a
../sim-server:
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@ -lpthread -lmysqlcppconn

../sim-server2: server2.o connection.o http_request.o http_response.o form_validator.o
../sim-server2: sim.o sim_template.o sim_errors.o
../sim-server2: sim_session.o sim_user.o sim_contest.o sim_contest_utility.o
../sim-server2: ../lib/sim.a ../simlib/simlib.a
../sim-server2:
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(CXXFLAGS) -o $@ -lpthread -lmysqlcppconn

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
