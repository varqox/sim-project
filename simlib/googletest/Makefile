include ../Makefile.config

EXTRA_CXX_FLAGS += -isystem '$(shell pwd)/include' -I . -pthread

../gtest_main.a: src/gtest-all.o src/gtest_main.o
	$(Q)$(call P,AR,$@)$(AR) cr $@ $?

# Build dependencies manually because only that two are needed
$(eval $(call dependencies_list, src/gtest-all.cc src/gtest_main.cc))

.PHONY: clean
clean:
	$(Q)$(RM) src/*.o src/*.deps ../gtest_main.a
