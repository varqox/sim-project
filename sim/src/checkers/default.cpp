extern "C" {
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

int areEqual(char *s1, size_t l1, char *s2, size_t l2) {
  while (l1 && isspace(s1[l1 - 1])) --l1;
  while (l2 && isspace(s2[l2 - 1])) --l2;
  s1[l1] = s2[l2] = '\0';
  return strcmp(s1, s2);
}

int main(int argc, char *argv[]) {
  // argv[0] command (ignored)
  // argv[1] test_in
  // argv[2] test_out (right answer)
  // argv[3] answer to check
  assert(argc == 4);
  FILE *fout = fopen(argv[2], "r"), *fans = fopen(argv[3], "r");
  assert(fout && fans);  // Open has not failed

  size_t len1 = 0, len2 = 0, line = 0;
  ssize_t read1, read2;
  char *lout = NULL, *lans = NULL;
  while (read1 = getline(&lout, &len1, fout),
         read2 = getline(&lans, &len2, fans), read1 != -1 && read2 != -1) {
    ++line;
    if (0 != areEqual(lout, read1, lans, read2)) {
      printf("Line %zu: Read: '%s', Expected: '%s'\n", line, lans, lout);
      return 1;
    }
  }
  while (read1 != -1) {
    if (0 != areEqual(lout, read1, lans, 0)) {
      printf("Line %zu: Read: EOF, Expected: '%s'\n", line, lout);
      return 1;
    }
    read1 = getline(&lout, &len1, fout);
    ++line;
  }
  while (read2 != -1) {
    if (0 != areEqual(lans, read2, lout, 0)) {
      printf("Line %zu: Read: '%s', Expected: EOF\n", line, lans);
      return 1;
    }
    read2 = getline(&lans, &len2, fans);
    ++line;
  }
  return 0;
}
}  // extern "C"
