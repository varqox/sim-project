
PHONY := all
all: ../CTH

../CTH: main.o coloring.o aho.o
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@

.PHONY: $(PHONY)

-include $(shell ls | egrep ".c$$|.cpp$$" | sed "s/$$/.dep-s/")

%.o: %.c
	$(Q)$(call P,CC,$@)$(CC) $< $(CXXFLAGS) -o $@

%.o: %.cpp
	$(Q)$(call P,CXX,$@)$(CXX) $< $(CXXFLAGS) -o $@

%.dep-s:
	$(Q) echo -n "$@ " > $@
	$(Q)$(call P,CC,$@)$(CC) -MM $* >> $@
