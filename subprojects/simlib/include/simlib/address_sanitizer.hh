#pragma once

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ADDRESS_SANITIZER 1
#else
#define ADDRESS_SANITIZER 0
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define ADDRESS_SANITIZER 1
#else
#define ADDRESS_SANITIZER 0
#endif
