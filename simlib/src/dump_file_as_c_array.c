#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        (void)fprintf(stderr, "Usage: %s <file> <c_array_name>\n", argv[0]);
        return 1;
    }

    const char* c_array_name = argv[2];
    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        perror("Error: open()");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st)) {
        perror("Error: fstat()");
        return 1;
    }

    unsigned char* data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (data == MAP_FAILED) { // NOLINT(performance-no-int-to-ptr)
        perror("Error: mmap()");
        return 1;
    }
    // Make stdout full-buffered
    if (setvbuf(stdout, NULL, _IOFBF, BUFSIZ)) {
        perror("Error: setvbuf()");
        return 1;
    }

    printf("#pragma GCC diagnostic push\n");
    printf("#pragma GCC diagnostic ignored \"-Woverlength-strings\"\n");
    printf("const unsigned char %s[] = \"", c_array_name);
    for (off_t i = 0; i < st.st_size; ++i) {
        int c = data[i];
        putchar('\\');
        putchar((c >> 6) + '0');
        putchar(((c >> 3) & 7) + '0');
        putchar((c & 7) + '0');
    }
    printf("\";\n");
    printf("#pragma GCC diagnostic pop\n");
    printf("const unsigned int %s_len = %ji;\n", c_array_name, (intmax_t)st.st_size);

    return 0;
}
