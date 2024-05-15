// Krzysztof Małysa
#include <iostream>
#include <unistd.h>

using namespace std;

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	cout << "! ?\n" << flush;

	// Solution should be killed after checker verdicts WRONG
	for (;;) {
		pause(); // To minimize CPU time consumption under load
	}

	return 0;
}
