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

#define L(s) { #s, sizeof(#s) - 1 }

static const struct lang {
    struct { const char s[3]; uint8_t l; } match;
    struct { const char s[3]; uint8_t l; } canonical;
} langmap[] = {
    {L(aa), L(aar)},
    {L(ab), L(abk)},
    {L(ae), L(ave)},
    {L(af), L(afr)},
    {L(ak), L(aka)},
    {L(am), L(amh)},
    {L(an), L(arg)},
    {L(ar), L(ara)},
    {L(as), L(asm)},
    {L(av), L(ava)},
    {L(ay), L(aym)},
    {L(az), L(aze)},
    {L(ba), L(bak)},
    {L(be), L(bel)},
    {L(bg), L(bul)},
    {L(bh), L(bih)},
    {L(bi), L(bis)},
    {L(bm), L(bam)},
    {L(bn), L(ben)},
    {L(bo), L(tib)},
    {L(bod), L(tib)},
    {L(br), L(bre)},
    {L(bs), L(bos)},
    {L(ca), L(cat)},
    {L(ce), L(che)},
    {L(ces), L(cze)},
    {L(ch), L(cha)},
    {L(co), L(cos)},
    {L(cr), L(cre)},
    {L(cs), L(cze)},
    {L(cu), L(chu)},
    {L(cv), L(chv)},
    {L(cy), L(wel)},
    {L(cym), L(wel)},
    {L(da), L(dan)},
    {L(de), L(ger)},
    {L(deu), L(ger)},
    {L(dv), L(div)},
    {L(dz), L(dzo)},
    {L(ee), L(ewe)},
    {L(el), L(gre)},
    {L(ell), L(gre)},
    {L(en), L(eng)},
    {L(eo), L(epo)},
    {L(es), L(spa)},
    {L(et), L(est)},
    {L(eu), L(baq)},
    {L(eus), L(baq)},
    {L(fa), L(per)},
    {L(fas), L(per)},
    {L(ff), L(ful)},
    {L(fi), L(fin)},
    {L(fj), L(fij)},
    {L(fo), L(fao)},
    {L(fr), L(fre)},
    {L(fra), L(fre)},
    {L(fy), L(fry)},
    {L(ga), L(gle)},
    {L(gd), L(gla)},
    {L(gl), L(glg)},
    {L(gn), L(grn)},
    {L(gu), L(guj)},
    {L(gv), L(glv)},
    {L(ha), L(hau)},
    {L(he), L(heb)},
    {L(hi), L(hin)},
    {L(ho), L(hmo)},
    {L(hr), L(hrv)},
    {L(ht), L(hat)},
    {L(hu), L(hun)},
    {L(hy), L(arm)},
    {L(hye), L(arm)},
    {L(hz), L(her)},
    {L(ia), L(ina)},
    {L(id), L(ind)},
    {L(ie), L(ile)},
    {L(ig), L(ibo)},
    {L(ii), L(iii)},
    {L(ik), L(ipk)},
    {L(io), L(ido)},
    {L(is), L(ice)},
    {L(isl), L(ice)},
    {L(it), L(ita)},
    {L(iu), L(iku)},
    {L(ja), L(jpn)},
    {L(jv), L(jav)},
    {L(ka), L(geo)},
    {L(kat), L(geo)},
    {L(kg), L(kon)},
    {L(ki), L(kik)},
    {L(kj), L(kua)},
    {L(kk), L(kaz)},
    {L(kl), L(kal)},
    {L(km), L(khm)},
    {L(kn), L(kan)},
    {L(ko), L(kor)},
    {L(kr), L(kau)},
    {L(ks), L(kas)},
    {L(ku), L(kur)},
    {L(kv), L(kom)},
    {L(kw), L(cor)},
    {L(ky), L(kir)},
    {L(la), L(lat)},
    {L(lb), L(ltz)},
    {L(lg), L(lug)},
    {L(li), L(lim)},
    {L(ln), L(lin)},
    {L(lo), L(lao)},
    {L(lt), L(lit)},
    {L(lu), L(lub)},
    {L(lv), L(lav)},
    {L(mg), L(mlg)},
    {L(mh), L(mah)},
    {L(mi), L(mao)},
    {L(mk), L(mac)},
    {L(mkd), L(mac)},
    {L(ml), L(mal)},
    {L(mn), L(mon)},
    {L(mr), L(mar)},
    {L(mri), L(mao)},
    {L(ms), L(may)},
    {L(msa), L(may)},
    {L(mt), L(mlt)},
    {L(my), L(bur)},
    {L(mya), L(bur)},
    {L(na), L(nau)},
    {L(nb), L(nob)},
    {L(nd), L(nde)},
    {L(ne), L(nep)},
    {L(ng), L(ndo)},
    {L(nl), L(dut)},
    {L(nld), L(dut)},
    {L(nn), L(nno)},
    {L(no), L(nor)},
    {L(nr), L(nbl)},
    {L(nv), L(nav)},
    {L(ny), L(nya)},
    {L(oc), L(oci)},
    {L(oj), L(oji)},
    {L(om), L(orm)},
    {L(or), L(ori)},
    {L(os), L(oss)},
    {L(pa), L(pan)},
    {L(pi), L(pli)},
    {L(pl), L(pol)},
    {L(ps), L(pus)},
    {L(pt), L(por)},
    {L(qu), L(que)},
    {L(rm), L(roh)},
    {L(rn), L(run)},
    {L(ro), L(rum)},
    {L(ron), L(rum)},
    {L(ru), L(rus)},
    {L(rw), L(kin)},
    {L(sa), L(san)},
    {L(sc), L(srd)},
    {L(sd), L(snd)},
    {L(se), L(sme)},
    {L(sg), L(sag)},
    {L(si), L(sin)},
    {L(sk), L(slo)},
    {L(sl), L(slv)},
    {L(slk), L(slo)},
    {L(sm), L(smo)},
    {L(sn), L(sna)},
    {L(so), L(som)},
    {L(sq), L(alb)},
    {L(sqi), L(alb)},
    {L(sr), L(srp)},
    {L(ss), L(ssw)},
    {L(st), L(sot)},
    {L(su), L(sun)},
    {L(sv), L(swe)},
    {L(sw), L(swa)},
    {L(ta), L(tam)},
    {L(te), L(tel)},
    {L(tg), L(tgk)},
    {L(th), L(tha)},
    {L(ti), L(tir)},
    {L(tk), L(tuk)},
    {L(tl), L(tgl)},
    {L(tn), L(tsn)},
    {L(to), L(ton)},
    {L(tr), L(tur)},
    {L(ts), L(tso)},
    {L(tt), L(tat)},
    {L(tw), L(twi)},
    {L(ty), L(tah)},
    {L(ug), L(uig)},
    {L(uk), L(ukr)},
    {L(ur), L(urd)},
    {L(uz), L(uzb)},
    {L(ve), L(ven)},
    {L(vi), L(vie)},
    {L(vo), L(vol)},
    {L(wa), L(wln)},
    {L(wo), L(wol)},
    {L(xh), L(xho)},
    {L(yi), L(yid)},
    {L(yo), L(yor)},
    {L(za), L(zha)},
    {L(zh), L(chi)},
    {L(zho), L(chi)},
    {L(zu), L(zul)},
};

static int lang_compare(const void *key, const void *lang)
{
    const struct lang *l = lang;
    return bstrcasecmp(*(const bstr*)key, (bstr){(unsigned char *)l->match.s, l->match.l});
}

static bstr canonicalize(bstr lang)
{
    const struct lang *l = bsearch(&lang, langmap, MP_ARRAY_SIZE(langmap),
                                   sizeof(langmap[0]), &lang_compare);
    return l ? (bstr){(unsigned char *)l->canonical.s, l->canonical.l} : lang;
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
