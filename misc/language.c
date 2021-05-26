/*
 * Language code utility functions
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "language.h"

#include "common/common.h"
#include "osdep/strnlen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const struct lang {
    char match[4];
    char canonical[4];
} langmap[] = {
    {"aa", "aar"},
    {"ab", "abk"},
    {"ae", "ave"},
    {"af", "afr"},
    {"ak", "aka"},
    {"am", "amh"},
    {"an", "arg"},
    {"ar", "ara"},
    {"as", "asm"},
    {"av", "ava"},
    {"ay", "aym"},
    {"az", "aze"},
    {"ba", "bak"},
    {"be", "bel"},
    {"bg", "bul"},
    {"bh", "bih"},
    {"bi", "bis"},
    {"bm", "bam"},
    {"bn", "ben"},
    {"bo", "tib"},
    {"bod", "tib"},
    {"br", "bre"},
    {"bs", "bos"},
    {"ca", "cat"},
    {"ce", "che"},
    {"ces", "cze"},
    {"ch", "cha"},
    {"co", "cos"},
    {"cr", "cre"},
    {"cs", "cze"},
    {"cu", "chu"},
    {"cv", "chv"},
    {"cy", "wel"},
    {"cym", "wel"},
    {"da", "dan"},
    {"de", "ger"},
    {"deu", "ger"},
    {"dv", "div"},
    {"dz", "dzo"},
    {"ee", "ewe"},
    {"el", "gre"},
    {"ell", "gre"},
    {"en", "eng"},
    {"eo", "epo"},
    {"es", "spa"},
    {"et", "est"},
    {"eu", "baq"},
    {"eus", "baq"},
    {"fa", "per"},
    {"fas", "per"},
    {"ff", "ful"},
    {"fi", "fin"},
    {"fj", "fij"},
    {"fo", "fao"},
    {"fr", "fre"},
    {"fra", "fre"},
    {"fy", "fry"},
    {"ga", "gle"},
    {"gd", "gla"},
    {"gl", "glg"},
    {"gn", "grn"},
    {"gu", "guj"},
    {"gv", "glv"},
    {"ha", "hau"},
    {"he", "heb"},
    {"hi", "hin"},
    {"ho", "hmo"},
    {"hr", "hrv"},
    {"ht", "hat"},
    {"hu", "hun"},
    {"hy", "arm"},
    {"hye", "arm"},
    {"hz", "her"},
    {"ia", "ina"},
    {"id", "ind"},
    {"ie", "ile"},
    {"ig", "ibo"},
    {"ii", "iii"},
    {"ik", "ipk"},
    {"io", "ido"},
    {"is", "ice"},
    {"isl", "ice"},
    {"it", "ita"},
    {"iu", "iku"},
    {"ja", "jpn"},
    {"jv", "jav"},
    {"ka", "geo"},
    {"kat", "geo"},
    {"kg", "kon"},
    {"ki", "kik"},
    {"kj", "kua"},
    {"kk", "kaz"},
    {"kl", "kal"},
    {"km", "khm"},
    {"kn", "kan"},
    {"ko", "kor"},
    {"kr", "kau"},
    {"ks", "kas"},
    {"ku", "kur"},
    {"kv", "kom"},
    {"kw", "cor"},
    {"ky", "kir"},
    {"la", "lat"},
    {"lb", "ltz"},
    {"lg", "lug"},
    {"li", "lim"},
    {"ln", "lin"},
    {"lo", "lao"},
    {"lt", "lit"},
    {"lu", "lub"},
    {"lv", "lav"},
    {"mg", "mlg"},
    {"mh", "mah"},
    {"mi", "mao"},
    {"mk", "mac"},
    {"mkd", "mac"},
    {"ml", "mal"},
    {"mn", "mon"},
    {"mr", "mar"},
    {"mri", "mao"},
    {"ms", "may"},
    {"msa", "may"},
    {"mt", "mlt"},
    {"my", "bur"},
    {"mya", "bur"},
    {"na", "nau"},
    {"nb", "nob"},
    {"nd", "nde"},
    {"ne", "nep"},
    {"ng", "ndo"},
    {"nl", "dut"},
    {"nld", "dut"},
    {"nn", "nno"},
    {"no", "nor"},
    {"nr", "nbl"},
    {"nv", "nav"},
    {"ny", "nya"},
    {"oc", "oci"},
    {"oj", "oji"},
    {"om", "orm"},
    {"or", "ori"},
    {"os", "oss"},
    {"pa", "pan"},
    {"pi", "pli"},
    {"pl", "pol"},
    {"ps", "pus"},
    {"pt", "por"},
    {"qu", "que"},
    {"rm", "roh"},
    {"rn", "run"},
    {"ro", "rum"},
    {"ron", "rum"},
    {"ru", "rus"},
    {"rw", "kin"},
    {"sa", "san"},
    {"sc", "srd"},
    {"sd", "snd"},
    {"se", "sme"},
    {"sg", "sag"},
    {"si", "sin"},
    {"sk", "slo"},
    {"sl", "slv"},
    {"slk", "slo"},
    {"sm", "smo"},
    {"sn", "sna"},
    {"so", "som"},
    {"sq", "alb"},
    {"sqi", "alb"},
    {"sr", "srp"},
    {"ss", "ssw"},
    {"st", "sot"},
    {"su", "sun"},
    {"sv", "swe"},
    {"sw", "swa"},
    {"ta", "tam"},
    {"te", "tel"},
    {"tg", "tgk"},
    {"th", "tha"},
    {"ti", "tir"},
    {"tk", "tuk"},
    {"tl", "tgl"},
    {"tn", "tsn"},
    {"to", "ton"},
    {"tr", "tur"},
    {"ts", "tso"},
    {"tt", "tat"},
    {"tw", "twi"},
    {"ty", "tah"},
    {"ug", "uig"},
    {"uk", "ukr"},
    {"ur", "urd"},
    {"uz", "uzb"},
    {"ve", "ven"},
    {"vi", "vie"},
    {"vo", "vol"},
    {"wa", "wln"},
    {"wo", "wol"},
    {"xh", "xho"},
    {"yi", "yid"},
    {"yo", "yor"},
    {"za", "zha"},
    {"zh", "chi"},
    {"zho", "chi"},
    {"zu", "zul"},
};

struct langsearch {
    const char *str;
    size_t size;
};

static int lang_compare(const void *s, const void *k)
{
    const struct langsearch *search = s;
    const struct lang *key = k;

    int ret = strncasecmp(search->str, key->match, search->size);
    if (!ret && search->size < sizeof(key->match) && key->match[search->size])
        return 1;
    return ret;
}

static void canonicalize(const char **lang, size_t *size)
{
    if (*size > sizeof(langmap[0].match))
        return;

    struct langsearch search = {*lang, *size};
    struct lang *l = bsearch(&search, langmap, MP_ARRAY_SIZE(langmap), sizeof(langmap[0]),
                             &lang_compare);

    if (l) {
        *lang = l->canonical;
        *size = strnlen(l->canonical, sizeof(l->canonical));
    }
}

static bool tag_matches(const char *l1, size_t s1, const char *l2, size_t s2)
{
    return s1 == s2 && !strncasecmp(l1, l2, s1);
}

int mp_match_lang_single(const char *l1, const char *l2)
{
    // We never consider null or empty strings to match
    if (!l1 || !l2 || !*l1 || !*l2)
        return 0;

    // The first subtag should always be a language; canonicalize to 3-letter ISO 639-2B (arbitrarily chosen)
    size_t s1 = strcspn(l1, "-_");
    size_t s2 = strcspn(l2, "-_");

    const char *l1c = l1;
    const char *l2c = l2;
    size_t s1c = s1;
    size_t s2c = s2;

    canonicalize(&l1c, &s1c);
    canonicalize(&l2c, &s2c);

    // If the first subtags don't match, we have no match at all
    if (!tag_matches(l1c, s1c, l2c, s2c))
        return 0;

    // Attempt to match each subtag in each string against each in the other
    int score = 1;
    bool x1 = false;
    int count = 0;
    for (;;) {
        l1 += s1;

        while (*l1 == '-' || *l1 == '_')
            l1++;

        if (!*l1)
            break;

        s1 = strcspn(l1, "-_");
        if (tag_matches(l1, s1, "x", 1)) {
            x1 = true;
            continue;
        }

        const char *l2o = l2;
        size_t s2o = s2;
        bool x2 = false;
        for (;;) {
            l2 += s2;

            while (*l2 == '-' || *l2 == '_')
                l2++;

            if (!*l2)
                break;

            s2 = strcspn(l2, "-_");
            if (tag_matches(l2, s2, "x", 1)) {
                x2 = true;
                if (!x1)
                    break;
                continue;
            }

            // Private-use subtags only match against other private-use subtags
            if (x1 && !x2)
                continue;

            if (tag_matches(l1c, s1c, l2c, s2c)) {
                // Matches for subtags earlier in the user's string take priority over later ones,
                // for up to LANGUAGE_SCORE_BITS subtags
                int shift = (LANGUAGE_SCORE_BITS - count - 1);
                if (shift < 0)
                    shift = 0;
                score += (1 << shift);

                if (score >= LANGUAGE_SCORE_MAX)
                    return LANGUAGE_SCORE_MAX;
            }
        }

        l2 = l2o;
        s2 = s2o;

        count++;
    }

    return score;
}
