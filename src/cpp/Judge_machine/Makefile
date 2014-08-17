
PHONY := all
all: ../judge_machine

../judge_machine: main.o compile.o other_functions.o submissions_queue.o judge.o db.o
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@ -lmysqlcppconn

.PHONY: $(PHONY)

-include $(shell ls | egrep ".c$$|.cpp$$" | sed "s/$$/.dep-s/")

%.o: %.c
	$(Q)$(call P,CC,$@)$(CC) $< $(CXXFLAGS) -o $@

%.o: %.cpp
	$(Q)$(call P,CXX,$@)$(CXX) $< $(CXXFLAGS) -o $@

%.dep-s:
	$(Q) echo -n "$@ " > $@
	$(Q)$(call P,CC,$@)$(CC) -MM $* >> $@
