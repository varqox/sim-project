
.PHONY: all clean mrproper

all: ../CTH

../CTH: main.o coloring.o aho.o
	$(LINK) $^ $(LFLAGS) -o $@

main.o: main.cpp main.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

coloring.o: coloring.cpp main.hpp aho.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

aho.o: aho.cpp main.hpp aho.hpp
	$(CXX) $< $(CXXFLAGS) -o $@

clean:
	$(RM) *.o

mrproper:
	$(RM) *.o