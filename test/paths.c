#include "common/common.h"
#include "common/msg.h"
#include "config.h"
#include "options/path.h"
#include "test_utils.h"

static void test_join(char *file, int line, char *a, char *b, char *c)
{
    char *res = mp_path_join(NULL, a, b);
    assert_string_equal_impl(file, line, res, c);
    talloc_free(res);
}


static void test_normalize(char *file, int line, char *expected, char *path)
{
    void *ctx = talloc_new(NULL);
    char *normalized = mp_normalize_path(ctx, path);
    assert_string_equal_impl(file, line, normalized, expected);
    talloc_free(ctx);
}

#define TEST_JOIN(a, b, c) \
    test_join(__FILE__, __LINE__, a, b, c);

#define TEST_ABS(abs, a) \
    assert_int_equal_impl(__FILE__, __LINE__, abs, mp_path_is_absolute(bstr0(a)))

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
#if !HAVE_DOS_PATHS
    TEST_NORMALIZE("/foo", "/foo");
#endif

#if defined(_WIN32) && (!defined(HAVE_PATHCCH) || !HAVE_PATHCCH)
    return 0;
#endif

    void *ctx = talloc_new(NULL);
    bstr dst = bstr0(mp_getcwd(ctx));
    bstr_xappend(ctx, &dst, bstr0("/foo"));
#if HAVE_DOS_PATHS
    char *p = dst.start;
    while (*p) {
        *p = *p == '/' ? '\\' : *p;
        p++;
    }
#endif
    TEST_NORMALIZE(dst.start, "foo");
    talloc_free(ctx);

#if HAVE_DOS_PATHS
    TEST_NORMALIZE("C:\\foo\\baz", "C:/foo/bar/../baz");
    TEST_NORMALIZE("C:\\", "C:/foo/../..");
    TEST_NORMALIZE("C:\\foo\\baz", "C:/foo/bar/./../baz");
    TEST_NORMALIZE("C:\\foo\\bar\\baz", "C:/foo//bar/./baz");
    TEST_NORMALIZE("C:\\foo\\bar\\baz", "C:/foo\\./bar\\/baz");
    TEST_NORMALIZE("C:\\file.mkv", "\\\\?\\C:\\folder\\..\\file.mkv");
    TEST_NORMALIZE("C:\\dir", "\\\\?\\C:\\dir\\subdir\\..\\.");
    TEST_NORMALIZE("D:\\newfile.txt", "\\\\?\\D:\\\\new\\subdir\\..\\..\\newfile.txt");
    TEST_NORMALIZE("\\\\server\\share\\path", "\\\\?\\UNC/server/share/path/.");
    TEST_NORMALIZE("C:\\", "C:/.");
    TEST_NORMALIZE("C:\\", "C:/../");
#else
    TEST_NORMALIZE("/foo/bar", "/foo//bar");
    TEST_NORMALIZE("/foo/bar", "/foo///bar");
    TEST_NORMALIZE("/foo/bar", "/foo/bar/");
    TEST_NORMALIZE("/foo/bar", "/foo/./bar");
    TEST_NORMALIZE("/usr", "/usr/bin/..");
#endif

    return 0;
}
