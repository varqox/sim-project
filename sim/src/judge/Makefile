# Compile commands
CC = gcc
CXX = g++
LINK = g++
CFLAGS = -O3
CXXFLAGS = -O3 -c
LFLAGS = -s -O3
RM = rm -f

.PHONY: all clean force-clean judge_machine CTH

all: judge_machine CTH

judge_machine:
	make -C Judge_machine $(MFLAGS)

CTH:
	make -C CppToHTML $(MFLAGS)

clean:
	$(RM) *.o
	make clean -C Judge_machine $(MFLAGS)
	make clean -C CppToHTML $(MFLAGS)

force-clean:
	$(RM) *.o
	$(RM) judge_machine CTH test
	make force-clean -C Judge_machine $(MFLAGS)
	make force-clean -C CppToHTML $(MFLAGS)