#include "../include/debug.h"

namespace stack_unwinding {
thread_local InplaceArray<InplaceBuff<256>, 32> marks_collected;
} // stack_unwinding
