#include "../include/debug.h"

namespace stack_unwinding {
thread_local InplaceArray<InplaceBuff<1024>, 64> marks_collected;
} // namespace stack_unwinding
