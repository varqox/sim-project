// Krzysztof Małysa
#include <iostream>

using namespace std;

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	int n;
	long long k;
	cin >> n >> k;
	for (int i = 1; i <= n; ++i) {
		cout << "? " << i << '\n' << flush;
		long long x;
		cin >> x;
		if (x == k) {
			cout << "! " << i << '\n' << flush;
			break;
		}
	}

	return 0;
}
