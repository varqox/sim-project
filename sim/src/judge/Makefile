
PHONY := all
all: judge_machine CTH

PHONY += judge_machine
judge_machine:
	$(Q)$(MAKE) -C Judge_machine dep-s
	$(Q)$(MAKE) -C Judge_machine

PHONY += CTH
CTH:
	$(Q)$(MAKE) -C CppToHTML dep-s
	$(Q)$(MAKE) -C CppToHTML

PHONY += mrproper
mrproper:
	$(Q)$(RM) judge_machine CTH test

PHONY += dep-s
dep-s:

.PHONY: $(PHONY)
