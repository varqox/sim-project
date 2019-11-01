#include <sim/random.hh>
#include <simlib/debug.h>
#include <simlib/random.h>

using std::string;

string generate_random_token(size_t length) {
	STACK_UNWINDING_MARK;

	constexpr char t[] =
	   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	constexpr size_t len = sizeof(t) - 1;

	// Generate random id of length SESSION_ID_LENGTH
	string res(length, '0');
	for (char& c : res)
		c = t[getRandom<int>(0, len - 1)];

	return res;
}
