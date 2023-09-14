#include <libavutil/common.h>

#include "options/m_option.h"
#include "options/path.h"
#include "osdep/subprocess.h"
#include "test_utils.h"

#ifdef NDEBUG
static_assert(false, "don't define NDEBUG for tests");
#endif

void assert_int_equal_impl(const char *file, int line, int64_t a, int64_t b)
{
    if (a != b) {
        printf("%s:%d: %"PRId64" != %"PRId64"\n", file, line, a, b);
        abort();
    }
}

void assert_string_equal_impl(const char *file, int line,
                              const char *a, const char *b)
{
    if (strcmp(a, b) != 0) {
        printf("%s:%d: '%s' != '%s'\n", file, line, a, b);
        abort();
    }
}

void assert_float_equal_impl(const char *file, int line,
                              double a, double b, double tolerance)
{
    if (fabs(a - b) > tolerance) {
        printf("%s:%d: %f != %f\n", file, line, a, b);
        abort();
    }
}

FILE *test_open_out(const char *outdir, const char *name)
{
    mp_mkdirp(outdir);
    assert(mp_path_isdir(outdir));
    char *path = mp_tprintf(4096, "%s/%s", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("Could not open '%s' for writing: %s\n", path,
               mp_strerror(errno));
        abort();
    }
    return f;
}

void assert_text_files_equal_impl(const char *file, int line,
                                  const char *refdir, const char *outdir,
                                  const char *ref, const char *new,
                                  const char *err)
{
    char *path_ref = mp_tprintf(4096, "%s/%s", refdir, ref);
    char *path_new = mp_tprintf(4096, "%s/%s", outdir, new);

    struct mp_subprocess_opts opts = {
        .exe = "diff",
        .args = (char*[]){"diff", "-u", "--", path_ref, path_new, 0},
        .fds = { {0, .src_fd = 0}, {1, .src_fd = 1}, {2, .src_fd = 2} },
        .num_fds = 3,
    };

    struct mp_subprocess_result res;
    mp_subprocess2(&opts, &res);

    if (res.error || res.exit_status) {
        if (res.error)
            printf("Note: %s\n", mp_subprocess_err_str(res.error));
        printf("Giving up.\n");
        abort();
    }
}

static void hexdump(const uint8_t *d, size_t size)
{
    printf("|");
    while (size--) {
        printf(" %02x", d[0]);
        d++;
    }
    printf(" |\n");
}

void assert_memcmp_impl(const char *file, int line,
                        const void *a, const void *b, size_t size)
{
    if (memcmp(a, b, size) == 0)
        return;

    printf("%s:%d: mismatching data:\n", file, line);
    hexdump(a, size);
    hexdump(b, size);
    abort();
}

/* Stubs: see test_utils.h */
struct mp_log *mp_null_log;
const char *mp_help_text;

void mp_msg(struct mp_log *log, int lev, const char *format, ...) {};
int mp_msg_find_level(const char *s) {return 0;};
int mp_msg_level(struct mp_log *log) {return 0;};
void mp_write_console_ansi(void) {};
void mp_set_avdict(AVDictionary **dict, char **kv) {};

#ifndef WIN32_TESTS
void mp_add_timeout(void) {};
void mp_rel_time_to_timespec(void) {};
void mp_time_us(void) {};
void mp_time_us_to_realtime(void) {};
#endif
