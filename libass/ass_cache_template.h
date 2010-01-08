#ifdef CREATE_STRUCT_DEFINITIONS
#undef CREATE_STRUCT_DEFINITIONS
#define START(funcname, structname) \
    typedef struct structname {
#define GENERIC(type, member) \
        type member;
#define FTVECTOR(member) \
        FT_Vector member;
#define BITMAPHASHKEY(member) \
        BitmapHashKey member;
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
#define BITMAPHASHKEY(member) \
            bitmap_compare(&a->member, &b->member, sizeof(a->member)) &&
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
#define BITMAPHASHKEY(member) { \
        unsigned temp = bitmap_hash(&p->member, sizeof(p->member)); \
        hval = fnv_32a_buf(&temp, sizeof(temp), hval); \
        }
#define END(typedefname) \
        return hval; \
    }

#else
#error missing defines
#endif



// describes a bitmap; bitmaps with equivalents structs are considered identical
START(bitmap, bitmap_hash_key)
    GENERIC(char, bitmap) // bool : true = bitmap, false = outline
    GENERIC(ASS_Font *, font)
    GENERIC(double, size) // font size
    GENERIC(uint32_t, ch) // character code
    FTVECTOR(outline) // border width, 16.16 fixed point value
    GENERIC(int, bold)
    GENERIC(int, italic)
    GENERIC(char, be) // blur edges
    GENERIC(double, blur) // gaussian blur
    GENERIC(unsigned, scale_x) // 16.16
    GENERIC(unsigned, scale_y) // 16.16
    GENERIC(int, frx) // signed 16.16
    GENERIC(int, fry) // signed 16.16
    GENERIC(int, frz) // signed 16.16
    GENERIC(int, fax) // signed 16.16
    GENERIC(int, fay) // signed 16.16
    // shift vector that was added to glyph before applying rotation
    // = 0, if frx = fry = frx = 0
    // = (glyph base point) - (rotation origin), otherwise
    GENERIC(int, shift_x)
    GENERIC(int, shift_y)
    FTVECTOR(advance) // subpixel shift vector
    FTVECTOR(shadow_offset) // shadow subpixel shift
    GENERIC(unsigned, drawing_hash) // hashcode of a drawing
    GENERIC(unsigned, flags)    // glyph decoration
    GENERIC(unsigned, border_style)
END(BitmapHashKey)

// describes an outline glyph
START(glyph, glyph_hash_key)
    GENERIC(ASS_Font *, font)
    GENERIC(double, size) // font size
    GENERIC(uint32_t, ch) // character code
    GENERIC(int, bold)
    GENERIC(int, italic)
    GENERIC(unsigned, scale_x) // 16.16
    GENERIC(unsigned, scale_y) // 16.16
    FTVECTOR(outline) // border width, 16.16
    GENERIC(unsigned, drawing_hash) // hashcode of a drawing
    GENERIC(unsigned, flags)    // glyph decoration flags
    GENERIC(unsigned, border_style)
END(GlyphHashKey)

// Cache for composited bitmaps
START(composite, composite_hash_key)
    GENERIC(int, aw)
    GENERIC(int, ah)
    GENERIC(int, bw)
    GENERIC(int, bh)
    GENERIC(int, ax)
    GENERIC(int, ay)
    GENERIC(int, bx)
    GENERIC(int, by)
    GENERIC(int, as)
    GENERIC(int, bs)
    GENERIC(unsigned char *, a)
    GENERIC(unsigned char *, b)
END(CompositeHashKey)


#undef START
#undef GENERIC
#undef FTVECTOR
#undef BITMAPHASHKEY
#undef END
