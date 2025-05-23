#pragma once

#include "misc/bstr.h"

/**
 * @brief Determines the number of columns required to display a given string.
 *
 * @param str Sequence of UTF-8 chars
 * @param max_width Maximum allowed width of string
 * @param cut_pos If max_width is exceeded, this will be initialized to last
 *                full printable character before width limit.
 * @return int width of the string
 */
int term_disp_width(bstr str, int max_width, const unsigned char **cut_pos);
