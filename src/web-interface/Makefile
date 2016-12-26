include ../../Makefile.config

.PHONY: all
all: ../sim-server ../sim-server2

$(eval $(call dependencies))

../sim-server: server.o connection.o http_request.o http_response.o form_validator.o
../sim-server: session.o template.o errors.o user.o problemset.o jobs.o
../sim-server: contest.o contest_utilities.o submissions.o files.o sim_base.o sim.o
../sim-server: ../lib/sim.a
../sim-server:
	$(LINK) -lmysqlcppconn -pthread

../sim-server2: server2.o connection.o http_request.o http_response.o form_validator.o
../sim-server2: session.o template.o errors.o user.o problemset.o jobs.o
../sim-server2: contest.o contest_utilities.o submissions.o files.o sim_base.o sim.o
../sim-server2: ../lib/sim.a
../sim-server2:
	$(LINK) -lmysqlcppconn -pthread

# Technique used to force browsers to always keep updated version of the files below
template.o: EXTRA_CXX_FLAGS += '-DSTYLES_CSS_HASH="$(shell printf '%x' $$(stat -c '%Y' ../public/kit/styles.css))"'
template.o: EXTRA_CXX_FLAGS += '-DJQUERY_JS_HASH="$(shell printf '%x' $$(stat -c '%Y' ../public/kit/jquery.js))"'
template.o: EXTRA_CXX_FLAGS += '-DSCRIPTS_JS_HASH="$(shell printf '%x' $$(stat -c '%Y' ../public/kit/scripts.js))"'

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../sim-server
