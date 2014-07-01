#ifndef MP_CTYPE_H_
#define MP_CTYPE_H_

// Roughly follows C semantics, but doesn't account for EOF, allows char as
// parameter, and is locale independent (always uses "C" locale).

static inline int mp_isprint(char c) { return (unsigned char)c >= 32; }
static inline int mp_isspace(char c) { return c == ' ' || c == '\f' || c == '\n' ||
                                              c == '\r' || c == '\t' || c =='\v'; }
static inline int mp_isupper(char c) { return c >= 'A' && c <= 'Z'; }
static inline int mp_islower(char c) { return c >= 'a' && c <= 'z'; }
static inline int mp_isdigit(char c) { return c >= '0' && c <= '9'; }
static inline int mp_isalpha(char c) { return mp_isupper(c) || mp_islower(c); }
static inline int mp_isalnum(char c) { return mp_isalpha(c) || mp_isdigit(c); }

static inline char mp_tolower(char c) { return mp_isupper(c) ? c - 'A' + 'a' : c; }
static inline char mp_toupper(char c) { return mp_islower(c) ? c - 'a' + 'A' : c; }

#endif
