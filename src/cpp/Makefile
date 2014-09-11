
PHONY := all
all: chmod-default judge_machine CTH conver

PHONY += judge_machine
judge_machine:
	$(Q)$(MAKE) -C Judge_machine

PHONY += CTH
CTH:
	$(Q)$(MAKE) -C CppToHTML

PHONY += conver
conver:
	$(Q)$(MAKE) -C Conver

chmod-default: ../Algorithms/other/chmod-default.cpp
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

PHONY += mrproper
mrproper:
	$(Q)$(RM) chmod-default judge_machine CTH

.PHONY: $(PHONY)
