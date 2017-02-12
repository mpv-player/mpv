#ifndef MP_CHARSET_CONV_H
#define MP_CHARSET_CONV_H

#include <stdbool.h>
#include "misc/bstr.h"

struct mp_log;

enum {
    MP_ICONV_VERBOSE = 1,       // print errors instead of failing silently
    MP_ICONV_ALLOW_CUTOFF = 2,  // allow partial input data
    MP_STRICT_UTF8 = 4,         // don't fall back to UTF-8-BROKEN when guessing
    MP_NO_LATIN1_FALLBACK = 8,  // fall back to input buffer instead of latin1
};

bool mp_charset_is_utf8(const char *user_cp);
bool mp_charset_is_utf16(const char *user_cp);
const char *mp_charset_guess(void *talloc_ctx, struct mp_log *log, bstr buf,
                             const char *user_cp, int flags);
bstr mp_iconv_to_utf8(struct mp_log *log, bstr buf, const char *cp, int flags);

#endif
