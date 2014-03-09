
.PHONY: all clean mrproper judge_machine CTH

all: judge_machine CTH

judge_machine:
	make -C Judge_machine

CTH:
	make -C CppToHTML

clean:
	$(RM) *.o
	make clean -C Judge_machine
	make clean -C CppToHTML

mrproper:
	$(RM) *.o
	$(RM) judge_machine CTH test
	make mrproper -C Judge_machine
	make mrproper -C CppToHTML