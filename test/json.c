#include "test_helpers.h"

#include "common/common.h"
#include "misc/json.h"
#include "misc/node.h"

struct entry {
    const char *src;
    const char *out_txt;
    struct mpv_node out_data;
    bool expect_fail;
};

#define TEXT(...) #__VA_ARGS__

#define VAL_LIST(...) (struct mpv_node[]){__VA_ARGS__}

#define L(...) __VA_ARGS__

#define NODE_INT64(v) {.format = MPV_FORMAT_INT64,  .u = { .int64 = (v) }}
#define NODE_STR(v)   {.format = MPV_FORMAT_STRING, .u = { .string = (v) }}
#define NODE_BOOL(v)  {.format = MPV_FORMAT_FLAG,   .u = { .flag = (bool)(v) }}
#define NODE_FLOAT(v) {.format = MPV_FORMAT_DOUBLE, .u = { .double_ = (v) }}
#define NODE_NONE()   {.format = MPV_FORMAT_NONE }
#define NODE_ARRAY(...) {.format = MPV_FORMAT_NODE_ARRAY, .u = { .list =    \
    &(struct mpv_node_list) {                                               \
        .num = sizeof(VAL_LIST(__VA_ARGS__)) / sizeof(struct mpv_node),     \
        .values = VAL_LIST(__VA_ARGS__)}}}
#define NODE_MAP(k, v) {.format = MPV_FORMAT_NODE_MAP, .u = { .list =       \
    &(struct mpv_node_list) {                                               \
        .num = sizeof(VAL_LIST(v)) / sizeof(struct mpv_node),               \
        .values = VAL_LIST(v),                                              \
        .keys = (char**)(const char *[]){k}}}}

static const struct entry entries[] = {
    { "null", "null", NODE_NONE()},
    { "true", "true", NODE_BOOL(true)},
    { "false", "false", NODE_BOOL(false)},
    { "", .expect_fail = true},
    { "abc", .expect_fail = true},
    { "  123  ", "123", NODE_INT64(123)},
    { "123.25", "123.250000", NODE_FLOAT(123.25)},
    { TEXT("a\n\\"), TEXT("a\u000a\u005c"), NODE_STR("a\n\\")},
    { "[1,2,3]", "[1,2,3]",
        NODE_ARRAY(NODE_INT64(1), NODE_INT64(2), NODE_INT64(3))},
    { "[ ]", "[]", NODE_ARRAY()},
    { "[1,2,]", .expect_fail = true},
    { "[1,,2]", .expect_fail = true},
    { TEXT({"a":1, "b":2}), TEXT({"a":1,"b":2}),
        NODE_MAP(L("a", "b"), L(NODE_INT64(1), NODE_INT64(2)))},
    { "{ }", "{}", NODE_MAP(L(), L())},
    { TEXT({"a":b}), .expect_fail = true},
    { TEXT({a:"b"}), .expect_fail = true},
    { TEXT({"a":1,}), .expect_fail = true},
};

#define MAX_DEPTH 10

static void test_json(void **state)
{
    for (int n = 0; n < MP_ARRAY_SIZE(entries); n++) {
        const struct entry *e = &entries[n];
        print_message("%d: %s\n", n, e->src);
        void *tmp = talloc_new(NULL);
        char *s = talloc_strdup(tmp, e->src);
        json_skip_whitespace(&s);
        struct mpv_node res;
        bool ok = json_parse(tmp, &res, &s, MAX_DEPTH) >= 0;
        assert_true(ok != e->expect_fail);
        if (!ok)
            continue;
        char *d = talloc_strdup(tmp, "");
        assert_true(json_write(&d, &res) >= 0);
        assert_string_equal(e->out_txt, d);
        assert_true(equal_mpv_node(&e->out_data, &res));
        talloc_free(tmp);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_json),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

