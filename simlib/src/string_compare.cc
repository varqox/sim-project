#include <cstddef>

bool slow_equal(const char* str1, const char* str2, size_t len) noexcept {
	int rc = 0;
	for (size_t i = 0; i < len; ++i) {
		rc |= static_cast<unsigned char>(str1[i]) ^
		      static_cast<unsigned char>(str2[i]);
	}

	return (rc == 0);
}
