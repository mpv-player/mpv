
typedef struct {
    unsigned char *bmp;
    unsigned char *pal;
    int w,h,c;
} raw_file;

typedef struct {
    char *name;
    int spacewidth;
    int charspace;
    int height;
//    char *fname_a;
//    char *fname_b;
    raw_file* pic_a[16];
    raw_file* pic_b[16];
    short font[512];
    short start[512];
    short width[512];
} font_desc_t;

raw_file* load_raw(char *name);
font_desc_t* read_font_desc(char* fname);
