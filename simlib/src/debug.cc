#include "../include/debug.h"

extern "C" int __cxa_thread_atexit(void (*func)(void*), void *obj,
	void *dso_symbol)
{
	int __cxa_thread_atexit_impl(void (*)(void*), void *, void *);
	return __cxa_thread_atexit_impl(func, obj, dso_symbol);
}

namespace stack_unwinding {
thread_local InplaceArray<InplaceBuff<256>, 32> marks_collected;
} // stack_unwinding
