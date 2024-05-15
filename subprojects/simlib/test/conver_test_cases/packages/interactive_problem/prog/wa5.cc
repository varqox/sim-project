// Krzysztof Małysa
#include <exception>
#include <iostream>

using namespace std;

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	cout << "! -3\n" << flush;

	// Wait for checker to exit with WRONG
	string s;
	while (getline(cin, s)) {}
	// Runtime error
	std::terminate();

	return 0;
}
