#pragma once

#include <cstdlib>

// Deleter that uses free() to deallocate, useful with std::unique_ptr<>
struct delete_using_free {
	template <class T>
	void operator()(T* p) const noexcept {
		free(p); // NOLINT(cppcoreguidelines-no-malloc)
	}
};
