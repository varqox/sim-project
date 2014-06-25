
PHONY := all
all: ../CTH

../CTH: main.o coloring.o aho.o
	$(Q)$(P_LINK)$(LINK) $^ $(LFLAGS) -o $@

PHONY += dep-s
dep-s: $(shell ls | egrep -i ".c$$|.cpp$$" | sed "s/$$/.dep-s/")

.PHONY: $(PHONY)

include $(shell ls | grep ".dep-s$$")

%.o: %.c
	$(Q)$(call P,CC,$<.dep-s)$(CC) -MM $< -o $<.dep-s
	$(Q)$(call P,CC,$@)$(CC) $< $(CXXFLAGS) -o $@

%.o: %.cpp
	$(Q)$(call P,CC,$<.dep-s)$(CC) -MM $< -o $<.dep-s
	$(Q)$(call P,CXX,$@)$(CXX) $< $(CXXFLAGS) -o $@

%.dep-s: %
	$(Q)$(call P,CC,$@)$(CC) -MM $< -o $@

