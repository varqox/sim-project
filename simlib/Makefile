include Makefile.config

DESTDIR = build

.PHONY: build
build:
ifeq ($(MAKELEVEL), 0)
	@echo "DEBUG: $(DEBUG)"
	@echo "CC -> $(CC)"
	@echo "CXX -> $(CXX)"
endif
	$(Q)$(MAKE) -C src/
ifeq ($(MAKELEVEL), 0)
	@echo "\033[32mBuild finished\033[0m"
endif

.PHONY: test
test: build
	$(Q)$(MAKE) -C test/

.PHONY: clean
clean:
	$(Q)$(RM) simlib.a
	$(Q)$(MAKE) clean -C src/
	$(Q)$(MAKE) clean -C test/

.PHONY: help
help:
	@echo "Nothing is here yet..."
