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

#ifndef MP_CONTENT_RECOGNITION_H
#define MP_CONTENT_RECOGNITION_H

#include <stdbool.h>

// Content rating categories returned by the recognition model
enum mp_content_rating {
    MP_CONTENT_RATING_UNKNOWN = 0,
    MP_CONTENT_RATING_G,           // General audiences
    MP_CONTENT_RATING_PG,          // Parental guidance suggested
    MP_CONTENT_RATING_PG13,        // Parents strongly cautioned
    MP_CONTENT_RATING_R,           // Restricted
    MP_CONTENT_RATING_NC17,        // Adults only
};

// Content flags detected by frame analysis
enum mp_content_flags {
    MP_CONTENT_FLAG_NONE        = 0,
    MP_CONTENT_FLAG_VIOLENCE    = 1 << 0,
    MP_CONTENT_FLAG_LANGUAGE    = 1 << 1,
    MP_CONTENT_FLAG_NUDITY      = 1 << 2,
    MP_CONTENT_FLAG_DRUG_USE    = 1 << 3,
    MP_CONTENT_FLAG_GORE        = 1 << 4,
    MP_CONTENT_FLAG_HORROR      = 1 << 5,
    MP_CONTENT_FLAG_GAMBLING    = 1 << 6,
};

// Result of content analysis for a given frame or segment
struct mp_content_result {
    enum mp_content_rating rating;
    unsigned int flags;             // bitmask of mp_content_flags
    float confidence;               // 0.0 - 1.0
};

// Returns the minimum allowed age for a given content rating
static inline int mp_content_rating_min_age(enum mp_content_rating rating)
{
    switch (rating) {
    case MP_CONTENT_RATING_G:    return 0;
    case MP_CONTENT_RATING_PG:   return 7;
    case MP_CONTENT_RATING_PG13: return 13;
    case MP_CONTENT_RATING_R:    return 17;
    case MP_CONTENT_RATING_NC17: return 18;
    default:                     return 0;
    }
}

// Returns a human-readable string for a content rating
static inline const char *mp_content_rating_str(enum mp_content_rating rating)
{
    switch (rating) {
    case MP_CONTENT_RATING_G:    return "G";
    case MP_CONTENT_RATING_PG:   return "PG";
    case MP_CONTENT_RATING_PG13: return "PG-13";
    case MP_CONTENT_RATING_R:    return "R";
    case MP_CONTENT_RATING_NC17: return "NC-17";
    default:                     return "UNKNOWN";
    }
}

// Returns a human-readable description of content flags
static inline const char *mp_content_flags_str(unsigned int flags)
{
    if (flags & MP_CONTENT_FLAG_NUDITY)   return "nudity";
    if (flags & MP_CONTENT_FLAG_GORE)     return "graphic violence";
    if (flags & MP_CONTENT_FLAG_VIOLENCE) return "violence";
    if (flags & MP_CONTENT_FLAG_DRUG_USE) return "substance use";
    if (flags & MP_CONTENT_FLAG_LANGUAGE) return "strong language";
    if (flags & MP_CONTENT_FLAG_HORROR)   return "horror themes";
    if (flags & MP_CONTENT_FLAG_GAMBLING) return "gambling";
    return "none";
}

// Returns true if the content should be blocked for the given user age
static inline bool mp_content_should_block(struct mp_content_result *result,
                                           int user_age, int filter_mode)
{
    if (!result || result->rating == MP_CONTENT_RATING_UNKNOWN)
        return false;

    int min_age = mp_content_rating_min_age(result->rating);

    // In strict mode, use lower confidence threshold (more aggressive blocking)
    float threshold = (filter_mode == 2) ? 0.5f : 0.75f;

    if (result->confidence < threshold)
        return false;

    return user_age < min_age;
}

#endif
