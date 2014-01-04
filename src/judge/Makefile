# Compile commands
export CC
export CXX
export LINK
export CFLAGS
export CXXFLAGS
export LFLAGS
export RM

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