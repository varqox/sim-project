# Compile commands
CC = gcc
CXX = g++
LINK = g++
CFLAGS = -O3
CXXFLAGS = -O3 -c
LFLAGS = -s -O3
RM = rm -f

.PHONY: all clean force-clean

all: ../CTH

../CTH: main.o coloring.o aho.o
	$(LINK) $^ $(LFLAGS) -o $@

main.o: main.cpp main.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

coloring.o: coloring.cpp main.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

aho.o: aho.cpp main.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

clean:
	$(RM) *.o

force-clean:
	$(RM) *.o