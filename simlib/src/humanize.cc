#include "../include/humanize.hh"
#include "../include/concat_tostr.hh"

using std::string;

string humanize_file_size(uint64_t size) {
	constexpr uint64_t MIN_KIB = 1ull << 10;
	constexpr uint64_t MIN_MIB = 1ull << 20;
	constexpr uint64_t MIN_GIB = 1ull << 30;
	constexpr uint64_t MIN_TIB = 1ull << 40;
	constexpr uint64_t MIN_PIB = 1ull << 50;
	constexpr uint64_t MIN_EIB = 1ull << 60;
	constexpr uint64_t MIN_3DIGIT_KIB = 102349ull;
	constexpr uint64_t MIN_3DIGIT_MIB = 104805172ull;
	constexpr uint64_t MIN_3DIGIT_GIB = 107320495309ull;
	constexpr uint64_t MIN_3DIGIT_TIB = 109896187196212ull;
	constexpr uint64_t MIN_3DIGIT_PIB = 112533595688920269ull;

	// Bytes
	if (size < MIN_KIB)
		return (size == 1 ? "1 byte" : concat_tostr(size, " bytes"));

	double dsize = size;
	// KiB
	if (size < MIN_3DIGIT_KIB)
		return to_string(dsize / MIN_KIB, 1) + " KiB";
	if (size < MIN_MIB)
		return to_string(dsize / MIN_KIB, 0) + " KiB";
	// MiB
	if (size < MIN_3DIGIT_MIB)
		return to_string(dsize / MIN_MIB, 1) + " MiB";
	if (size < MIN_GIB)
		return to_string(dsize / MIN_MIB, 0) + " MiB";
	// GiB
	if (size < MIN_3DIGIT_GIB)
		return to_string(dsize / MIN_GIB, 1) + " GiB";
	if (size < MIN_TIB)
		return to_string(dsize / MIN_GIB, 0) + " GiB";
	// TiB
	if (size < MIN_3DIGIT_TIB)
		return to_string(dsize / MIN_TIB, 1) + " TiB";
	if (size < MIN_PIB)
		return to_string(dsize / MIN_TIB, 0) + " TiB";
	// PiB
	if (size < MIN_3DIGIT_PIB)
		return to_string(dsize / MIN_PIB, 1) + " PiB";
	if (size < MIN_EIB)
		return to_string(dsize / MIN_PIB, 0) + " PiB";
	// EiB
	return to_string(dsize / MIN_EIB, 1) + " EiB";
}
