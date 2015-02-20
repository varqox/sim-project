include ../../Makefile.config

.PHONY: all
all: ../judge_machine

ifneq ($(MAKECMDGOALS), clean)
-include $(shell ls | egrep "\.c$$|\.cpp$$" | sed "s/$$/\.deps/")
endif

../judge_machine: main.o compile.o other_functions.o submissions_queue.o judge.o db.o
	$(Q)$(call P,LINK,$@)$(CXX) $^ $(LFLAGS) -o $@ -lmysqlcppconn

.PHONY: clean
clean:
	$(Q)$(RM) *.o *.deps ../judge_machine
