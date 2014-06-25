
PHONY := all
all: ../judge_machine

../judge_machine: main.o compile.o other_functions.o reports_queue.o judge.o
	$(Q)$(call P,LINK,$@)$(LINK) $^ $(LFLAGS) -o $@ -lmysqlcppconn

PHONY += dep-s
dep-s: $(shell ls | egrep ".c$$|.cpp$$" | sed "s/$$/.dep-s/")

.PHONY: $(PHONY)

include $(shell ls | grep ".dep-s$$")

%.o: %.c
	$(Q)$(call P,CC,$@)$(CC) $< $(CXXFLAGS) -o $@

%.o: %.cpp
	$(Q)$(call P,CXX,$@)$(CXX) $< $(CXXFLAGS) -o $@

$(foreach xx, $(shell ls | egrep ".dep-s$$"), $(eval $(xx): $(shell cat $(xx) | sed s/.*://)))

%.dep-s:
	$(Q)$(call P,CC,$@)$(CC) -MM $* -o $@
