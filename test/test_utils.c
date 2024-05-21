#include <libavutil/common.h>

#include "common/msg.h"
#include "options/m_option.h"
#include "options/path.h"
#include "osdep/subprocess.h"
#include "osdep/terminal.h"
#include "test_utils.h"

#ifdef NDEBUG
static_assert(false, "don't define NDEBUG for tests");
#endif

void assert_int_equal_impl(const char *file, int line, int64_t a, int64_t b)
{
    if (a != b) {
        printf("%s:%d: %"PRId64" != %"PRId64"\n", file, line, a, b);
        fflush(stdout);
        abort();
    }
}

void assert_string_equal_impl(const char *file, int line,
                              const char *a, const char *b)
{
    if (strcmp(a, b) != 0) {
        printf("%s:%d: '%s' != '%s'\n", file, line, a, b);
        fflush(stdout);
        abort();
    }
}

void assert_float_equal_impl(const char *file, int line,
                              double a, double b, double tolerance)
{
    if (fabs(a - b) > tolerance) {
        printf("%s:%d: %f != %f\n", file, line, a, b);
        fflush(stdout);
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
        fflush(stdout);
        abort();
    }
    return f;
}

void assert_text_files_equal_impl(const char *file, int line,
                                  const char *refdir, const char *outdir,
                                  const char *ref, const char *new,
                                  const char *err)
{
    char path_ref[4096];
    char path_new[4096];

    snprintf(path_ref, sizeof(path_ref), "%s/%s", refdir, ref);
    snprintf(path_new, sizeof(path_new), "%s/%s", outdir, new);

    bool ok = false;
    FILE *fref = fopen(path_ref, "r");
    FILE *fnew = fopen(path_new, "r");

    if (!fref || !fnew) {
        printf("Error: Could not open files %s or %s\n", path_ref, path_new);
        goto done;
    }

    char ref_line[4096];
    char new_line[4096];
    int line_num = 0;

    while (fgets(ref_line, sizeof(ref_line), fref))
    {
        line_num++;

        if (!fgets(new_line, sizeof(new_line), fnew)) {
            printf("Extra line %d in reference file: %s", line_num, ref_line);
            goto done;
        }

        if (strcmp(ref_line, new_line)) {
            printf("Difference found at line %d:\n", line_num);
            printf("Reference: %s", ref_line);
            printf("New file: %s", new_line);
            goto done;
        }
    }

    if (fgets(new_line, sizeof(new_line), fnew)) {
        printf("Extra line %d in new file: %s", line_num, new_line);
        goto done;
    }

    ok = true;

done:
    if (fref)
        fclose(fref);
    if (fnew)
        fclose(fnew);
    if (!ok) {
        fflush(stdout);
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
    fflush(stdout);
    abort();
}

/* Stubs: see test_utils.h */
struct mp_log *const mp_null_log;
const char *mp_help_text;

void mp_msg(struct mp_log *log, int lev, const char *format, ...) {};
int mp_msg_find_level(const char *s) {return 0;};
int mp_msg_level(struct mp_log *log) {return 0;};
void mp_msg_set_max_level(struct mp_log *log, int lev) {};
int mp_console_vfprintf(void *wstream, const char *format, va_list args) {return 0;};
int mp_console_write(void *wstream, bstr str) {return 0;};
bool mp_check_console(void *handle) { return false; };
void mp_set_avdict(AVDictionary **dict, char **kv) {};
struct mp_log *mp_log_new(void *talloc_ctx, struct mp_log *parent,
                          const char *name) { return NULL; };
