DB_HPP = db.hpp
JUDGE_HPP = judge.hpp
REPORTS_QUEUE_HPP = reports_queue.hpp $(DB_HPP)
MAIN_HPP = main.hpp $(JUDGE_HPP) $(REPORTS_QUEUE_HPP)

.PHONY: all clean mrproper

all: ../judge_machine

../judge_machine: main.o compile.o other_functions.o reports_queue.o judge.o
	$(LINK) $^ $(LFLAGS) -o $@ -lmysqlcppconn

main.o: main.cpp $(MAIN_HPP) $(JUDGE_HPP)
	$(CXX) $< $(CXXFLAGS) -o $@

compile.o: compile.cpp $(MAIN_HPP)
	$(CXX) $< $(CXXFLAGS) -o $@

reports_queue.o: reports_queue.cpp $(REPORTS_QUEUE_HPP)
	$(CXX) $< $(CXXFLAGS) -o $@

other_functions.o: other_functions.cpp
	$(CXX) $< $(CXXFLAGS) -o $@

judge.o: judge.cpp $(JUDGE_HPP) $(MAIN_HPP)
	$(CXX) $< $(CXXFLAGS) -o $@

clean:
	$(RM) *.o

mrproper:
	$(RM) *.o