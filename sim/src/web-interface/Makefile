include ../../Makefile.config

.PHONY: all
all: ../sim-server # ../sim-server2

$(eval $(call dependencies))

../sim-server: server.o connection.o http_request.o http_response.o
../sim-server: sim.o session.o template.o
../sim-server: api.o jobs_api.o users_api.o submissions_api.o problems_api.o contests_api.o
../sim-server: users.o jobs.o problems.o submissions.o contests.o
../sim-server: ../lib/sim.a ../lib/sqlite3.a
../sim-server:
	$(LINK) -lsupc++ -lrt -larchive

# ../sim-server2: server2.o connection.o http_request.o http_response.o form_validator.o
# ../sim-server2: session.o template.o errors.o user.o problemset.o jobs.o
# ../sim-server2: contest.o contest_utilities.o submissions.o files.o sim_base.o sim.o
# ../sim-server2: ../lib/sim.a
# ../sim-server2:
	# $(LINK) -lmysqlcppconn -pthread -lsupc++

# Technique used to force browsers to always keep up-to-date version of the
# files below
template.o: EXTRA_CXX_FLAGS += '-DSTYLES_CSS_HASH="$(shell printf '%x' $$(stat -c '%Y' ../static/kit/styles.css))"'
template.o: EXTRA_CXX_FLAGS += '-DJQUERY_JS_HASH="$(shell printf '%x' $$(stat -c '%Y' ../static/kit/jquery.js))"'
template.o: EXTRA_CXX_FLAGS += '-DSCRIPTS_JS_HASH="$(shell printf '%x' $$(stat -c '%Y' ../static/kit/scripts.js))"'

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../sim-server
