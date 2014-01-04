# Compile commands
export CC
export CXX
export LINK
export CFLAGS
export CXXFLAGS
export LFLAGS
export RM

.PHONY: all clean dist-clean

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

dist-clean:
	$(RM) *.o