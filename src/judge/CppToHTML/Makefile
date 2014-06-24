
PHONY := all
all: ../CTH

../CTH: main.o coloring.o aho.o
	$(Q)$(P_LINK)$(LINK) $^ $(LFLAGS) -o $@

.PHONY: $(PHONY)

-include $(shell ls | grep "\.cpp$$" | sed s/.cpp$$/.mkdep/)
-include $(shell ls | grep "\.c$$" | sed s/.c$$/.mkdep/)

%.o: %.c
	$(Q)$(P_CC)$(CC) $< $(CFLAGS) -o $@

%.o: %.cpp
	$(Q)$(P_CXX)$(CXX) $< $(CXXFLAGS) -o $@

%.mkdep: %.c
	$(Q)$(P_CC)$(CC) -MM $< -o $@

%.mkdep: %.cpp
	$(Q)$(P_CC)$(CC) -MM $< -o $@
