#ifdef __cplusplus
extern "C" {
#endif

#define _GNU_SOURCE // without this compilation fails because of missing O_CLOEXEC

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Constants below configure how much context is print around the found difference in output.
// solution answer: ___________________x__________________
// test out:        ___________________y__________________
//                  <-- HISTORY_LEN --> <-- FUTURE_LEN -->
#define HISTORY_LEN 24
#define FUTURE_LEN 64
#define BUF_LEN (1 << 16)

struct ResultBuf {
    char buf[512 + HISTORY_LEN + FUTURE_LEN];
    size_t written;
};

void res_buf_append_bytes(struct ResultBuf* res_buf, const char* bytes, size_t len) {
    len = MIN(len, sizeof(res_buf->buf) - res_buf->written);
    memcpy(res_buf->buf + res_buf->written, bytes, len);
    res_buf->written += len;
}

void res_buf_append_str(struct ResultBuf* res_buf, const char* str) {
    while (*str != '\0' && res_buf->written < sizeof(res_buf->buf)) {
        res_buf->buf[res_buf->written++] = *str++;
    }
}

void res_buf_append_u64(struct ResultBuf* res_buf, uint64_t x) {
    char rev_str_x[sizeof("18446744073709551615") - 1];
    size_t written = 0;
    do {
        rev_str_x[written++] = (char)('0' + (char)(x % 10));
        x /= 10;
    } while (x > 0);

    while (written > 0 && res_buf->written < sizeof(res_buf->buf)) {
        res_buf->buf[res_buf->written++] = rev_str_x[--written];
    }
}

__attribute__((noreturn)) void res_buf_print_and_exit(const struct ResultBuf* res_buf) {
    (void)write(STDERR_FILENO, res_buf->buf, res_buf->written);
    _exit(0);
}

__attribute__((noreturn)) void
assert_failed(const char* file, uint64_t line, const char* func, const char* expr) {
    struct ResultBuf res_buf;
    res_buf.written = 0;
    res_buf_append_str(&res_buf, file);
    res_buf_append_str(&res_buf, ":");
    res_buf_append_u64(&res_buf, line);
    res_buf_append_str(&res_buf, ": ");
    res_buf_append_str(&res_buf, func);
    res_buf_append_str(&res_buf, ": Assertion `");
    res_buf_append_str(&res_buf, expr);
    res_buf_append_str(&res_buf, "'failed\n");
    (void)write(STDERR_FILENO, res_buf.buf, res_buf.written);
    _exit(1);
}

#define checker_assert(expr)                                                           \
    ((expr) ? (void)0 : assert_failed(__FILE__, __LINE__, __PRETTY_FUNCTION__, #expr))

#define STATIC_ASSERT(cond) STATIC_ASSERT_IMPL(cond, __LINE__)
#define STATIC_ASSERT_IMPL(cond, line) STATIC_ASSERT_IMPL2(cond, line)
#define STATIC_ASSERT_IMPL2(cond, line)                                \
    extern const char static_assertion_at_line_##line[(cond) ? 1 : -1]

struct FileReader {
    char* buf;
    size_t len;
    size_t beg;
    size_t max_len;
    int fd;
    bool eof;
};

void init_file_reader(
    struct FileReader* reader, char* underlying_buf, size_t underlying_buf_max_len, int fd
) {
    reader->buf = underlying_buf;
    reader->len = 0;
    reader->beg = 0;
    reader->max_len = underlying_buf_max_len;
    reader->fd = fd;
    reader->eof = false;
}

void fill_file_reader_if_empty(struct FileReader* reader) {
    if (reader->len == 0 && !reader->eof) {
        ssize_t len = read(reader->fd, reader->buf, reader->max_len);
        checker_assert(len >= 0);
        if (len == 0) {
            reader->eof = true;
        }
        reader->len = len;
        reader->beg = 0;
    }
}

void fill_file_reader_with_history(struct FileReader* reader) {
    if (reader->len == 0 && !reader->eof) {
        if (reader->beg > HISTORY_LEN) {
            memmove(reader->buf, reader->buf + reader->beg - HISTORY_LEN, HISTORY_LEN);
            reader->beg = HISTORY_LEN;
        }
        ssize_t len = read(reader->fd, reader->buf + reader->beg, reader->max_len - reader->beg);
        checker_assert(len >= 0);
        if (len == 0) {
            reader->eof = true;
        }
        reader->len = len;
    }
}

void advance_reader(struct FileReader* reader) {
    ++reader->beg;
    --reader->len;
}

struct HistoryToPrint {
    char buf[HISTORY_LEN];
    size_t len;
};

struct FileLoc {
    uint64_t line;
    uint64_t column;
};

void advance_file_loc(struct FileLoc* file_loc, char c) {
    if (c == '\n') {
        ++file_loc->line;
        file_loc->column = 1;
    } else {
        ++file_loc->column;
    }
}

void fill_histories_to_print(
    const struct FileReader* ans_reader,
    struct HistoryToPrint* ans_history,
    struct FileLoc* ans_file_pos,
    const struct FileReader* out_reader,
    struct HistoryToPrint* out_history,
    ssize_t ans_reader_is_this_bytes_ahead
) {
    const struct FileReader* further_reader =
        ans_reader_is_this_bytes_ahead >= 0 ? ans_reader : out_reader;
    size_t longer_history_len = 0;
    for (;;) {
        if (longer_history_len == HISTORY_LEN) {
            break;
        }
        if (further_reader->beg - longer_history_len == 0) {
            // History is shorter than HISTORY_LEN
            break;
        }
        char c = further_reader->buf[further_reader->beg - longer_history_len - 1];
        if (c == '\n' || c == ' ') {
            break;
        }
        ++longer_history_len;
    }

    struct HistoryToPrint* longer_history;
    struct HistoryToPrint* shorter_history;
    size_t longer_is_ahead_of_shorter_this_bytes_num;
    if (ans_reader_is_this_bytes_ahead >= 0) {
        longer_history = ans_history;
        shorter_history = out_history;
        longer_is_ahead_of_shorter_this_bytes_num = ans_reader_is_this_bytes_ahead;
    } else {
        longer_history = out_history;
        shorter_history = ans_history;
        longer_is_ahead_of_shorter_this_bytes_num = -ans_reader_is_this_bytes_ahead;
    }

    if (longer_history_len == HISTORY_LEN) {
        STATIC_ASSERT(HISTORY_LEN >= 3);
        longer_history->buf[0] = '.';
        longer_history->buf[1] = '.';
        longer_history->buf[2] = '.';
        memcpy(
            longer_history->buf + 3,
            further_reader->buf + further_reader->beg - longer_history_len + 3,
            longer_history_len - 3
        );
    } else {
        memcpy(
            longer_history->buf,
            further_reader->buf + further_reader->beg - longer_history_len,
            longer_history_len
        );
    }
    longer_history->len = longer_history_len;

    if (longer_is_ahead_of_shorter_this_bytes_num < longer_history_len) {
        memcpy(
            shorter_history->buf,
            longer_history->buf,
            longer_history_len - longer_is_ahead_of_shorter_this_bytes_num
        );
        shorter_history->len = longer_history_len - longer_is_ahead_of_shorter_this_bytes_num;
    } else {
        shorter_history->len = 0;
    }

    if (longer_history == ans_history) {
        ans_file_pos->column -=
            longer_history_len == HISTORY_LEN ? longer_history_len - 3 : longer_history_len;
    } else {
        ans_file_pos->column -= longer_history_len == HISTORY_LEN
            ? (shorter_history->len <= 3 ? 0 : shorter_history->len - 3)
            : shorter_history->len;
    }
}

struct FutureToPrint {
    char buf[FUTURE_LEN];
    size_t len;
};

void fill_future_to_print(struct FileReader* reader, struct FutureToPrint* future) {
    size_t len = 0;
    for (;;) {
        if (len == FUTURE_LEN) {
            break;
        }
        fill_file_reader_if_empty(reader);
        if (reader->eof || reader->buf[reader->beg] == '\n') {
            // Trim trailing whitespace not to print it in the context
            while (len > 0 && future->buf[len - 1] == ' ') {
                --len;
            }
            break;
        }
        future->buf[len++] = reader->buf[reader->beg];
        advance_reader(reader);
    }
    if (len == FUTURE_LEN) {
        STATIC_ASSERT(FUTURE_LEN >= 3);
        future->buf[len - 1] = '.';
        future->buf[len - 2] = '.';
        future->buf[len - 3] = '.';
    }
    future->len = len;
}

void start_wrong_answer(struct ResultBuf* res_buf, const struct FileLoc* file_pos) {
    res_buf->written = 0;
    res_buf_append_str(res_buf, "WRONG\n0\nLine ");
    res_buf_append_u64(res_buf, file_pos->line);
    res_buf_append_str(res_buf, " column ");
    res_buf_append_u64(res_buf, file_pos->column);
    res_buf_append_str(res_buf, ": ");
}

int main(int argc, char** argv) {
    // argv[0] command (ignored)
    // argv[1] test_in
    // argv[2] test_out (right answer)
    // argv[3] answer to check
    //
    // Output (to stderr):
    //   Line 1: "OK" or "WRONG"
    //   Line 2 (optional; ignored if line 1 == "WRONG" - score is set to 0 anyway):
    //     Leave empty or provide a real number x from interval [0, 100], which means that the
    //     solution will get no more than x percent of test's maximal score.
    //   Line 3 and next (optional): A checker comment

    checker_assert(argc == 4);
    int out_fd = open(argv[2], O_RDONLY | O_CLOEXEC);
    int ans_fd = open(argv[3], O_RDONLY | O_CLOEXEC);
    checker_assert(out_fd >= 0 && ans_fd >= 0); // Each open() was successful

    struct FileReader out;
    char out_buf[HISTORY_LEN + BUF_LEN];
    init_file_reader(&out, out_buf, sizeof(out_buf), out_fd);

    struct FileReader ans;
    char ans_buf[HISTORY_LEN + BUF_LEN];
    init_file_reader(&ans, ans_buf, sizeof(ans_buf), ans_fd);

    struct FileLoc ans_file_pos;
    ans_file_pos.line = 1;
    ans_file_pos.column = 1;

    for (;;) {
        fill_file_reader_with_history(&out);
        fill_file_reader_with_history(&ans);

        if (out.eof || ans.eof) {
            ssize_t ans_is_this_bytes_ahead = 0;
            ssize_t ans_is_this_bytes_ahead_dir;
            struct FileReader* longer_reader;
            if (out.eof) {
                ans_is_this_bytes_ahead_dir = 1;
                longer_reader = &ans;
            } else {
                ans_is_this_bytes_ahead_dir = -1;
                longer_reader = &out;
            }

            for (;;) {
                fill_file_reader_with_history(longer_reader);
                if (longer_reader->eof) {
                    break;
                }
                char c = longer_reader->buf[longer_reader->beg];
                if (c == ' ' || c == '\n') {
                    ans_is_this_bytes_ahead += ans_is_this_bytes_ahead_dir;
                    advance_reader(longer_reader);
                    if (longer_reader == &ans) {
                        advance_file_loc(&ans_file_pos, c);
                    }
                    continue;
                }

                // c is a non-whitespace character and EOF was expected
                struct HistoryToPrint ans_history;
                struct HistoryToPrint out_history;
                fill_histories_to_print(
                    &ans, &ans_history, &ans_file_pos, &out, &out_history, ans_is_this_bytes_ahead
                );
                // Future starts AFTER the current character
                advance_reader(longer_reader);
                // We don't advance ans_file_pos since it points to the beginning of the history

                struct ResultBuf res_buf;
                start_wrong_answer(&res_buf, &ans_file_pos);
                res_buf_append_str(&res_buf, "read ");
                if (longer_reader == &ans || ans_history.len > 0) {
                    res_buf_append_str(&res_buf, "'");
                    res_buf_append_bytes(&res_buf, ans_history.buf, ans_history.len);
                    if (longer_reader == &ans) {
                        res_buf_append_bytes(&res_buf, &c, 1);
                        struct FutureToPrint ans_future;
                        fill_future_to_print(&ans, &ans_future);
                        res_buf_append_bytes(&res_buf, ans_future.buf, ans_future.len);
                    }
                    res_buf_append_str(&res_buf, "'");
                } else {
                    res_buf_append_str(&res_buf, "end of file");
                }

                res_buf_append_str(&res_buf, ", expected ");

                if (longer_reader != &ans || out_history.len > 0) {
                    res_buf_append_str(&res_buf, "'");
                    res_buf_append_bytes(&res_buf, out_history.buf, out_history.len);
                    if (longer_reader != &ans) {
                        res_buf_append_bytes(&res_buf, &c, 1);
                        struct FutureToPrint out_future;
                        fill_future_to_print(&out, &out_future);
                        res_buf_append_bytes(&res_buf, out_future.buf, out_future.len);
                    }
                    res_buf_append_str(&res_buf, "'\n");
                } else {
                    res_buf_append_str(&res_buf, "end of file\n");
                }
                res_buf_print_and_exit(&res_buf);
            }
            break;
        }

        ssize_t common_len = MIN(out.len, ans.len);
        while (common_len--) {
            char out_c = out.buf[out.beg];
            char ans_c = ans.buf[ans.beg];

            if (out_c == ans_c) {
                advance_reader(&out);
                advance_reader(&ans);
                advance_file_loc(&ans_file_pos, ans_c);
                continue;
            }

            if (out_c == '\n' || ans_c == '\n') {
                ssize_t ans_is_this_bytes_ahead = 0;
                ssize_t ans_is_this_bytes_ahead_dir;
                struct FileReader* longer_reader;
                struct FileReader* shorter_reader;
                if (out_c == '\n') {
                    ans_is_this_bytes_ahead_dir = 1;
                    longer_reader = &ans;
                    shorter_reader = &out;
                } else {
                    ans_is_this_bytes_ahead_dir = -1;
                    longer_reader = &out;
                    shorter_reader = &ans;
                }

                for (;;) {
                    fill_file_reader_with_history(longer_reader);
                    if (longer_reader->eof) {
                        break;
                    }
                    char c = longer_reader->buf[longer_reader->beg];
                    if (c == '\n') {
                        advance_reader(longer_reader);
                        if (longer_reader == &ans) {
                            advance_file_loc(&ans_file_pos, '\n');
                        }
                        break;
                    }
                    if (c == ' ') {
                        ans_is_this_bytes_ahead += ans_is_this_bytes_ahead_dir;
                        advance_reader(longer_reader);
                        if (longer_reader == &ans) {
                            advance_file_loc(&ans_file_pos, ' ');
                        }
                        continue;
                    }

                    // c is a non-whitespace character and \n was expected
                    struct HistoryToPrint ans_history;
                    struct HistoryToPrint out_history;
                    fill_histories_to_print(
                        &ans,
                        &ans_history,
                        &ans_file_pos,
                        &out,
                        &out_history,
                        ans_is_this_bytes_ahead
                    );
                    // Future starts AFTER the current character
                    advance_reader(longer_reader);
                    // We don't advance ans_file_pos since it points to the beginning of the history

                    struct ResultBuf res_buf;
                    start_wrong_answer(&res_buf, &ans_file_pos);
                    res_buf_append_str(&res_buf, "read '");
                    if (longer_reader == &ans || ans_history.len > 0) {
                        res_buf_append_bytes(&res_buf, ans_history.buf, ans_history.len);
                        if (longer_reader == &ans) {
                            res_buf_append_bytes(&res_buf, &c, 1);
                            struct FutureToPrint ans_future;
                            fill_future_to_print(&ans, &ans_future);
                            res_buf_append_bytes(&res_buf, ans_future.buf, ans_future.len);
                        }
                    }
                    res_buf_append_str(&res_buf, "', expected '");
                    if (longer_reader != &ans || out_history.len > 0) {
                        res_buf_append_bytes(&res_buf, out_history.buf, out_history.len);
                        if (longer_reader != &ans) {
                            res_buf_append_bytes(&res_buf, &c, 1);
                            struct FutureToPrint out_future;
                            fill_future_to_print(&out, &out_future);
                            res_buf_append_bytes(&res_buf, out_future.buf, out_future.len);
                        }
                    }
                    res_buf_append_str(&res_buf, "'\n");
                    res_buf_print_and_exit(&res_buf);
                }
                advance_reader(shorter_reader);
                if (shorter_reader == &ans) {
                    advance_file_loc(&ans_file_pos, '\n');
                }
                break;
            }

            // Found difference inside the current line
            struct HistoryToPrint ans_history;
            struct HistoryToPrint out_history;
            fill_histories_to_print(&ans, &ans_history, &ans_file_pos, &out, &out_history, 0);

            // Future starts AFTER the current character
            advance_reader(&out);
            advance_reader(&ans);
            // We don't advance ans_file_pos since it points to the beginning of the history

            struct FutureToPrint ans_future;
            fill_future_to_print(&ans, &ans_future);
            struct FutureToPrint out_future;
            fill_future_to_print(&out, &out_future);

            struct ResultBuf res_buf;
            start_wrong_answer(&res_buf, &ans_file_pos);
            res_buf_append_str(&res_buf, "read '");
            res_buf_append_bytes(&res_buf, ans_history.buf, ans_history.len);
            res_buf_append_bytes(&res_buf, &ans_c, 1);
            res_buf_append_bytes(&res_buf, ans_future.buf, ans_future.len);
            res_buf_append_str(&res_buf, "', expected '");
            res_buf_append_bytes(&res_buf, out_history.buf, out_history.len);
            res_buf_append_bytes(&res_buf, &out_c, 1);
            res_buf_append_bytes(&res_buf, out_future.buf, out_future.len);
            res_buf_append_str(&res_buf, "'\n");
            res_buf_print_and_exit(&res_buf);
        }
    }

    (void)write(STDERR_FILENO, "OK\n", 3);
    return 0;
}

#ifdef __cplusplus
} // extern "C"
#endif
