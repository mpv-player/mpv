/*
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

#include <limits.h>
#include <stdint.h>

#include "common/common.h"
#include "misc/bstr.h"

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

static int lang_compare(const void *key, const void *lang)
{
    return bstrcasecmp0(*(const bstr*)key, ((const struct lang*)lang)->match);
}

static bstr canonicalize(bstr lang)
{
    const struct lang *l = bsearch(&lang, langmap, MP_ARRAY_SIZE(langmap),
                                   sizeof(langmap[0]), &lang_compare);
    return l ? bstr0(l->canonical) : lang;
}

int mp_match_lang(char **langs, const char *lang)
{
    if (!lang)
        return 0;

    void *ta_ctx = talloc_new(NULL);
    int lang_parts_n = 0;
    bstr *lang_parts = NULL;
    bstr rest = bstr0(lang);
    while (rest.len) {
        bstr s = bstr_split(rest, "-", &rest);
        MP_TARRAY_APPEND(ta_ctx, lang_parts, lang_parts_n, s);
    }

    int best_score = 0;
    if (!lang_parts_n)
        goto done;

    for (int idx = 0; langs && langs[idx]; idx++) {
        rest = bstr0(langs[idx]);
        int part = 0;
        int score = 0;
        while (rest.len) {
            bstr s = bstr_split(rest, "-", &rest);
            if (!part) {
                if (bstrcasecmp(canonicalize(lang_parts[0]), canonicalize(s)))
                    break;
                score = INT_MAX - idx;
                part++;
                continue;
            }

            if (part >= lang_parts_n)
                break;

            if (bstrcasecmp(lang_parts[part], s))
                score -= 1000;

            part++;
        }
        score -= (lang_parts_n - part) * 1000;
        best_score = MPMAX(best_score, score);
    }

done:
    talloc_free(ta_ctx);
    return best_score;
}
