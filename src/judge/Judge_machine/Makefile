troll = $(shell ls | grep '.cpp')

PHONY := all
all: ../judge_machine

../judge_machine: main.o compile.o other_functions.o reports_queue.o judge.o
	$(Q)$(P_LINK)$(LINK) $^ $(LFLAGS) -o $@ -lmysqlcppconn

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
