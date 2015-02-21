include ../../Makefile.config

.PHONY: all
all: ../sim-server

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cc$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../sim-server: core.o connection.o http_request.o http_response.o ../lib/libsim.a
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@ -lpthread

../lib/%:
ifeq ($(MAKELEVEL), 0)
	$(Q)$(MAKE) -C ../lib/
endif

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../sim-server
