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

/* match.c
 * C implementation of fzy matching
 *
 * original code by John Hawthorn, https://github.com/jhawthorn/fzy
 * modifications by
 *   Rom Grk, https://github.com/romgrk
 *   Seth Warn, https://github.com/swarn
 */

#include "match.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bonus.h"


int has_match(char const * needle, char const * haystack, int case_sensitive)
{
    char needle_lower[MATCH_MAX_LEN + 1];
    char haystack_lower[MATCH_MAX_LEN + 1];

    if (! case_sensitive)
    {
        int const n = (int)strlen(needle);
        int const m = (int)strlen(haystack);

        for (int i = 0; i < n; i++)
            needle_lower[i] = (char)tolower(needle[i]);
        for (int i = 0; i < m; i++)
            haystack_lower[i] = (char)tolower(haystack[i]);

        needle_lower[n] = 0;
        haystack_lower[m] = 0;

        needle = needle_lower;
        haystack = haystack_lower;
    }

    while (*needle)
    {
        haystack = strchr(haystack, *needle++);
        if (! haystack)
            return 0;

        haystack++;
    }
    return 1;
}


#define SWAP(x, y, T)                                                                  \
    do                                                                                 \
    {                                                                                  \
        T SWAP = x;                                                                    \
        (x) = y;                                                                       \
        (y) = SWAP;                                                                    \
    } while (0)

#define max(a, b) (((a) > (b)) ? (a) : (b))


struct match_struct
{
    int needle_len;
    int haystack_len;

    char const * needle;
    char const * haystack;

    char lower_needle[MATCH_MAX_LEN];
    char lower_haystack[MATCH_MAX_LEN];

    score_t match_bonus[MATCH_MAX_LEN];
};


static void precompute_bonus(char const * haystack, score_t * match_bonus)
{
    /* Which positions are beginning of words */
    char last_ch = '/';
    for (int i = 0; haystack[i]; i++)
    {
        char ch = haystack[i];
        match_bonus[i] = COMPUTE_BONUS(last_ch, ch);
        last_ch = ch;
    }
}

static void setup_match_struct(
    struct match_struct * match,
    char const * needle,
    char const * haystack,
    int is_case_sensitive)
{
    match->needle_len = (int)strlen(needle);
    match->haystack_len = (int)strlen(haystack);

    if (match->haystack_len > MATCH_MAX_LEN || match->needle_len > match->haystack_len)
    {
        return;
    }

    if (is_case_sensitive)
    {
        match->needle = needle;
        match->haystack = haystack;
    }
    else
    {
        for (int i = 0; i < match->needle_len; i++)
            match->lower_needle[i] = (char)tolower(needle[i]);

        for (int i = 0; i < match->haystack_len; i++)
            match->lower_haystack[i] = (char)tolower(haystack[i]);

        match->needle = match->lower_needle;
        match->haystack = match->lower_haystack;
    }

    precompute_bonus(haystack, match->match_bonus);
}

static inline void match_row(
    struct match_struct const * match,
    int row,
    score_t * curr_D,
    score_t * curr_M,
    score_t const * last_D,
    score_t const * last_M)
{
    unsigned n = match->needle_len;
    unsigned m = match->haystack_len;
    int i = row;

    char const * needle = match->needle;
    char const * haystack = match->haystack;
    score_t const * match_bonus = match->match_bonus;

    score_t prev_score = SCORE_MIN;
    score_t gap_score = i == n - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

    for (int j = 0; j < m; j++)
    {
        if (needle[i] == haystack[j])
        {
            score_t score = SCORE_MIN;
            if (i == 0)
            {
                // The match_bonus values are computed out to the length of the
                // haystack in precompute_bonus. The index j is less than m,
                // the length of the haystack. So the "garbage value" warning
                // here is false.
                // NOLINTNEXTLINE(clang-analyzer-core.UndefinedBinaryOperatorResult)
                score = (j * SCORE_GAP_LEADING) + match_bonus[j];
            }
            else if (j)
            {
                /* i > 0 && j > 0*/
                score =
                    // NOLINTNEXTLINE(clang-analyzer-core.UndefinedBinaryOperatorResult)
                    max(last_M[j - 1] + match_bonus[j],

                        /* consecutive match, doesn't stack with match_bonus */
                        last_D[j - 1] + SCORE_MATCH_CONSECUTIVE);
            }
            curr_D[j] = score;
            curr_M[j] = prev_score = max(score, prev_score + gap_score);
        }
        else
        {
            curr_D[j] = SCORE_MIN;
            curr_M[j] = prev_score = prev_score + gap_score;
        }
    }
}

score_t match(char const * needle, char const * haystack, int case_sensitive)
{
    if (! *needle)
        return SCORE_MIN;

    struct match_struct match;
    setup_match_struct(&match, needle, haystack, case_sensitive);

    unsigned n = match.needle_len;
    unsigned m = match.haystack_len;

    // Unreasonably large candidate; return no score. If it is a valid match,
    // it will still be returned, it will just be ranked below any reasonably
    // sized candidates.
    if (m > MATCH_MAX_LEN || n > m)
        return SCORE_MIN;

    // If `needle` is a subsequence of `haystack` and the same length, then
    // they are the same string.
    if (n == m)
        return SCORE_MAX;

    // D[][] Stores the best score for this position ending with a match.
    // M[][] Stores the best possible score at this position.
    score_t D[2][MATCH_MAX_LEN];
    score_t M[2][MATCH_MAX_LEN];

    score_t * last_D = D[0];
    score_t * last_M = M[0];
    score_t * curr_D = D[1];
    score_t * curr_M = M[1];

    for (int i = 0; i < n; i++)
    {
        match_row(&match, i, curr_D, curr_M, last_D, last_M);

        SWAP(curr_D, last_D, score_t *);
        SWAP(curr_M, last_M, score_t *);
    }

    return last_M[m - 1];
}

score_t match_positions(
    char const * needle,
    char const * haystack,
    index_t * positions,
    int is_case_sensitive)
{
    if (! *needle)
        return SCORE_MIN;

    struct match_struct match;
    setup_match_struct(&match, needle, haystack, is_case_sensitive);

    int n = match.needle_len;
    int m = match.haystack_len;

    // Unreasonably large candidate; return no score. If it is a valid match,
    // it will still be returned, it will just be ranked below any reasonably
    // sized candidates
    if (m > MATCH_MAX_LEN || n > m)
        return SCORE_MIN;

    // If `needle` is a subsequence of `haystack` and the same length, then
    // they are the same string.
    if (n == m)
    {
        if (positions)
            for (int i = 0; i < n; i++)
                positions[i] = i;

        return SCORE_MAX;
    }

    // D[][] Stores the best score for this position ending with a match.
    // M[][] Stores the best possible score at this position.
    typedef score_t score_row_t[MATCH_MAX_LEN];
    score_row_t * const D = malloc(sizeof(score_row_t) * n);
    score_row_t * const M = malloc(sizeof(score_row_t) * n);

    score_t * last_D = NULL;
    score_t * last_M = NULL;
    score_t * curr_D = NULL;
    score_t * curr_M = NULL;

    for (int i = 0; i < n; i++)
    {
        curr_D = &D[i][0];
        curr_M = &M[i][0];

        match_row(&match, i, curr_D, curr_M, last_D, last_M);

        last_D = curr_D;
        last_M = curr_M;
    }

    /* backtrace to find the positions of optimal matching */
    int match_required = 0;
    for (int i = n - 1, j = m - 1; i >= 0; i--)
    {
        for (; j >= 0; j--)
        {
            // There may be multiple paths which result in the optimal
            // weight.
            //
            // For simplicity, we will pick the first one we encounter,
            // the latest in the candidate string.
            // NOLINTNEXTLINE(clang-analyzer-core.UndefinedBinaryOperatorResult)
            if (D[i][j] != SCORE_MIN && (match_required || D[i][j] == M[i][j]))
            {
                // If this score was determined using SCORE_MATCH_CONSECUTIVE,
                // the previous character MUST be a match
                match_required =
                    i && j && M[i][j] == D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE;

                if (positions)
                    positions[i] = j--;

                break;
            }
        }
    }

    score_t result = M[n - 1][m - 1];

    free(M);
    free(D);

    return result;
}

