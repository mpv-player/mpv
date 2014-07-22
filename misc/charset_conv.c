/*
 * This file is part of mpv.
 *
 * Based on code taken from libass (ISC license), which was originally part
 * of MPlayer (GPL).
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>

#include "config.h"

#include "common/msg.h"

#if HAVE_ENCA
#include <enca.h>
#endif

#if HAVE_LIBGUESS
#include <libguess.h>
#endif

#if HAVE_ICONV
#include <iconv.h>
#endif

#include "charset_conv.h"

bool mp_charset_is_utf8(const char *user_cp)
{
    return user_cp && (strcasecmp(user_cp, "utf8") == 0 ||
                       strcasecmp(user_cp, "utf-8") == 0);
}

// Split the string on ':' into components.
// out_arr is at least max entries long.
// Return number of out_arr entries filled.
static int split_colon(const char *user_cp, int max, bstr *out_arr)
{
    if (!user_cp || max < 1)
        return 0;

    int count = 0;
    while (1) {
        const char *next = strchr(user_cp, ':');
        if (next && max - count > 1) {
            out_arr[count++] = (bstr){(char *)user_cp, next - user_cp};
            user_cp = next + 1;
        } else {
            out_arr[count++] = (bstr){(char *)user_cp, strlen(user_cp)};
            break;
        }
    }
    return count;
}

// Returns true if user_cp implies that calling mp_charset_guess() on the
// input data is required to determine the real codepage. This is the case
// if user_cp is not a real iconv codepage, but a magic value that requests
// for example ENCA charset auto-detection.
bool mp_charset_requires_guess(const char *user_cp)
{
    bstr res[2] = {{0}};
    int r = split_colon(user_cp, 2, res);
    // Note that "utf8" is the UTF-8 codepage, while "utf8:..." specifies UTF-8
    // by default, plus a codepage that is used if the input is not UTF-8.
    return bstrcasecmp0(res[0], "enca") == 0 ||
           bstrcasecmp0(res[0], "auto") == 0 ||
           bstrcasecmp0(res[0], "guess") == 0 ||
           (r > 1 && bstrcasecmp0(res[0], "utf-8") == 0) ||
           (r > 1 && bstrcasecmp0(res[0], "utf8") == 0);
}

static const char *const utf_bom[3] = {"\xEF\xBB\xBF", "\xFF\xFE", "\xFE\xFF"};
static const char *const utf_enc[3] = {"utf-8",        "utf-16le", "utf-16be"};

static const char *ms_bom_guess(bstr buf)
{
    for (int n = 0; n < 3; n++) {
        if (bstr_startswith0(buf, utf_bom[n]))
            return utf_enc[n];
    }
    return NULL;
}

#if HAVE_ENCA
static const char *enca_guess(struct mp_log *log, bstr buf, const char *language)
{
    if (!language || !language[0])
        language = "__"; // neutral language

    const char *detected_cp = NULL;

    EncaAnalyser analyser = enca_analyser_alloc(language);
    if (analyser) {
        enca_set_termination_strictness(analyser, 0);
        EncaEncoding enc = enca_analyse_const(analyser, buf.start, buf.len);
        const char *tmp = enca_charset_name(enc.charset, ENCA_NAME_STYLE_ICONV);
        if (tmp && enc.charset != ENCA_CS_UNKNOWN)
            detected_cp = tmp;
        enca_analyser_free(analyser);
    } else {
        mp_err(log, "ENCA doesn't know language '%s'\n", language);
        size_t langcnt;
        const char **languages = enca_get_languages(&langcnt);
        mp_err(log, "ENCA supported languages:");
        for (int i = 0; i < langcnt; i++)
            mp_err(log, " %s", languages[i]);
        mp_err(log, "\n");
        free(languages);
    }

    return detected_cp;
}
#endif

#if HAVE_LIBGUESS
static const char *libguess_guess(struct mp_log *log, bstr buf,
                                  const char *language)
{
    if (!language || !language[0] || strcmp(language, "help") == 0) {
        mp_err(log, "libguess needs a language: "
               "japanese taiwanese chinese korean russian arabic turkish "
               "greek hebrew polish baltic\n");
        return NULL;
    }

    return libguess_determine_encoding(buf.start, buf.len, language);
}
#endif

// Runs charset auto-detection on the input buffer, and returns the result.
// If auto-detection fails, NULL is returned.
// If user_cp doesn't refer to any known auto-detection (for example because
// it's a real iconv codepage), user_cp is returned without even looking at
// the buf data.
const char *mp_charset_guess(struct mp_log *log, bstr buf, const char *user_cp,
                             int flags)
{
    if (!mp_charset_requires_guess(user_cp))
        return user_cp;

    bool use_auto = strcasecmp(user_cp, "auto") == 0;
    if (use_auto) {
#if HAVE_ENCA
        user_cp = "enca";
#else
        user_cp = "UTF-8:UTF-8-BROKEN";
#endif
    }

    // Do our own UTF-8 detection, because at least ENCA seems to get it
    // wrong sometimes (suggested by divVerent).
    int r = bstr_validate_utf8(buf);
    if (r >= 0 || (r > -8 && (flags & MP_ICONV_ALLOW_CUTOFF)))
        return "UTF-8";

    bstr params[3] = {{0}};
    split_colon(user_cp, 3, params);

    bstr type = params[0];
    char lang[100];
    snprintf(lang, sizeof(lang), "%.*s", BSTR_P(params[1]));
    const char *fallback = params[2].start; // last item, already 0-terminated

    const char *res = NULL;

    if (use_auto) {
        res = ms_bom_guess(buf);
        if (res)
            type = bstr0("auto");
    }

#if HAVE_ENCA
    if (bstrcasecmp0(type, "enca") == 0)
        res = enca_guess(log, buf, lang);
#endif
#if HAVE_LIBGUESS
    if (bstrcasecmp0(type, "guess") == 0)
        res = libguess_guess(log, buf, lang);
#endif
    if (bstrcasecmp0(type, "utf8") == 0 || bstrcasecmp0(type, "utf-8") == 0) {
        if (!fallback)
            fallback = params[1].start; // must be already 0-terminated
    }

    if (res) {
        mp_dbg(log, "%.*s detected charset: '%s'\n", BSTR_P(type), res);
    } else {
        res = fallback;
        mp_dbg(log, "Detection with %.*s failed: fallback to %s\n",
               BSTR_P(type), res && res[0] ? res : "broken UTF-8/Latin1");
    }

    if (!res && !(flags & MP_STRICT_UTF8))
        res = "UTF-8-BROKEN";

    return res;
}

// Convert the data in buf to UTF-8. The charset argument can be an iconv
// codepage, a value returned by mp_charset_conv_guess(), or a special value
// that triggers autodetection of the charset (e.g. using ENCA).
// The auto-detection is the only difference to mp_iconv_to_utf8().
//  buf: same as mp_iconv_to_utf8()
//  user_cp: iconv codepage, special value, NULL
//  flags: same as mp_iconv_to_utf8()
//  returns: same as mp_iconv_to_utf8()
bstr mp_charset_guess_and_conv_to_utf8(struct mp_log *log, bstr buf,
                                       const char *user_cp, int flags)
{
    return mp_iconv_to_utf8(log, buf, mp_charset_guess(log, buf, user_cp, flags),
                            flags);
}

// Use iconv to convert buf to UTF-8.
// Returns buf.start==NULL on error. Returns buf if cp is NULL, or if there is
// obviously no conversion required (e.g. if cp is "UTF-8").
// Returns a newly allocated buffer if conversion is done and succeeds. The
// buffer will be terminated with 0 for convenience (the terminating 0 is not
// included in the returned length).
// Free the returned buffer with talloc_free().
//  buf: input data
//  cp: iconv codepage (or NULL)
//  flags: combination of MP_ICONV_* flags
//  returns: buf (no conversion), .start==NULL (error), or allocated buffer
bstr mp_iconv_to_utf8(struct mp_log *log, bstr buf, const char *cp, int flags)
{
#if HAVE_ICONV
    if (!cp || !cp[0] || mp_charset_is_utf8(cp))
        return buf;

    if (strcasecmp(cp, "ASCII") == 0)
        return buf;

    if (strcasecmp(cp, "UTF-8-BROKEN") == 0)
        return bstr_sanitize_utf8_latin1(NULL, buf);

    iconv_t icdsc;
    if ((icdsc = iconv_open("UTF-8", cp)) == (iconv_t) (-1)) {
        if (flags & MP_ICONV_VERBOSE)
            mp_err(log, "Error opening iconv with codepage '%s'\n", cp);
        goto failure;
    }

    size_t size = buf.len;
    size_t osize = size;
    size_t ileft = size;
    size_t oleft = size - 1;

    char *outbuf = talloc_size(NULL, osize);
    char *ip = buf.start;
    char *op = outbuf;

    while (1) {
        int clear = 0;
        size_t rc;
        if (ileft)
            rc = iconv(icdsc, &ip, &ileft, &op, &oleft);
        else {
            clear = 1; // clear the conversion state and leave
            rc = iconv(icdsc, NULL, NULL, &op, &oleft);
        }
        if (rc == (size_t) (-1)) {
            if (errno == E2BIG) {
                size_t offset = op - outbuf;
                outbuf = talloc_realloc_size(NULL, outbuf, osize + size);
                op = outbuf + offset;
                osize += size;
                oleft += size;
            } else {
                if (errno == EINVAL && (flags & MP_ICONV_ALLOW_CUTOFF)) {
                    // This is intended for cases where the input buffer is cut
                    // at a random byte position. If this happens in the middle
                    // of the buffer, it should still be an error. We say it's
                    // fine if the error is within 10 bytes of the end.
                    if (ileft <= 10)
                        break;
                }
                if (flags & MP_ICONV_VERBOSE) {
                    mp_err(log, "Error recoding text with codepage '%s'\n", cp);
                }
                talloc_free(outbuf);
                iconv_close(icdsc);
                goto failure;
            }
        } else if (clear)
            break;
    }

    iconv_close(icdsc);

    outbuf[osize - oleft - 1] = 0;
    return (bstr){outbuf, osize - oleft - 1};
#endif

failure:
    return (bstr){0};
}
