#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <linux/tty.h>
#include <asm/types.h>

/* Definitions of frame buffers						*/

#define FB_MAJOR		29
#define FB_MAX			32	/* sufficient for now */

/* ioctls
   0x46 is 'F'								*/
#define FBIOGET_VSCREENINFO	0x4600
#define FBIOPUT_VSCREENINFO	0x4601
#define FBIOGET_FSCREENINFO	0x4602
#define FBIOGETCMAP		0x4604
#define FBIOPUTCMAP		0x4605
#define FBIOPAN_DISPLAY		0x4606
/* 0x4607-0x460B are defined below */
/* #define FBIOGET_MONITORSPEC	0x460C */
/* #define FBIOPUT_MONITORSPEC	0x460D */
/* #define FBIOSWITCH_MONIBIT	0x460E */
#define FBIOGET_CON2FBMAP	0x460F
#define FBIOPUT_CON2FBMAP	0x4610
#define FBIOBLANK		0x4611		/* arg: 0 or vesa level + 1 */
#define FBIOGET_VBLANK		_IOR('F', 0x12, struct fb_vblank)

/* next 2 lines are temporary solution, support in 2.5 will be different */
#define FBIOGET_VT_VSCREENINFO	0x4680
#define FBIOPUT_VT_VSCREENINFO	0x4681


#define FB_TYPE_PACKED_PIXELS		0	/* Packed Pixels	*/
#define FB_TYPE_PLANES			1	/* Non interleaved planes */
#define FB_TYPE_INTERLEAVED_PLANES	2	/* Interleaved planes	*/
#define FB_TYPE_TEXT			3	/* Text/attributes	*/
#define FB_TYPE_VGA_PLANES		4	/* EGA/VGA planes	*/

#define FB_AUX_TEXT_MDA		0	/* Monochrome text */
#define FB_AUX_TEXT_CGA		1	/* CGA/EGA/VGA Color text */
#define FB_AUX_TEXT_S3_MMIO	2	/* S3 MMIO fasttext */
#define FB_AUX_TEXT_MGA_STEP16	3	/* MGA Millenium I: text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_MGA_STEP8	4	/* other MGAs:      text, attr,  6 reserved bytes */

#define FB_AUX_VGA_PLANES_VGA4		0	/* 16 color planes (EGA/VGA) */
#define FB_AUX_VGA_PLANES_CFB4		1	/* CFB4 in planes (VGA) */
#define FB_AUX_VGA_PLANES_CFB8		2	/* CFB8 in planes (VGA) */

#define FB_VISUAL_MONO01		0	/* Monochr. 1=Black 0=White */
#define FB_VISUAL_MONO10		1	/* Monochr. 1=White 0=Black */
#define FB_VISUAL_TRUECOLOR		2	/* True color	*/
#define FB_VISUAL_PSEUDOCOLOR		3	/* Pseudo color (like atari) */
#define FB_VISUAL_DIRECTCOLOR		4	/* Direct color */
#define FB_VISUAL_STATIC_PSEUDOCOLOR	5	/* Pseudo color readonly */

#define FB_ACCEL_NONE		0	/* no hardware accelerator	*/
#define FB_ACCEL_ATARIBLITT	1	/* Atari Blitter		*/
#define FB_ACCEL_AMIGABLITT	2	/* Amiga Blitter                */
#define FB_ACCEL_S3_TRIO64	3	/* Cybervision64 (S3 Trio64)    */
#define FB_ACCEL_NCR_77C32BLT	4	/* RetinaZ3 (NCR 77C32BLT)      */
#define FB_ACCEL_S3_VIRGE	5	/* Cybervision64/3D (S3 ViRGE)	*/
#define FB_ACCEL_ATI_MACH64GX	6	/* ATI Mach 64GX family		*/
#define FB_ACCEL_DEC_TGA	7	/* DEC 21030 TGA		*/
#define FB_ACCEL_ATI_MACH64CT	8	/* ATI Mach 64CT family		*/
#define FB_ACCEL_ATI_MACH64VT	9	/* ATI Mach 64CT family VT class */
#define FB_ACCEL_ATI_MACH64GT	10	/* ATI Mach 64CT family GT class */
#define FB_ACCEL_SUN_CREATOR	11	/* Sun Creator/Creator3D	*/
#define FB_ACCEL_SUN_CGSIX	12	/* Sun cg6			*/
#define FB_ACCEL_SUN_LEO	13	/* Sun leo/zx			*/
#define FB_ACCEL_IMS_TWINTURBO	14	/* IMS Twin Turbo		*/
#define FB_ACCEL_3DLABS_PERMEDIA2 15	/* 3Dlabs Permedia 2		*/
#define FB_ACCEL_MATROX_MGA2064W 16	/* Matrox MGA2064W (Millenium)	*/
#define FB_ACCEL_MATROX_MGA1064SG 17	/* Matrox MGA1064SG (Mystique)	*/
#define FB_ACCEL_MATROX_MGA2164W 18	/* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGA2164W_AGP 19	/* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGAG100	20	/* Matrox G100 (Productiva G100) */
#define FB_ACCEL_MATROX_MGAG200	21	/* Matrox G200 (Myst, Mill, ...) */
#define FB_ACCEL_SUN_CG14	22	/* Sun cgfourteen		 */
#define FB_ACCEL_SUN_BWTWO	23	/* Sun bwtwo			*/
#define FB_ACCEL_SUN_CGTHREE	24	/* Sun cgthree			*/
#define FB_ACCEL_SUN_TCX	25	/* Sun tcx			*/
#define FB_ACCEL_MATROX_MGAG400	26	/* Matrox G400			*/
#define FB_ACCEL_NV3		27	/* nVidia RIVA 128              */
#define FB_ACCEL_NV4		28	/* nVidia RIVA TNT		*/
#define FB_ACCEL_NV5		29	/* nVidia RIVA TNT2		*/
#define FB_ACCEL_CT_6555x	30	/* C&T 6555x			*/
#define FB_ACCEL_3DFX_BANSHEE	31	/* 3Dfx Banshee			*/
#define FB_ACCEL_ATI_RAGE128	32	/* ATI Rage128 family		*/

struct fb_fix_screeninfo {
	char id[16];			/* identification string eg "TT Builtin" */
	unsigned long smem_start;	/* Start of frame buffer mem */
					/* (physical address) */
	__u32 smem_len;			/* Length of frame buffer mem */
	__u32 type;			/* see FB_TYPE_*		*/
	__u32 type_aux;			/* Interleave for interleaved Planes */
	__u32 visual;			/* see FB_VISUAL_*		*/ 
	__u16 xpanstep;			/* zero if no hardware panning  */
	__u16 ypanstep;			/* zero if no hardware panning  */
	__u16 ywrapstep;		/* zero if no hardware ywrap    */
	__u32 line_length;		/* length of a line in bytes    */
	unsigned long mmio_start;	/* Start of Memory Mapped I/O   */
					/* (physical address) */
	__u32 mmio_len;			/* Length of Memory Mapped I/O  */
	__u32 accel;			/* Type of acceleration available */
	__u16 reserved[3];		/* Reserved for future compatibility */
};

/* Interpretation of offset for color fields: All offsets are from the right,
 * inside a "pixel" value, which is exactly 'bits_per_pixel' wide (means: you
 * can use the offset as right argument to <<). A pixel afterwards is a bit
 * stream and is written to video memory as that unmodified. This implies
 * big-endian byte order if bits_per_pixel is greater than 8.
 */
struct fb_bitfield {
	__u32 offset;			/* beginning of bitfield	*/
	__u32 length;			/* length of bitfield		*/
	__u32 msb_right;		/* != 0 : Most significant bit is */ 
					/* right */ 
};

#define FB_NONSTD_HAM		1	/* Hold-And-Modify (HAM)        */

#define FB_ACTIVATE_NOW		0	/* set values immediately (or vbl)*/
#define FB_ACTIVATE_NXTOPEN	1	/* activate on next open	*/
#define FB_ACTIVATE_TEST	2	/* don't set, round up impossible */
#define FB_ACTIVATE_MASK       15
					/* values			*/
#define FB_ACTIVATE_VBL	       16	/* activate values on next vbl  */
#define FB_CHANGE_CMAP_VBL     32	/* change colormap on vbl	*/
#define FB_ACTIVATE_ALL	       64	/* change all VCs on this fb	*/

#define FB_ACCELF_TEXT		1	/* text mode acceleration */

#define FB_SYNC_HOR_HIGH_ACT	1	/* horizontal sync high active	*/
#define FB_SYNC_VERT_HIGH_ACT	2	/* vertical sync high active	*/
#define FB_SYNC_EXT		4	/* external sync		*/
#define FB_SYNC_COMP_HIGH_ACT	8	/* composite sync high active   */
#define FB_SYNC_BROADCAST	16	/* broadcast video timings      */
					/* vtotal = 144d/288n/576i => PAL  */
					/* vtotal = 121d/242n/484i => NTSC */
#define FB_SYNC_ON_GREEN	32	/* sync on green */

#define FB_VMODE_NONINTERLACED  0	/* non interlaced */
#define FB_VMODE_INTERLACED	1	/* interlaced	*/
#define FB_VMODE_DOUBLE		2	/* double scan */
#define FB_VMODE_MASK		255

#define FB_VMODE_YWRAP		256	/* ywrap instead of panning     */
#define FB_VMODE_SMOOTH_XPAN	512	/* smooth xpan possible (internally used) */
#define FB_VMODE_CONUPDATE	512	/* don't update x/yoffset	*/

struct fb_var_screeninfo {
	__u32 xres;			/* visible resolution		*/
	__u32 yres;
	__u32 xres_virtual;		/* virtual resolution		*/
	__u32 yres_virtual;
	__u32 xoffset;			/* offset from virtual to visible */
	__u32 yoffset;			/* resolution			*/

	__u32 bits_per_pixel;		/* guess what			*/
	__u32 grayscale;		/* != 0 Graylevels instead of colors */

	struct fb_bitfield red;		/* bitfield in fb mem if true color, */
	struct fb_bitfield green;	/* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency			*/	

	__u32 nonstd;			/* != 0 Non standard pixel format */

	__u32 activate;			/* see FB_ACTIVATE_*		*/

	__u32 height;			/* height of picture in mm    */
	__u32 width;			/* width of picture in mm     */

	__u32 accel_flags;		/* acceleration flags (hints)	*/

	/* Timing: All values in pixclocks, except pixclock (of course) */
	__u32 pixclock;			/* pixel clock in ps (pico seconds) */
	__u32 left_margin;		/* time from sync to picture	*/
	__u32 right_margin;		/* time from picture to sync	*/
	__u32 upper_margin;		/* time from sync to picture	*/
	__u32 lower_margin;
	__u32 hsync_len;		/* length of horizontal sync	*/
	__u32 vsync_len;		/* length of vertical sync	*/
	__u32 sync;			/* see FB_SYNC_*		*/
	__u32 vmode;			/* see FB_VMODE_*		*/
	__u32 reserved[6];		/* Reserved for future compatibility */
};

struct fb_cmap {
	__u32 start;			/* First entry	*/
	__u32 len;			/* Number of entries */
	__u16 *red;			/* Red values	*/
	__u16 *green;
	__u16 *blue;
	__u16 *transp;			/* transparency, can be NULL */
};

struct fb_con2fbmap {
	__u32 console;
	__u32 framebuffer;
};

/* VESA Blanking Levels */
#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3

struct fb_monspecs {
	__u32 hfmin;			/* hfreq lower limit (Hz) */
	__u32 hfmax; 			/* hfreq upper limit (Hz) */
	__u16 vfmin;			/* vfreq lower limit (Hz) */
	__u16 vfmax;			/* vfreq upper limit (Hz) */
	unsigned dpms : 1;		/* supports DPMS */
};

/* next structure is only 2.2 temporary hack, 2.3 solution will be different */
struct fb_vt_info {
	__u32 console;
	struct fb_var_screeninfo *info;
};

#define FB_VBLANK_VBLANKING     1       /* currently in a vertical blank */
#define FB_VBLANK_HBLANKING     2       /* currently in a horizontal blank */
#define FB_VBLANK_HAVE_VBLANK   4       /* vertical blanks can be detected */
#define FB_VBLANK_HAVE_HBLANK   8       /* horizontal blanks can be detected */
#define FB_VBLANK_HAVE_COUNT    16      /* global retrace counter is available */
#define FB_VBLANK_HAVE_VCOUNT   32      /* the vcount field is valid */
#define FB_VBLANK_HAVE_HCOUNT   64      /* the hcount field is valid */

struct fb_vblank {
	__u32 flags;                    /* FB_VBLANK flags */
	__u32 count;                    /* counter of retraces since boot */
	__s32 vcount;                   /* current scanline position */
	__s32 hcount;                   /* current scandot position */
	__u32 reserved[4];              /* reserved for future compatibility */
};

#ifdef __KERNEL__

#if 1 /* to go away in 2.4.0 */
extern int GET_FB_IDX(kdev_t rdev);
#else
#define GET_FB_IDX(node)	(MINOR(node))
#endif

#include <linux/fs.h>
#include <linux/init.h>


struct fb_info;
struct fb_info_gen;
struct vm_area_struct;
struct file;

    /*
     *  Frame buffer operations
     */

struct fb_ops {
    /* open/release and usage marking */
    int (*fb_open)(struct fb_info *info, int user);
    int (*fb_release)(struct fb_info *info, int user);
    /* get non settable parameters */
    int (*fb_get_fix)(struct fb_fix_screeninfo *fix, int con,
		      struct fb_info *info); 
    /* get settable parameters */
    int (*fb_get_var)(struct fb_var_screeninfo *var, int con,
		      struct fb_info *info);		
    /* set settable parameters */
    int (*fb_set_var)(struct fb_var_screeninfo *var, int con,
		      struct fb_info *info);		
    /* get colormap */
    int (*fb_get_cmap)(struct fb_cmap *cmap, int kspc, int con,
		       struct fb_info *info);
    /* set colormap */
    int (*fb_set_cmap)(struct fb_cmap *cmap, int kspc, int con,
		       struct fb_info *info);
    /* pan display */
    int (*fb_pan_display)(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info);
    /* perform fb specific ioctl */
    int (*fb_ioctl)(struct inode *inode, struct file *file, unsigned int cmd,
		    unsigned long arg, int con, struct fb_info *info);
    /* perform fb specific mmap */
    int (*fb_mmap)(struct fb_info *info, struct file *file, struct vm_area_struct *vma);
    /* switch to/from raster image mode */
    int (*fb_rasterimg)(struct fb_info *info, int start);
};

struct fb_info {
   char modename[40];			/* default video mode */
   kdev_t node;
   int flags;
   int open;                            /* Has this been open already ? */
#define FBINFO_FLAG_MODULE	1	/* Low-level driver is a module */
   struct fb_fix_screeninfo fix;        /* Current fix */
   struct fb_monspecs monspecs;         /* Current Monitor specs */
   struct fb_ops *fbops;
   char *screen_base;                   /* Virtual address */
   struct display *disp;		/* initial display variable */
   struct display *currcon;
   struct vc_data *display_fg;		/* Console visible on this display */
   char fontname[40];			/* default font name */
   int (*changevar)(int);		/* tell console var has changed */
   int (*switch_con)(int, struct fb_info*);
					/* tell fb to switch consoles */
   int (*updatevar)(int, struct fb_info*);
					/* tell fb to update the vars */
   void (*blank)(int, struct fb_info*);	/* tell fb to (un)blank the screen */
					/* arg = 0: unblank */
					/* arg > 0: VESA level (arg-1) */
   void *pseudo_palette;                /* Fake palette of 16 colors and 
					   the cursor's color for non
                                           palette mode */
   /* From here on everything is device dependent */
   void *par;	
};

#ifdef MODULE
#define FBINFO_FLAG_DEFAULT	FBINFO_FLAG_MODULE
#else
#define FBINFO_FLAG_DEFAULT	0
#endif

    /*
     *  This structure abstracts from the underlying hardware. It is not
     *  mandatory but used by the `generic' frame buffer operations.
     *  Read drivers/video/skeletonfb.c for more information.
     */

struct fbgen_hwswitch {
    void (*detect)(void);
    int (*encode_fix)(struct fb_fix_screeninfo *fix, const void *par,
		      struct fb_info_gen *info);
    int (*decode_var)(const struct fb_var_screeninfo *var, void *par,
		      struct fb_info_gen *info);
    int (*encode_var)(struct fb_var_screeninfo *var, const void *par,
		      struct fb_info_gen *info);
    void (*get_par)(void *par, struct fb_info_gen *info);
    void (*set_par)(const void *par, struct fb_info_gen *info);
    int (*getcolreg)(unsigned regno, unsigned *red, unsigned *green,
		     unsigned *blue, unsigned *transp, struct fb_info *info);
    int (*setcolreg)(unsigned regno, unsigned red, unsigned green,
		     unsigned blue, unsigned transp, struct fb_info *info);
    int (*pan_display)(const struct fb_var_screeninfo *var,
		       struct fb_info_gen *info);
    int (*blank)(int blank_mode, struct fb_info_gen *info);
    void (*set_disp)(const void *par, struct display *disp,
		     struct fb_info_gen *info);
};

struct fb_info_gen {
    struct fb_info info;

    /* Entries for a generic frame buffer device */
    /* Yes, this starts looking like C++ */
    u_int parsize;
    struct fbgen_hwswitch *fbhw;

   /* From here on everything is device dependent */
};

    /*
     *  `Generic' versions of the frame buffer device operations
     */

extern int fbgen_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info);
extern int fbgen_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info);
extern int fbgen_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info);
extern int fbgen_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info);
extern int fbgen_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info);
extern int fbgen_pan_display(struct fb_var_screeninfo *var, int con,
			     struct fb_info *info);
extern int fbgen_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg, int con,
		       struct fb_info *info);

    /*
     *  Helper functions
     */

extern int fbgen_do_set_var(struct fb_var_screeninfo *var, int isactive,
			    struct fb_info_gen *info);
extern void fbgen_set_disp(int con, struct fb_info_gen *info);
extern void fbgen_install_cmap(int con, struct fb_info_gen *info);
extern int fbgen_update_var(int con, struct fb_info *info);
extern int fbgen_switch(int con, struct fb_info *info);
extern void fbgen_blank(int blank, struct fb_info *info);


/* drivers/video/fbmem.c */
extern int register_framebuffer(struct fb_info *fb_info);
extern int unregister_framebuffer(const struct fb_info *fb_info);

extern int num_registered_fb;
extern struct fb_info *registered_fb[FB_MAX];

/* drivers/video/fbmon.c */
extern int fbmon_valid_timings(u_int pixclock, u_int htotal, u_int vtotal,
			       const struct fb_info *fb_info);
extern int fbmon_dpms(const struct fb_info *fb_info);

/* drivers/video/fbcmap.c */
extern int fb_alloc_cmap(struct fb_cmap *cmap, int len, int transp);
extern void fb_copy_cmap(struct fb_cmap *from, struct fb_cmap *to,
			 int fsfromto);
extern int fb_get_cmap(struct fb_cmap *cmap, int kspc,
		       int (*getcolreg)(u_int, u_int *, u_int *, u_int *,
					u_int *, struct fb_info *),
		       struct fb_info *fb_info);
extern int fb_set_cmap(struct fb_cmap *cmap, int kspc,
		       int (*setcolreg)(u_int, u_int, u_int, u_int, u_int,
					struct fb_info *),
		       struct fb_info *fb_info);
extern struct fb_cmap *fb_default_cmap(int len);
extern void fb_invert_cmaps(void);

struct fb_videomode {
    const char *name;	/* optional */
    u32 refresh;	/* optional */
    u32 xres;
    u32 yres;
    u32 pixclock;
    u32 left_margin;
    u32 right_margin;
    u32 upper_margin;
    u32 lower_margin;
    u32 hsync_len;
    u32 vsync_len;
    u32 sync;
    u32 vmode;
};

extern int __init fb_find_mode(struct fb_var_screeninfo *var,
			       struct fb_info *info, const char *mode_option,
			       const struct fb_videomode *db,
			       unsigned int dbsize,
			       const struct fb_videomode *default_mode,
			       unsigned int default_bpp);

#endif /* __KERNEL__ */

#if 1

#define FBCMD_GET_CURRENTPAR	0xDEAD0005
#define FBCMD_SET_CURRENTPAR	0xDEAD8005

#endif


#if 1 /* Preliminary */

   /*
    *    Hardware Cursor
    */

#define FBIOGET_FCURSORINFO     0x4607
#define FBIOGET_VCURSORINFO     0x4608
#define FBIOPUT_VCURSORINFO     0x4609
#define FBIOGET_CURSORSTATE     0x460A
#define FBIOPUT_CURSORSTATE     0x460B


struct fb_fix_cursorinfo {
	__u16 crsr_width;		/* width and height of the cursor in */
	__u16 crsr_height;		/* pixels (zero if no cursor)	*/
	__u16 crsr_xsize;		/* cursor size in display pixels */
	__u16 crsr_ysize;
	__u16 crsr_color1;		/* colormap entry for cursor color1 */
	__u16 crsr_color2;		/* colormap entry for cursor color2 */
};

struct fb_var_cursorinfo {
	__u16 width;
	__u16 height;
	__u16 xspot;
	__u16 yspot;
	__u8 data[1];			/* field with [height][width]        */
};

struct fb_cursorstate {
	__s16 xoffset;
	__s16 yoffset;
	__u16 mode;
};

#define FB_CURSOR_OFF		0
#define FB_CURSOR_ON		1
#define FB_CURSOR_FLASH		2

#endif /* Preliminary */

#endif /* _LINUX_FB_H */
