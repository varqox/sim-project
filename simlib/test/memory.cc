#include "simlib/memory.hh"

#include <gtest/gtest.h>

using std::unique_ptr;

TEST(DISABLED_memory, delete_using_free) {
	unique_ptr<char[], delete_using_free>((char*)malloc(42));

	unique_ptr<struct stat, delete_using_free>(
	   (struct stat*)malloc(sizeof(struct stat)));

	// TODO: add some mocking
}
