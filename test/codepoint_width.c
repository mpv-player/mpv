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

#include "test_utils.h"

#include <limits.h>

#include "misc/codepoint_width.h"

#define W(s) term_disp_width((bstr)bstr0_lit(s), INT_MAX, &(const unsigned char *){NULL})

int main(void) {
    assert_int_equal(W("A"), 1);               // Single ASCII character
    assert_int_equal(W("ABC"), 3);             // Multiple ASCII characters

    assert_int_equal(W("\u3042"), 2);          // Full-width Japanese Hiragana 'あ' (U+3042)
    assert_int_equal(W("\u4F60"), 2);          // Full-width Chinese character '你' (U+4F60)
    assert_int_equal(W("\u4F60\u597D"), 4);    // Two full-width Chinese characters '你好' (U+4F60 U+597D)

    assert_int_equal(W("a\u0301"), 1);         // 'a' + combining acute accent '́' (U+0301)
    assert_int_equal(W("e\u0301"), 1);         // 'e' + combining acute accent '́' (U+0301)
    assert_int_equal(W("a\u0308"), 1);         // 'a' + combining diaeresis '̈' (U+0308)

    // Family emoji: 👩‍❤️‍👩 (Woman + ZWJ + Heart + ZWJ + Woman;
    // Code points: U+1F469 U+200D U+2764 U+FE0F U+200D U+1F469)
    assert_int_equal(W("\U0001F469\u200D\u2764\uFE0F\u200D\U0001F469"), 2);
    // Man emoji with skin tone modifier
    assert_int_equal(W("\U0001F468\U0001F3FE"), 2);
    // Person Shrugging + skin tone modifier + ZWJ + woman sign + variant selector
    assert_int_equal(W("\U0001F937\U0001F3FE\u200D\U00002640\U0000FE0F"), 2);
    // Regional indicator symbols forming the flag of Poland
    assert_int_equal(W("\U0001F1F5\U0001F1F1"), 2);

    assert_int_equal(W("\n"), 0);              // Newline (should not take up any visual space)
    assert_int_equal(W("\t"), 8);              // Tab (tabstop assumed to be 8)
    assert_int_equal(W("\0"), 0);              // Null character (non-visible)

    assert_int_equal(W("A\u3042"), 3);         // ASCII 'A' + full-width Japanese Hiragana 'あ' (U+3042)
    assert_int_equal(W("a\u0301A"), 2);        // Combining character 'á' (a + U+0301) and ASCII 'A'

    // Grapheme cluster + ASCII 'A' + full-width Japanese Hiragana 'あ' (U+3042)
    assert_int_equal(W("\U0001F469\u200D\u2764\uFE0F\u200D\U0001F469A\u3042"), 5);

    assert_int_equal(W("A\nB"), 2);            // ASCII characters with newline (newline should not affect width)
    assert_int_equal(W("ABC\tDEF"), 11);       // Tab inside a string

    // Single illegal character
    assert_int_equal(W("abc" "\xFF" "def"), 7);
    // Codepoint sequence cut off by one byte
    assert_int_equal(W("abc"  "\xF0\x9F\x98" "def"), 7);

    // Examples from https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/#G66453
    // Table 3-8. U+FFFD for Non-Shortest Form Sequences
    assert_int_equal(W("\xC0\xAF\xE0\x80\xBF\xF0\x81\x82\x41"), 9);
    // Table 3-9. U+FFFD for Ill-Formed Sequences for Surrogates
    assert_int_equal(W("\xED\xA0\x80\xED\xBF\xBF\xED\xAF\x41"), 9);
    // Table 3-10. U+FFFD for Other Ill-Formed Sequences
    assert_int_equal(W("\xF4\x91\x92\x93\xFF\x41\x80\xBF\x42"), 9);
    // Table 3-11. U+FFFD for Truncated Sequences
    assert_int_equal(W("\xE1\x80\xE2\xF0\x91\x92\xF1\xBF\x41"), 5);

    // ASCII characters with color
    assert_int_equal(W("\033[31mABC\033[0m\033[32mDEF\033[0m"), 6);

    // ASCII characters with color and a newline
    assert_int_equal(W("\033[31mABC\033[0m\033[32mDEF\033[0m\n"), 6);

    // ASCII characters with carriage return
    assert_int_equal(W("ABC\rDEF"), 3);

    bstr str = bstr0("ABCDEF");
    const unsigned char *cut_pos;

    cut_pos = NULL;
    assert_int_equal(term_disp_width(str, 3, &cut_pos), 3);
    assert_int_equal(cut_pos - str.start, 3);

    cut_pos = NULL;
    assert_int_equal(term_disp_width(str, -2, &cut_pos), 0);
    assert_int_equal(cut_pos - str.start, 0);

    cut_pos = NULL;
    assert_int_equal(term_disp_width(str, str.len, &cut_pos), 6);
    if (cut_pos) {
        printf("%s:%d: cut_pos != NULL\n", __FILE__, __LINE__);
        fflush(stdout);
        abort();
    }
}
