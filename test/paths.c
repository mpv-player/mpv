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

#define TEST_JOIN(a, b, c) \
    test_join(__FILE__, __LINE__, a, b, c);

#define TEST_ABS(abs, a) \
    test_abs(__FILE__, __LINE__, abs, a)

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
    return 0;
}
