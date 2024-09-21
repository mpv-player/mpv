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

#include "misc/codepoint_width.h"

#define W(s) term_disp_width((bstr)bstr0_lit(s))

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
    assert_int_equal(W("\t"), 0);              // Tab (no visual width itself)
    assert_int_equal(W("\0"), 0);              // Null character (non-visible)

    assert_int_equal(W("A\u3042"), 3);         // ASCII 'A' + full-width Japanese Hiragana 'あ' (U+3042)
    assert_int_equal(W("a\u0301A"), 2);        // Combining character 'á' (a + U+0301) and ASCII 'A'

    // Grapheme cluster + ASCII 'A' + full-width Japanese Hiragana 'あ' (U+3042)
    assert_int_equal(W("\U0001F469\u200D\u2764\uFE0F\u200D\U0001F469A\u3042"), 5);

    assert_int_equal(W("A\nB"), 2);            // ASCII characters with newline (newline should not affect width)
    assert_int_equal(W("ABC\tDEF"), 6);        // Tab inside a string (no visual width for '\t')

    // ASCII characters with color
    assert_int_equal(W("\033[31mABC\033[0m\033[32mDEF\033[0m"), 6);

    // ASCII characters with color and a newline
    assert_int_equal(W("\033[31mABC\033[0m\033[32mDEF\033[0m\n"), 6);

    // ASCII characters with carriage return
    assert_int_equal(W("ABC\rDEF"), 3);
}
