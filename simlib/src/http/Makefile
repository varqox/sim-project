include ../../Makefile.config

.PHONY: all
all: http.o

$(eval $(call dependencies))

http.o: server.o
	$(Q)$(call P,LD,$@)$(LD) -r $^ -o $@

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps

