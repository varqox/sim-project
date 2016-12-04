include ../Makefile.config

.PHONY: run-all
run-all:
	$(Q)$(MAKE) -C cpp_syntax_highlighter/

.PHONY: build-all
build-all:
	$(Q)$(MAKE) -C cpp_syntax_highlighter/ build

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
	$(Q)$(MAKE) clean -C cpp_syntax_highlighter/
