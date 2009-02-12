#ifdef CREATE_STRUCT_DEFINITIONS
#undef CREATE_STRUCT_DEFINITIONS
#define START(funcname, structname) \
    typedef struct structname {
#define GENERIC(type, member) \
        type member;
#define FTVECTOR(member) \
        FT_Vector member;
#define END(typedefnamename) \
    } typedefnamename;

#elif defined(CREATE_COMPARISON_FUNCTIONS)
#undef CREATE_COMPARISON_FUNCTIONS
#define START(funcname, structname) \
    static int funcname##_compare(void *key1, void *key2, size_t key_size) \
    { \
        struct structname *a = key1; \
        struct structname *b = key2; \
        return // conditions follow
#define GENERIC(type, member) \
            a->member == b->member &&
#define FTVECTOR(member) \
            a->member.x == b->member.x && a->member.y == b->member.y &&
#define END(typedefname) \
            1; \
    }

#elif defined(CREATE_HASH_FUNCTIONS)
#undef CREATE_HASH_FUNCTIONS
#define START(funcname, structname) \
    static unsigned funcname##_hash(void *buf, size_t len) \
    { \
        struct structname *p = buf; \
        unsigned hval = FNV1_32A_INIT;
#define GENERIC(type, member) \
        hval = fnv_32a_buf(&p->member, sizeof(p->member), hval);
#define FTVECTOR(member) GENERIC(, member.x); GENERIC(, member.y);
#define END(typedefname) \
        return hval; \
    }

#else
#error missing defines
#endif



// describes a bitmap; bitmaps with equivalents structs are considered identical
START(bitmap, bipmap_hash_key_s)
    GENERIC(char, bitmap) // bool : true = bitmap, false = outline
    GENERIC(ass_font_t *, font)
    GENERIC(double, size) // font size
    GENERIC(uint32_t, ch) // character code
    GENERIC(unsigned, outline) // border width, 16.16 fixed point value
    GENERIC(int, bold)
    GENERIC(int, italic)
    GENERIC(char, be) // blur edges
    GENERIC(double, blur) // gaussian blur
    GENERIC(unsigned, scale_x) // 16.16
    GENERIC(unsigned, scale_y) // 16.16
    GENERIC(int, frx) // signed 16.16
    GENERIC(int, fry) // signed 16.16
    GENERIC(int, frz) // signed 16.16
    // shift vector that was added to glyph before applying rotation
    // = 0, if frx = fry = frx = 0
    // = (glyph base point) - (rotation origin), otherwise
    GENERIC(int, shift_x)
    GENERIC(int, shift_y)
    FTVECTOR(advance) // subpixel shift vector
END(bitmap_hash_key_t)

// describes an outline glyph
START(glyph, glyph_hash_key_s)
    GENERIC(ass_font_t *, font)
    GENERIC(double, size) // font size
    GENERIC(uint32_t, ch) // character code
    GENERIC(int, bold)
    GENERIC(int, italic)
    GENERIC(unsigned, scale_x) // 16.16
    GENERIC(unsigned, scale_y) // 16.16
    FTVECTOR(advance) // subpixel shift vector
    GENERIC(unsigned, outline) // border width, 16.16
END(glyph_hash_key_t)

#undef START
#undef GENERIC
#undef FTVECTOR
#undef END
