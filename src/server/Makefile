include ../../Makefile.config

.PHONY: all
all: ../sim-server

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../sim-server: core.o connection.o http_request.o http_response.o sim_main.o sim_template.o sim_session.o sim_errors.o sim_user.o ../lib/libsim.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@ -lpthread -lmysqlcppconn

../lib/%: FORCE
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../sim-server
