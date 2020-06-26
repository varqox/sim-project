#pragma once

#include <fcntl.h>

// Access permissions
constexpr int S_0600 = S_IRUSR | S_IWUSR;
constexpr int S_0644 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
constexpr int S_0700 = S_IRWXU;
constexpr int S_0755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
