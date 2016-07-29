include Makefile.config

.PHONY: all
all: build-info
	@$(MAKE) src googletest
ifeq ($(MAKELEVEL), 0)
	@echo "\033[32mBuild finished\033[0m"
endif

.PHONY: build-info
build-info:
ifeq ($(MAKELEVEL), 0)
	@echo "DEBUG: $(DEBUG)"
	@echo "CC -> $(CC)"
	@echo "CXX -> $(CXX)"
endif

.PHONY: googletest src
googletest src: build-info
	$(Q)$(MAKE) -C $@

.PHONY: test
test: src googletest
	$(Q)$(MAKE) -C test/

.PHONY: clean
clean:
	$(Q)$(RM) simlib.a
	$(Q)$(MAKE) clean -C googletest/
	$(Q)$(MAKE) clean -C src/
	$(Q)$(MAKE) clean -C test/

.PHONY: help
help:
	@echo "Nothing is here yet..."
