
PHONY := all
all: ../judge_machine

../judge_machine: main.o compile.o other_functions.o reports_queue.o judge.o
	$(Q)$(P_LINK)$(LINK) $^ $(LFLAGS) -o $@ -lmysqlcppconn

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
