#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int are_equal(char* s1, size_t l1, char* s2, size_t l2) {
	while (l1 && isspace(s1[l1 - 1]))
		--l1;

	while (l2 && isspace(s2[l2 - 1]))
		--l2;

	s1[l1] = s2[l2] = '\0';
	return strcmp(s1, s2);
}

#ifdef NDEBUG
#define my_assert(...) ((void)0)
#else
#define my_assert(expr)                                                        \
	((expr) ? (void)0                                                          \
	        : (fprintf(stderr, "%s:%i %s: Assertion `%s' failed.\n", __FILE__, \
	                   __LINE__, __PRETTY_FUNCTION__, #expr),                  \
	           exit(-1)))
#endif

int main(int argc, char** argv) {
	// argv[0] command (ignored)
	// argv[1] test_in
	// argv[2] test_out (right answer)
	// argv[3] answer to check
	//
	// Output:
	// Line 1: "OK" or "WRONG"
	// Line 2 (optional; ignored if line 1 == "WRONG" - score is set to 0
	// anyway):
	//   leave empty or provide a real number x from interval [0, 100], which
	//   means that the test will get no more than x percent of its maximal
	//   score.
	// Line 3 and next (optional): A checker comment

	my_assert(argc == 4);
	FILE *fout = fopen(argv[2], "r"), *fans = fopen(argv[3], "r");
	my_assert(fout && fans); // Open was successful

	size_t len1 = 0, len2 = 0, line = 0;
	ssize_t read1, read2;
	char *lout = NULL, *lans = NULL;

	while (read1 = getline(&lout, &len1, fout),
	       read2 = getline(&lans, &len2, fans), read1 != -1 && read2 != -1) {
		++line;

		if (0 != are_equal(lout, read1, lans, read2)) {
			printf("WRONG\n0\nLine %zu: read: '%.77s%s', expected: '%.77s%s'\n",
			       line, lans, (strlen(lans) > 77 ? "..." : ""), lout,
			       (strlen(lout) > 77 ? "..." : ""));
			return 0;
		}
	}

	while (read1 != -1) {
		++line;

		if (0 != are_equal(lout, read1, lans, 0)) {
			printf("WRONG\n0\nLine %zu: read: EOF, expected: '%.157s%s'\n",
			       line, lout, (strlen(lout) > 157 ? "..." : ""));
			return 0;
		}

		read1 = getline(&lout, &len1, fout);
	}

	while (read2 != -1) {
		++line;

		if (0 != are_equal(lans, read2, lout, 0)) {
			printf("WRONG\n0\nLine %zu: read: '%.157s%s', expected: EOF\n",
			       line, lans, (strlen(lans) > 157 ? "..." : ""));
			return 0;
		}

		read2 = getline(&lans, &len2, fans);
	}

	puts("OK");
	return 0;
}

#ifdef __cplusplus
} // extern "C"
#endif
