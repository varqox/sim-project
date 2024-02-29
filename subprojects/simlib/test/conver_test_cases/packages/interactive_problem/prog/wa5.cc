// Krzysztof Małysa
#include <iostream>

using namespace std;

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	int n;
	cin >> n;

	cout << "! -3\n" << flush;

	for (;;)
		new char; // Runtime error happens after the checker exited with WRONG

	return 0;
}
