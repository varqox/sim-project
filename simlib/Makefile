include Makefile.config

DESTDIR = build

.PHONY: all
all:
ifeq ($(MAKELEVEL), 0)
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
endif
	$(Q)$(MAKE) -C src/
ifeq ($(MAKELEVEL), 0)
	@printf "\033[;32mBuild finished\033[0m\n"
endif

.PHONY: clean
clean:
	$(Q)$(RM) simlib.a
	$(Q)$(MAKE) clean -C src/

.PHONY: help
help:
	@echo "Nothing is here yet..."
