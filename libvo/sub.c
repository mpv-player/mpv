
typedef struct {
    unsigned char *bmp;
    unsigned char *pal;
    int w,h,c;
} raw_file;

raw_file* load_raw(char *name){
    int bpp;
    raw_file* raw=malloc(sizeof(raw_file));
    unsigned char head[32];
    FILE *f=fopen(name,"rb");
    if(!f) return NULL;                        // can't open
    if(fread(head,32,1,f)<1) return NULL;        // too small
    if(memcmp(head,"mhwanh",6)) return NULL;        // not raw file
    raw->w=head[8]*256+head[9];
    raw->h=head[10]*256+head[11];
    raw->c=head[12]*256+head[13];
    if(raw->c>256) return NULL;                 // too many colors!?
    printf("RAW: %d x %d, %d colors\n",raw->w,raw->h,raw->c);
    if(raw->c){
        raw->pal=malloc(raw->c*3);
        fread(raw->pal,3,raw->c,f);
        bpp=1;
    } else {
        raw->pal=NULL;
        bpp=3;
    }
    raw->bmp=malloc(raw->h*raw->w*bpp);
    fread(raw->bmp,raw->h*raw->w*bpp,1,f);
    fclose(f);
    return raw;
}

static int vo_font_loaded=-1;
static raw_file* vo_font_bmp=NULL;
static raw_file* vo_font_alpha=NULL;

void vo_load_font(char *bmpname,char *alphaname){
    vo_font_loaded=0;
    if(!(vo_font_bmp=load_raw(bmpname)))
        printf("vo: Can't load font BMP\n"); else
    if(!(vo_font_alpha=load_raw(alphaname)))
        printf("vo: Can't load font Alpha\n"); else
    vo_font_loaded=1;
}

int vo_sub_lines=2;
char* vo_sub_text[8];


void vo_draw_text(void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
    int i;
    int y=100;

    if(vo_sub_lines<=0) return; // no text
    
    if(vo_font_loaded==-1) vo_load_font("font_b.raw","font_a.raw");
    if(!vo_font_loaded) return; // no font
    
//    if(!vo_font_bmp) vo_load_font("mplayer_font_lowres_bitmap.raw","mplayer_font_lowres_alpha.raw");
    
    for(i=0;i<vo_sub_lines;i++){
        char* text="Hello World!"; //vo_sub_text[i];
        draw_alpha(100,y,50,vo_font_bmp->h,vo_font_bmp->bmp,vo_font_alpha->bmp,vo_font_bmp->w);
//        x11_draw_alpha(100,y,50,vo_font_bmp->h,vo_font_bmp->bmp,vo_font_alpha->bmp,vo_font_bmp->w);
        y+=50;
    }

}

