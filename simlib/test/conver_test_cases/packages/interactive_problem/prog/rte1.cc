// Krzysztof Małysa
#include <iostream>

using namespace std;

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	int n;
	cin >> n;
	n /= 1000000000;

	cout << 42 / n;
	cout << "! 1\n" << flush;

	return 0;
}
