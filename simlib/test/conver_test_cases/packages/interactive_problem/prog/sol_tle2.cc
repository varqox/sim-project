// Krzysztof Małysa
#include <iostream>

using namespace std;

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	int n;
	long long k;
	cin >> n >> k;
	int beg = 0, end = n - 1;
	while (beg < end) {
		int mid = (beg + end) >> 1;
		cout << "? " << mid + 1 << '\n' << flush;
		long long x;
		cin >> x;
		if (x < k)
			beg = mid + 1;
		else
			end = mid;
	}

	cout << "! " << beg + 1 << '\n' << flush;

	for (;;) {} // TLE happens after checker exited with OK

	return 0;
}
