include Makefile.config

DESTDIR = build

.PHONY: all
all:
	@printf "CC -> $(CC)\nCXX -> $(CXX)\n"
	$(Q)$(MAKE) -C src/
	@printf "\033[;32mBuild finished\033[0m\n"

.PHONY: clean
clean:
	$(Q)$(RM) simlib.a
	$(Q)$(MAKE) clean -C src/

.PHONY: help
help:
	@echo "Nothing is here yet..."
