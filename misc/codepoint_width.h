#pragma once

#include "misc/bstr.h"

/**
 * @brief Determines the number of columns required to display a given string.
 *
 * @param str Sequence of UTF-8 chars
 * @return int width of the string
 */
int term_disp_width(bstr str);
