#include "common/common.h"
#include "common/msg.h"
#include "config.h"
#include "options/path.h"
#include "test_utils.h"

static void test_join(char *file, int line, char *a, char *b, char *c)
{
    char *res = mp_path_join(NULL, a, b);
    if (strcmp(res, c) != 0) {
        printf("%s:%d: '%s' + '%s' = '%s', expected '%s'\n", file, line,
               a, b, res, c);
        abort();
    }
    talloc_free(res);
}

static void test_abs(char *file, int line, bool abs, char *a)
{
    if (mp_path_is_absolute(bstr0(a)) != abs) {
        printf("%s:%d: mp_path_is_absolute('%s') => %d, expected %d\n",
               file, line, a, !abs, abs);
        abort();
    }
}

static void test_normalize(char *file, int line, char *expected, char *path)
{
    void *ctx = talloc_new(NULL);
    char *normalized = mp_normalize_path(ctx, path);
    if (strcmp(normalized, expected)) {
        printf("%s:%d: mp_normalize_path('%s') => %s, expected %s\n",
               file, line, path, normalized, expected);
        fflush(stdout);
        abort();
    }
    talloc_free(ctx);
}

#define TEST_JOIN(a, b, c) \
    test_join(__FILE__, __LINE__, a, b, c);

#define TEST_ABS(abs, a) \
    test_abs(__FILE__, __LINE__, abs, a)

#define TEST_NORMALIZE(expected, path) \
    test_normalize(__FILE__, __LINE__, expected, path)

int main(void)
{
    TEST_ABS(true, "/ab");
    TEST_ABS(false, "ab");
    TEST_JOIN("",           "",             "");
    TEST_JOIN("a",          "",             "a");
    TEST_JOIN("/a",         "",             "/a");
    TEST_JOIN("",           "b",            "b");
    TEST_JOIN("",           "/b",           "/b");
    TEST_JOIN("ab",         "cd",           "ab/cd");
    TEST_JOIN("ab/",        "cd",           "ab/cd");
    TEST_JOIN("ab/",        "/cd",          "/cd");
    // Note: we prefer "/" on win32, but tolerate "\".
#if HAVE_DOS_PATHS
    TEST_ABS(true, "\\ab");
    TEST_ABS(true, "c:\\");
    TEST_ABS(true, "c:/");
    TEST_ABS(false, "c:");
    TEST_ABS(false, "c:a");
    TEST_ABS(false, "c:a\\");
    TEST_JOIN("ab\\",       "cd",           "ab\\cd");
    TEST_JOIN("ab\\",       "\\cd",         "\\cd");
    TEST_JOIN("c:/",        "de",           "c:/de");
    TEST_JOIN("c:/a",       "de",           "c:/a/de");
    TEST_JOIN("c:\\a",      "c:\\b",        "c:\\b");
    TEST_JOIN("c:/a",       "c:/b",         "c:/b");
    // Note: drive-relative paths are not always supported "properly"
    TEST_JOIN("c:/a",       "d:b",          "c:/a/d:b");
    TEST_JOIN("c:a",        "b",            "c:a/b");
    TEST_JOIN("c:",         "b",            "c:b");
#endif

    TEST_NORMALIZE("https://foo", "https://foo");
    TEST_NORMALIZE("/foo", "/foo");

    void *ctx = talloc_new(NULL);
    bstr dst = bstr0(mp_getcwd(ctx));
    bstr_xappend(ctx, &dst, bstr0("/foo"));
    TEST_NORMALIZE(dst.start, "foo");
    talloc_free(ctx);

#if (!HAVE_DOS_PATHS)
    TEST_NORMALIZE("/foo/bar", "/foo//bar");
    TEST_NORMALIZE("/foo/bar", "/foo///bar");
    TEST_NORMALIZE("/foo/bar", "/foo/bar/");
    TEST_NORMALIZE("/foo/bar", "/foo/./bar");
    TEST_NORMALIZE("/usr", "/usr/bin/..");
#endif

    return 0;
}
