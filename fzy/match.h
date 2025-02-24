/* The MIT License (MIT)
Copyright (c) 2020 Seth Warn
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* match.h
 * c interface to fzy matching
 *
 * original code by John Hawthorn, https://github.com/jhawthorn/fzy
 * modifications by
 *   Rom Grk, https://github.com/romgrk
 *   Seth Warn, https://github.com/swarn
 */

#ifndef FZY_NATIVE_H
#define FZY_NATIVE_H

#include <math.h>
#include <stdint.h>


typedef double score_t;
typedef uint32_t index_t;

#define SCORE_MAX INFINITY
#define SCORE_MIN (-INFINITY)
#define MATCH_MAX_LEN 1024


// Return true if `needle` is a subsequence of `haystack`.
//
// Control case sensitivity of matches with `case_sensitive`
int has_match(char const * needle, char const * haystack, int case_sensitive);


// Compute a matching score for two strings.
//
// Note: if `has_match(needle, haystack)` is not true, the return value
// is undefined.
//
// Returns a score measuring the quality of the match. Better matches get
// higher scores.
//
// - returns `SCORE_MIN` where `needle` or `haystack` are longer than
//   `MATCH_MAX_LEN`.
//
// - returns `SCORE_MIN` when `needle` or `haystack` are empty strings.
//
// - return `SCORE_MAX` when `strlen(needle) == strlen(haystack)`
score_t match(char const * needle, char const * haystack, int case_sensitive);


// Compute a matching score and the indices of matching characters.
//
// - The score is returned as in match()
//
// - `positions` is an array that will be filled in such that `positions[i]` is
//   the index of `haystack` where `needle[i]` matches in the optimal match.
score_t match_positions(
    char const * needle,
    char const * haystack,
    index_t * positions,
    int is_case_sensitive);


#endif

