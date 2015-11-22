include Makefile.config

.PHONY: all
all:
ifeq ($(MAKELEVEL), 0)
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
endif
	$(Q)$(MAKE) -C src/
ifeq ($(MAKELEVEL), 0)
	@printf "\033[;32mBuild finished\033[0m\n"
endif

.PHONY: install
install: all
	$(CP) src/sip /usr/local/bin

.PHONY: uninstall
uninstall:
	$(RM) /usr/local/bin/sip

.PHONY: clean
clean:
	$(Q)$(MAKE) clean -C src/

.PHONY: help
help:
	@echo "Nothing is here yet..."
