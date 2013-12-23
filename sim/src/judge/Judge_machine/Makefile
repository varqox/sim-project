# Compile commands
CC = gcc
CXX = g++
LINK = g++
CFLAGS = -O3
CXXFLAGS = -O3 -c
LFLAGS = -s -O3
RM = rm -f

.PHONY: all clean force-clean

all: ../judge_machine

../judge_machine: main.o compile.o other_functions.o reports_queue.o judge.o
	$(LINK) $^ $(LFLAGS) -o $@

main.o: main.cpp main.hpp judge.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

compile.o: compile.cpp main.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

reports_queue.o: reports_queue.cpp
	$(CXX) $< $(CXXFLAGS) -o $@

other_functions.o: other_functions.cpp
	$(CXX) $< $(CXXFLAGS) -o $@

judge.o: judge.cpp judge.hpp main.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

clean:
	$(RM) *.o

force-clean:
	$(RM) *.o