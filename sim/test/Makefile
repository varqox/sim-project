include ../Makefile.config

.PHONY: all
all: cpp_syntax_highlighter

.PHONY: cpp_syntax_highlighter
cpp_syntax_highlighter:
	$(Q)$(MAKE) -C $@/

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps
	$(Q)$(MAKE) clean -C cpp_syntax_highlighter/
