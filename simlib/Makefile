include Makefile.config

.PHONY: all
all: build-info
	@$(MAKE) src googletest
ifeq ($(MAKELEVEL), 0)
	@echo -e "\033[32mBuild finished\033[0m"
endif

.PHONY: build-info
build-info:
ifeq ($(MAKELEVEL), 0)
	@echo -e "DEBUG: $(DEBUG)"
	@echo -e "CC -> $(CC)"
	@echo -e "CXX -> $(CXX)"
endif

.PHONY: src
src: build-info
	$(Q)$(MAKE) -C $@

.PHONY: test
test: src googletest
	$(Q)$(MAKE) -C test/

.PHONY: build-test
build-test: src googletest
	$(Q)$(MAKE) -C test/ build

.PHONY: googletest
googletest: build-info gtest_main.a

gtest_main.a googletest/%.deps: EXTRA_CXX_FLAGS += -isystem '$(shell pwd)/googletest/googletest/include' -I googletest/googletest/ -pthread

gtest_main.a: googletest/googletest/src/gtest-all.o googletest/googletest/src/gtest_main.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $?

# Build dependencies manually because only that two are needed
$(eval $(call dependencies_list, googletest/googletest/src/gtest-all.cc googletest/googletest/src/gtest_main.cc))

.PHONY: clean
clean:
	$(Q)$(RM) simlib.a gtest_main.a googletest/googletest/src/*.o src/*.deps
	$(Q)$(MAKE) clean -C src/
	$(Q)$(MAKE) clean -C test/

.PHONY: help
help:
	@echo -e "Nothing is here yet..."
