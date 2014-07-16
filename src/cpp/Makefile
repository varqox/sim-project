
PHONY := all
all: chmod-default judge_machine CTH

PHONY += judge_machine
judge_machine:
	$(Q)$(MAKE) -C Judge_machine dep-s
	$(Q)$(MAKE) -C Judge_machine

PHONY += CTH
CTH:
	$(Q)$(MAKE) -C CppToHTML dep-s
	$(Q)$(MAKE) -C CppToHTML

chmod-default: chmod-default.cpp
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

PHONY += mrproper
mrproper:
	$(Q)$(RM) chmod-default judge_machine CTH

PHONY += dep-s
dep-s:

.PHONY: $(PHONY)
