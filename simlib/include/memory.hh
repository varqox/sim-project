#pragma once

#include <cstdlib>

// Deleter that uses free() to deallocate, useful with std::unique_ptr
template <class T>
struct delete_using_free {
	void operator()(T* p) const noexcept { free(p); }
};
