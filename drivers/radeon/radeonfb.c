/*
 *	drivers/video/radeonfb.c
 *	framebuffer driver for ATI Radeon chipset video boards
 *
 *	Copyright 2000	Ani Joshi <ajoshi@unixbox.com>
 *
 *
 *	ChangeLog:
 *	2000-08-03	initial version 0.0.1
 *	2000-09-10	more bug fixes, public release 0.0.5
 *	2001-02-19	mode bug fixes, 0.0.7
 *	2001-07-05	fixed scrolling issues, engine initialization,
 *			and minor mode tweaking, 0.0.9
 *
 *	2001-09-07	Radeon VE support
 *	2001-09-10	Radeon VE QZ support by Nick Kurshev <nickols_k@mail.ru>
 *			(limitations: on dualhead Radeons (VE, M6, M7)
 *			 driver works only on second head (DVI port).
 *			 TVout is not supported too. M6 & M7 chips
 *			 currently are not supported. Driver has a lot
 *			 of other bugs. Probably they can be solved by
 *			 importing XFree86 code, which has ATI's support).,
 *			 0.0.11
 *	2001-09-13	merge Ani Joshi radeonfb-0.1.0:
 *			console switching fixes, blanking fixes,
 *			0.1.0-ve.0
 *	2001-09-18	Radeon VE, M6 support (by Nick Kurshev <nickols_k@mail.ru>),
 *			Fixed bug of rom bios detection on VE (by NK),
 *                      Minor code cleanup (by NK),
 *			Enable CRT port on VE (by NK),
 *			Disable SURFACE_CNTL because mplayer doesn't work
 *			propertly (by NK)
 *			0.1.0-ve.1
 *	2001-09-25	MTRR support (by NK)
 *	Special thanks to ATI DevRel team for their hardware donations.
 *
 * LIMITATIONS: on dualhead Radeons (VE, M6, M7) driver doesn't work in
 * dual monitor configuration. TVout is not supported too.
 * Probably these problems can be solved by importing XFree86 code, which
 * has ATI's support.
 *
 * Mini-HOWTO: This driver doesn't accept any options. It only switches your
 * video card to graphics mode. Standard way to change video modes and other
 * video attributes is using 'fbset' utility.
 * Sample:
 * 
 * #!/bin/sh
 * fbset -fb /dev/fb0 -xres 640 -yres 480 -depth 32 -vxres 640 -vyres 480 -left 70 -right 50 -upper 70 -lower 70 -laced false -pixclock 39767
 *
*/

#define RADEON_VERSION	"0.1.0-ve.1"

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/io.h>

#include <video/fbcon.h> 
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include "radeon.h"


#define DEBUG	0

#if DEBUG
#define RTRACE		printk
#else
#define RTRACE		if(0) printk
#endif



enum radeon_chips {
	RADEON_QD,
	RADEON_QE,
	RADEON_QF,
	RADEON_QG,
	RADEON_QY,
	RADEON_QZ,
	RADEON_LY,
	RADEON_LZ,
	RADEON_LW
};

enum radeon_montype
{
    MT_NONE,
    MT_CRT, /* CRT-(cathode ray tube) analog monitor. (15-pin VGA connector) */
    MT_LCD, /* Liquid Crystal Display */
    MT_DFP, /* DFP-digital flat panel monitor. (24-pin DVI-I connector) */
    MT_CTV, /* Composite TV out (not in VE) */
    MT_STV  /* S-Video TV out (probably in VE only) */
};

enum radeon_ddctype
{
    DDC_NONE_DETECTED,
    DDC_MONID,
    DDC_DVI,
    DDC_VGA,
    DDC_CRT2
};

enum radeon_connectortype
{
    CONNECTOR_NONE,
    CONNECTOR_PROPRIETARY,
    CONNECTOR_CRT,
    CONNECTOR_DVI_I,
    CONNECTOR_DVI_D
};

static struct pci_device_id radeonfb_pci_table[] __devinitdata = {
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_QD, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QD},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_QE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QE},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_QF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QF},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_QG, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QG},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_QY, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QY},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_QZ, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_QZ},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_LY, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_LY},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_LZ, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_LZ},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_RADEON_LW, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RADEON_LW},
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, radeonfb_pci_table);


typedef struct {
	u16 reg;
	u32 val;
} reg_val;


/* these common regs are cleared before mode setting so they do not
 * interfere with anything
 */
reg_val common_regs[] = {
	{ OVR_CLR, 0 },	
	{ OVR_WID_LEFT_RIGHT, 0 },
	{ OVR_WID_TOP_BOTTOM, 0 },
	{ OV0_SCALE_CNTL, 0 },
	{ SUBPIC_CNTL, 0 },
	{ VIPH_CONTROL, 0 },
	{ I2C_CNTL_1, 0 },
	{ GEN_INT_CNTL, 0 },
	{ CAP0_TRIG_CNTL, 0 },
};

#define COMMON_REGS_SIZE = (sizeof(common_regs)/sizeof(common_regs[0]))

typedef struct {
        u8 clock_chip_type;
        u8 struct_size;
        u8 accelerator_entry;
        u8 VGA_entry;
        u16 VGA_table_offset;
        u16 POST_table_offset;
        u16 XCLK;
        u16 MCLK;
        u8 num_PLL_blocks;
        u8 size_PLL_blocks;
        u16 PCLK_ref_freq;
        u16 PCLK_ref_divider;
        u32 PCLK_min_freq;
        u32 PCLK_max_freq;
        u16 MCLK_ref_freq;
        u16 MCLK_ref_divider;
        u32 MCLK_min_freq;
        u32 MCLK_max_freq;
        u16 XCLK_ref_freq;
        u16 XCLK_ref_divider;
        u32 XCLK_min_freq;
        u32 XCLK_max_freq;
} __attribute__ ((packed)) PLL_BLOCK;


struct pll_info {
	int ppll_max;
	int ppll_min;
	int xclk;
	int ref_div;
	int ref_clk;
};


struct ram_info {
	int ml;
	int mb;
	int trcd;
	int trp;
	int twr;
	int cl;
	int tr2w;
	int loop_latency;
	int rloop;
};


struct radeon_regs {
	u32 crtc_h_total_disp;
	u32 crtc_h_sync_strt_wid;
	u32 crtc_v_total_disp;
	u32 crtc_v_sync_strt_wid;
	u32 crtc_pitch;
	u32 flags;
	u32 pix_clock;
	int xres, yres;
	int bpp;
	u32 crtc_gen_cntl;
	u32 crtc_ext_cntl;
#if defined(__BIG_ENDIAN)
	u32 surface_cntl;
#endif
	u32 dac_cntl;
	u32 dda_config;
	u32 dda_on_off;
	u32 ppll_div_3;
	u32 ppll_ref_div;
};


struct radeonfb_info {
	struct fb_info info;

	struct radeon_regs state;
	struct radeon_regs init_state;

	char name[14];
	char ram_type[12];

	int hasCRTC2;
	int crtDispType;
	int dviDispType;
	int hasTVout;
	int isM7;

	u32 mmio_base_phys;
	u32 fb_base_phys;

	u32 mmio_base;
	u32 fb_base;

	struct pci_dev *pdev;

	struct display disp;
	int currcon;
	struct display *currcon_display;

	struct { u8 red, green, blue, pad; } palette[256];

	int chipset;
	int video_ram;
	u8 rev;
	int pitch, bpp, depth;
	int xres, yres, pixclock;

	u32 dp_gui_master_cntl;

	struct pll_info pll;
	int pll_output_freq, post_div, fb_div;

	struct ram_info ram;

	u32 hack_crtc_ext_cntl;
	u32 hack_crtc_v_sync_strt_wid;
#ifdef CONFIG_MTRR
	struct { int vram; int vram_valid; } mtrr;
#endif
#if defined(FBCON_HAS_CFB16) || defined(FBCON_HAS_CFB32)
        union {
#if defined(FBCON_HAS_CFB16)
                u_int16_t cfb16[16];
#endif
#if defined(FBCON_HAS_CFB24)
                u_int32_t cfb24[16];
#endif  
#if defined(FBCON_HAS_CFB32)
                u_int32_t cfb32[16];
#endif  
        } con_cmap;
#endif  
};


static struct fb_var_screeninfo radeonfb_default_var = {
        640, 480, 640, 480, 0, 0, 8, 0,
        {0, 6, 0}, {0, 6, 0}, {0, 6, 0}, {0, 0, 0},
        0, 0, -1, -1, 0, 39721, 40, 24, 32, 11, 96, 2,
        0, FB_VMODE_NONINTERLACED
};


/*
 * IO macros
 */

#define INREG8(addr)		readb((rinfo->mmio_base)+addr)
#define OUTREG8(addr,val)	writeb(val, (rinfo->mmio_base)+addr)
#define INREG(addr)		readl((rinfo->mmio_base)+addr)
#define OUTREG(addr,val)	writel(val, (rinfo->mmio_base)+addr)

#define OUTPLL(addr,val)	OUTREG8(CLOCK_CNTL_INDEX, (addr & 0x0000001f) | 0x00000080); \
				OUTREG(CLOCK_CNTL_DATA, val)
#define OUTPLLP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INPLL(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTPLL(addr, _tmp);					\
	} while (0)

#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)


static __inline__ u32 _INPLL(struct radeonfb_info *rinfo, u32 addr)
{
	OUTREG8(CLOCK_CNTL_INDEX, addr & 0x0000001f);
	return (INREG(CLOCK_CNTL_DATA));
}

#define INPLL(addr)		_INPLL(rinfo, addr)


/*
 * 2D engine routines
 */

static __inline__ void radeon_engine_flush (struct radeonfb_info *rinfo)
{
	int i;

	/* initiate flush */
	OUTREGP(RB2D_DSTCACHE_CTLSTAT, RB2D_DC_FLUSH_ALL,
	        ~RB2D_DC_FLUSH_ALL);

	for (i=0; i < 2000000; i++) {
		if (!(INREG(RB2D_DSTCACHE_CTLSTAT) & RB2D_DC_BUSY))
			break;
	}
}


static __inline__ void _radeon_fifo_wait (struct radeonfb_info *rinfo, int entries)
{
	int i;

	for (i=0; i<2000000; i++)
		if ((INREG(RBBM_STATUS) & 0x7f) >= entries)
			return;
}


static __inline__ void _radeon_engine_idle (struct radeonfb_info *rinfo)
{
	int i;

	/* ensure FIFO is empty before waiting for idle */
	_radeon_fifo_wait (rinfo, 64);

	for (i=0; i<2000000; i++) {
		if (((INREG(RBBM_STATUS) & GUI_ACTIVE)) == 0) {
			radeon_engine_flush (rinfo);
			return;
		}
	}
}


#define radeon_engine_idle()		_radeon_engine_idle(rinfo)
#define radeon_fifo_wait(entries)	_radeon_fifo_wait(rinfo,entries)



/*
 * helper routines
 */

static __inline__ u32 radeon_get_dstbpp(u16 depth)
{
	switch (depth) {
		case 8:
			return DST_8BPP;
		case 15:
			return DST_15BPP;
		case 16:
			return DST_16BPP;
		case 24:
			return DST_24BPP;
		case 32:
			return DST_32BPP;
		default:
			return 0;
	}
}


static void _radeon_engine_reset(struct radeonfb_info *rinfo)
{
	u32 clock_cntl_index, mclk_cntl, rbbm_soft_reset;

	radeon_engine_flush (rinfo);

	clock_cntl_index = INREG(CLOCK_CNTL_INDEX);
	mclk_cntl = INPLL(MCLK_CNTL);

	OUTPLL(MCLK_CNTL, (mclk_cntl |
			   FORCEON_MCLKA |
			   FORCEON_MCLKB |
			   FORCEON_YCLKA |
			   FORCEON_YCLKB |
			   FORCEON_MC |
			   FORCEON_AIC));
	rbbm_soft_reset = INREG(RBBM_SOFT_RESET);

	OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset |
				SOFT_RESET_CP |
				SOFT_RESET_HI |
				SOFT_RESET_SE |
				SOFT_RESET_RE |
				SOFT_RESET_PP |
				SOFT_RESET_E2 |
				SOFT_RESET_RB |
				SOFT_RESET_HDP);
	INREG(RBBM_SOFT_RESET);
	OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset & (u32)
				~(SOFT_RESET_CP |
				  SOFT_RESET_HI |
				  SOFT_RESET_SE |
				  SOFT_RESET_RE |
				  SOFT_RESET_PP |
				  SOFT_RESET_E2 |
				  SOFT_RESET_RB |
				  SOFT_RESET_HDP));
	INREG(RBBM_SOFT_RESET);

	OUTPLL(MCLK_CNTL, mclk_cntl);
	OUTREG(CLOCK_CNTL_INDEX, clock_cntl_index);
	OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset);

	return;
}

#define radeon_engine_reset()		_radeon_engine_reset(rinfo)


static __inline__ u8 radeon_get_post_div_bitval(int post_div)
{
        switch (post_div) {
                case 1:
                        return 0x00;
                case 2: 
                        return 0x01;
                case 3: 
                        return 0x04;
                case 4:
                        return 0x02;
                case 6:
                        return 0x06;
                case 8:
                        return 0x03;
                case 12:
                        return 0x07;
                default:
                        return 0x02;
        }
}



static __inline__ int round_div(int num, int den)
{
        return (num + (den / 2)) / den;
}



static __inline__ int min_bits_req(int val)
{
        int bits_req = 0;
                
        if (val == 0)
                bits_req = 1;
                        
        while (val) {
                val >>= 1;
                bits_req++;
        }       

        return (bits_req);
}


static __inline__ int _max(int val1, int val2)
{
        if (val1 >= val2)
                return val1;
        else
                return val2;
}                       



/*
 * globals
 */
        
static char fontname[40] __initdata;
static char *mode_option __initdata;
static char noaccel __initdata = 0;
static int  nomtrr __initdata = 0;

#if 0
#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_radeon8;
#endif
#endif

#ifdef CONFIG_MTRR
static int mtrr = 1;
#endif

/*
 * prototypes
 */

static int radeonfb_get_fix (struct fb_fix_screeninfo *fix, int con,
                             struct fb_info *info);
static int radeonfb_get_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info);
static int radeonfb_set_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info);
static int radeonfb_get_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info);
static int radeonfb_set_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info);
static int radeonfb_pan_display (struct fb_var_screeninfo *var, int con,
                                 struct fb_info *info);
static int radeonfb_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                           unsigned long arg, int con, struct fb_info *info);
static int radeonfb_switch (int con, struct fb_info *info);
static int radeonfb_updatevar (int con, struct fb_info *info);
static void radeonfb_blank (int blank, struct fb_info *info);
static int radeon_get_cmap_len (const struct fb_var_screeninfo *var);
static int radeon_getcolreg (unsigned regno, unsigned *red, unsigned *green,
                             unsigned *blue, unsigned *transp,
                             struct fb_info *info);
static int radeon_setcolreg (unsigned regno, unsigned red, unsigned green,
                             unsigned blue, unsigned transp, struct fb_info *info);
static void radeon_set_dispsw (struct radeonfb_info *rinfo, struct display *disp);
static void radeon_save_state (struct radeonfb_info *rinfo,
                               struct radeon_regs *save);
static void radeon_engine_init (struct radeonfb_info *rinfo);
static void radeon_load_video_mode (struct radeonfb_info *rinfo,
                                    struct fb_var_screeninfo *mode);
static void radeon_write_mode (struct radeonfb_info *rinfo,
                               struct radeon_regs *mode);
static int __devinit radeon_set_fbinfo (struct radeonfb_info *rinfo);
static int __devinit radeon_init_disp (struct radeonfb_info *rinfo);
static int radeon_init_disp_var (struct radeonfb_info *rinfo);
static int radeonfb_pci_register (struct pci_dev *pdev,
                                 const struct pci_device_id *ent);
static void __devexit radeonfb_pci_unregister (struct pci_dev *pdev);
static char *radeon_find_rom(struct radeonfb_info *rinfo);
static void radeon_get_pllinfo(struct radeonfb_info *rinfo, char *bios_seg);
static void do_install_cmap(int con, struct fb_info *info);
static int radeonfb_do_maximize(struct radeonfb_info *rinfo,
				struct fb_var_screeninfo *var,
				struct fb_var_screeninfo *v,
				int nom, int den);

static struct fb_ops radeon_fb_ops = {
	fb_get_fix:		radeonfb_get_fix,
	fb_get_var:		radeonfb_get_var,
	fb_set_var:		radeonfb_set_var,
	fb_get_cmap:		radeonfb_get_cmap,
	fb_set_cmap:		radeonfb_set_cmap,
	fb_pan_display:		radeonfb_pan_display,
	fb_ioctl:		radeonfb_ioctl,
};


static struct pci_driver radeonfb_driver = {
	name:		"radeonfb",
	id_table:	radeonfb_pci_table,
	probe:		radeonfb_pci_register,
	remove:		radeonfb_pci_unregister,
};


int __init radeonfb_init (void)
{
#ifdef CONFIG_MTRR
    if (nomtrr) {
        mtrr = 0;
        printk("radeonfb: Parameter NOMTRR set\n");
    }
#endif
    return pci_module_init (&radeonfb_driver);
}


void __exit radeonfb_exit (void)
{
	pci_unregister_driver (&radeonfb_driver);
}


int __init radeonfb_setup (char *options)
{
        char *this_opt;

        if (!options || !*options)
                return 0;
 
        for (this_opt = strtok (options, ","); this_opt;
             this_opt = strtok (NULL, ",")) {
                if (!strncmp (this_opt, "font:", 5)) {
                        char *p;
                        int i;
        
                        p = this_opt + 5;
                        for (i=0; i<sizeof (fontname) - 1; i++)
                                if (!*p || *p == ' ' || *p == ',')
                                        break;
                        memcpy(fontname, this_opt + 5, i);
                } else if (!strncmp(this_opt, "noaccel", 7)) {
			noaccel = 1;
		}
#ifdef CONFIG_MTRR
		else if(!strncmp(this_opt, "nomtrr", 6)) {
		mtrr = 0;
		}
#endif
                else    mode_option = this_opt;
        }

	return 0;
}

#ifdef MODULE
module_init(radeonfb_init);
module_exit(radeonfb_exit);
#endif


MODULE_AUTHOR("Ani Joshi. (Radeon VE extensions by Nick Kurshev)");
MODULE_DESCRIPTION("framebuffer driver for ATI Radeon chipset. Ver: "RADEON_VERSION);
#ifdef CONFIG_MTRR
MODULE_PARM(nomtrr, "i");
MODULE_PARM_DESC(nomtrr, "Don't touch MTRR (touch=0(default))");
#endif

static char * GET_MON_NAME(int type)
{
  char *pret;
  switch(type)
  {
    case MT_NONE: pret = "no"; break;
    case MT_CRT:  pret = "CRT"; break;
    case MT_DFP:  pret = "DFP"; break;
    case MT_LCD:  pret = "LCD"; break;
    case MT_CTV:  pret = "CTV"; break;
    case MT_STV:  pret = "STV"; break;
    default:      pret = "Unknown";
  }
  return pret;
}


static int radeonfb_pci_register (struct pci_dev *pdev,
				  const struct pci_device_id *ent)
{
	struct radeonfb_info *rinfo;
	u32 tmp;
	int i, j;
	char *bios_seg = NULL;

	rinfo = kmalloc (sizeof (struct radeonfb_info), GFP_KERNEL);
	if (!rinfo) {
		printk ("radeonfb: could not allocate memory\n");
		return -ENODEV;
	}

	memset (rinfo, 0, sizeof (struct radeonfb_info));

	/* enable device */
	{
		int err;

		if ((err = pci_enable_device(pdev))) {
			printk("radeonfb: cannot enable device\n");
			kfree (rinfo);
			return -ENODEV;
		}
	}

	/* set base addrs */
	rinfo->fb_base_phys = pci_resource_start (pdev, 0);
	rinfo->mmio_base_phys = pci_resource_start (pdev, 2);

	/* request the mem regions */
	if (!request_mem_region (rinfo->fb_base_phys,
				 pci_resource_len(pdev, 0), "radeonfb")) {
		printk ("radeonfb: cannot reserve FB region\n");
		kfree (rinfo);
		return -ENODEV;
	}

	if (!request_mem_region (rinfo->mmio_base_phys,
				 pci_resource_len(pdev, 2), "radeonfb")) {
		printk ("radeonfb: cannot reserve MMIO region\n");
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

	/* map the regions */
	rinfo->mmio_base = (u32) ioremap (rinfo->mmio_base_phys,
				    		    RADEON_REGSIZE);
	if (!rinfo->mmio_base) {
		printk ("radeonfb: cannot map MMIO\n");
		release_mem_region (rinfo->mmio_base_phys,
				    pci_resource_len(pdev, 2));
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

	/* chipset */
	switch (pdev->device) {
		case PCI_DEVICE_ID_RADEON_QD:
			strcpy(rinfo->name, "Radeon QD ");
			break;
		case PCI_DEVICE_ID_RADEON_QE:
			strcpy(rinfo->name, "Radeon QE ");
			break;
		case PCI_DEVICE_ID_RADEON_QF:
			strcpy(rinfo->name, "Radeon QF ");
			break;
		case PCI_DEVICE_ID_RADEON_QG:
			strcpy(rinfo->name, "Radeon QG ");
			break;
		case PCI_DEVICE_ID_RADEON_QY:
			rinfo->hasCRTC2 = 1;
			strcpy(rinfo->name, "Radeon VE QY ");
			break;
		case PCI_DEVICE_ID_RADEON_QZ:
			rinfo->hasCRTC2 = 1;
			strcpy(rinfo->name, "Radeon VE QZ ");
			break;
		case PCI_DEVICE_ID_RADEON_LY:
			rinfo->hasCRTC2 = 1;
			strcpy(rinfo->name, "Radeon M6 LY ");
			break;
		case PCI_DEVICE_ID_RADEON_LZ:
			rinfo->hasCRTC2 = 1;
			strcpy(rinfo->name, "Radeon M6 LZ ");
			break;
		case PCI_DEVICE_ID_RADEON_LW:
/* Note: Only difference between VE,M6 and M7 is initialization CRTC2
   registers in dual monitor configuration!!! */
			rinfo->hasCRTC2 = 1;
			rinfo->isM7 = 1;
			strcpy(rinfo->name, "Radeon M7 LW ");
			break;
		default:
			release_mem_region (rinfo->mmio_base_phys,
				    pci_resource_len(pdev, 2));
			release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
			kfree (rinfo);
			return -ENODEV;
	}

	/* framebuffer size */
	tmp = INREG(CONFIG_MEMSIZE);

	/* mem size is bits [28:0], mask off the rest */
	rinfo->video_ram = tmp & CONFIG_MEMSIZE_MASK;

	/* ram type */
	tmp = INREG(MEM_SDRAM_MODE_REG);
	switch ((MEM_CFG_TYPE & tmp) >> 30) {
		case 0:
			/* SDR SGRAM (2:1) */
			strcpy(rinfo->ram_type, "SDR SGRAM");
			rinfo->ram.ml = 4;
			rinfo->ram.mb = 4;
			rinfo->ram.trcd = 1;
			rinfo->ram.trp = 2;
			rinfo->ram.twr = 1;
			rinfo->ram.cl = 2;
			rinfo->ram.loop_latency = 16;
			rinfo->ram.rloop = 16;
	
			break;
		case 1:
			/* DDR SGRAM */
			strcpy(rinfo->ram_type, "DDR SGRAM");
			rinfo->ram.ml = 4;
			rinfo->ram.mb = 4;
			rinfo->ram.trcd = 3;
			rinfo->ram.trp = 3;
			rinfo->ram.twr = 2;
			rinfo->ram.cl = 3;
			rinfo->ram.tr2w = 1;
			rinfo->ram.loop_latency = 16;
			rinfo->ram.rloop = 16;

			break;
		default:
			/* 64-bit SDR SGRAM */
			strcpy(rinfo->ram_type, "SDR SGRAM 64");
			rinfo->ram.ml = 4;
			rinfo->ram.mb = 8;
			rinfo->ram.trcd = 3;
			rinfo->ram.trp = 3;
			rinfo->ram.twr = 1;
			rinfo->ram.cl = 3;
			rinfo->ram.tr2w = 1;
			rinfo->ram.loop_latency = 17;
			rinfo->ram.rloop = 17;

			break;
	}

	bios_seg = radeon_find_rom(rinfo);
	radeon_get_pllinfo(rinfo, bios_seg);

	printk("radeonfb: ref_clk=%d, ref_div=%d, xclk=%d\n",
		rinfo->pll.ref_clk, rinfo->pll.ref_div, rinfo->pll.xclk);

	RTRACE("radeonfb: probed %s %dk videoram\n", (rinfo->ram_type), (rinfo->video_ram/1024));

 /*****
   VE and M6 have both DVI and CRT ports (for M6 DVI port can be switch to
   DFP port). The DVI port can also be conneted to a CRT with an adapter.
   Here is the definition of ports for this driver---
   (1) If both port are connected, DVI port will be treated as the Primary 
       port (uses CRTC1) and CRT port will be treated as the Secondary port
       (uses CRTC2)
   (2) If only one port is connected, it will treated as the Primary port
       (??? uses CRTC1 ???)
 *****/
	if(rinfo->hasCRTC2) {
	/* Using BIOS scratch registers works with for VE/M6,
	no such registers in regular RADEON!!!*/
		tmp = INREG(RADEON_BIOS_4_SCRATCH);
		/*check Primary (DVI/DFP port)*/
		if(tmp & 0x08) rinfo->dviDispType = MT_DFP;
		else if(tmp & 0x04) rinfo->dviDispType = MT_LCD;
		else if(tmp & 0x0200) rinfo->dviDispType = MT_CRT;
		else if(tmp & 0x10) rinfo->dviDispType = MT_CTV;
		else if(tmp & 0x20) rinfo->dviDispType = MT_STV;
		/*check Secondary (CRT port).*/
		if(tmp & 0x02) rinfo->crtDispType = MT_CRT;
		else if(tmp & 0x800) rinfo->crtDispType = MT_DFP;
		else if(tmp & 0x400) rinfo->crtDispType = MT_LCD;
		else if(tmp & 0x1000) rinfo->crtDispType = MT_CTV;
		else if(tmp & 0x2000) rinfo->crtDispType = MT_STV;
		if(rinfo->dviDispType == MT_NONE && 
		   rinfo->crtDispType == MT_NONE) {
			printk("radeonfb: No monitor detected!!!\n");
			release_mem_region (rinfo->mmio_base_phys,
					    pci_resource_len(pdev, 2));
			release_mem_region (rinfo->fb_base_phys,
					    pci_resource_len(pdev, 0));
			kfree (rinfo);
			return -ENODEV;
		}
	}
	else {
	  /*Regular Radeon ASIC, only one CRTC, but it could be
	    used for DFP with a DVI output, like AIW board*/
		rinfo->dviDispType = MT_NONE;
		tmp = INREG(FP_GEN_CNTL);
		if(tmp & FP_EN_TMDS) rinfo->crtDispType = MT_DFP;
		else rinfo->crtDispType = MT_CRT;
	}

	if(bios_seg) {
/*
  FIXME!!! TVout support currently is incomplete 
  On Radeon VE TVout is recognized as STV monitor on DVI port.
*/
		char * bios_ptr = bios_seg + 0x48L;
		rinfo->hasTVout = readw(bios_ptr+0x32);
	}

	rinfo->fb_base = (u32) ioremap (rinfo->fb_base_phys,
				  		  rinfo->video_ram);
	if (!rinfo->fb_base) {
		printk ("radeonfb: cannot map FB\n");
		iounmap ((void*)rinfo->mmio_base);
		release_mem_region (rinfo->mmio_base_phys,
				    pci_resource_len(pdev, 2));
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

	/* XXX turn off accel for now, blts aren't working right */
	noaccel = 1;

	/* set all the vital stuff */
	radeon_set_fbinfo (rinfo);

	/* save current mode regs before we switch into the new one
	 * so we can restore this upon __exit
	 */
	radeon_save_state (rinfo, &rinfo->init_state);

	/* init palette */
	for (i=0; i<16; i++) {
		j = color_table[i];
		rinfo->palette[i].red = default_red[j];
		rinfo->palette[i].green = default_grn[j];
		rinfo->palette[i].blue = default_blu[j];
	}

	pdev->driver_data = rinfo;

	if (register_framebuffer ((struct fb_info *) rinfo) < 0) {
		printk ("radeonfb: could not register framebuffer\n");
		iounmap ((void*)rinfo->fb_base);
		iounmap ((void*)rinfo->mmio_base);
		release_mem_region (rinfo->mmio_base_phys,
				    pci_resource_len(pdev, 2));
		release_mem_region (rinfo->fb_base_phys,
				    pci_resource_len(pdev, 0));
		kfree (rinfo);
		return -ENODEV;
	}

	if (!noaccel) {
		/* initialize the engine */
		radeon_engine_init (rinfo);
	}

	printk ("radeonfb: ATI %s %s %d MB\n",rinfo->name,rinfo->ram_type,
		(rinfo->video_ram/(1024*1024)));
	if(rinfo->hasCRTC2) {
	    printk("radeonfb: DVI port has %s monitor connected\n",GET_MON_NAME(rinfo->dviDispType));
	    printk("radeonfb: CRT port has %s monitor connected\n",GET_MON_NAME(rinfo->crtDispType));
	}
	else
	    printk("radeonfb: CRT port has %s monitor connected\n",GET_MON_NAME(rinfo->crtDispType));
	printk("radeonfb: This card has %sTVout\n",rinfo->hasTVout ? "" : "no ");
#ifdef CONFIG_MTRR
	if (mtrr) {
		rinfo->mtrr.vram = mtrr_add(rinfo->fb_base_phys,
				rinfo->video_ram, MTRR_TYPE_WRCOMB, 1);
		rinfo->mtrr.vram_valid = 1;
		/* let there be speed */
		printk("radeonfb: MTRR set to ON\n");
	}
#endif /* CONFIG_MTRR */

	return 0;
}



static void __devexit radeonfb_pci_unregister (struct pci_dev *pdev)
{
        struct radeonfb_info *rinfo = pdev->driver_data;
 
        if (!rinfo)
                return;
 
	/* restore original state */
        radeon_write_mode (rinfo, &rinfo->init_state);
 
        unregister_framebuffer ((struct fb_info *) rinfo);
#ifdef CONFIG_MTRR
        if (rinfo->mtrr.vram_valid)
            mtrr_del(rinfo->mtrr.vram, rinfo->fb_base_phys,
                     rinfo->video_ram);
#endif /* CONFIG_MTRR */
        iounmap ((void*)rinfo->mmio_base);
        iounmap ((void*)rinfo->fb_base);
 
	release_mem_region (rinfo->mmio_base_phys,
			    pci_resource_len(pdev, 2));
	release_mem_region (rinfo->fb_base_phys,
			    pci_resource_len(pdev, 0));
        
        kfree (rinfo);
}



static char *radeon_find_rom(struct radeonfb_info *rinfo)
{
#if defined(__i386__)
        u32  segstart;
        char *rom_base;
        char *rom;
        int  stage;
        int  i,j;
        char aty_rom_sig[] = "761295520";
        char *radeon_sig[] = {
	  "RG6",
	  "RADEON"
	};

	for(segstart=0x000c0000; segstart<0x000f0000; segstart+=0x00001000) {   
                stage = 1;
                                
                rom_base = (char *)ioremap(segstart, 0x1000);
                                        
                if ((*rom_base == 0x55) && (((*(rom_base + 1)) & 0xff) == 0xaa))
                        stage = 2;
                
                        
                if (stage != 2) { 
                        iounmap(rom_base);
                        continue;
                }

                rom = rom_base;

                for (i = 0; (i < 128 - strlen(aty_rom_sig)) && (stage != 3); i++) {
                        if (aty_rom_sig[0] == *rom)
                                if (strncmp(aty_rom_sig, rom,
                                                strlen(aty_rom_sig)) == 0)
                                        stage = 3;
                        rom++;
                }  
                if (stage != 3) {
                        iounmap(rom_base);
                        continue; 
                }
                rom = rom_base;
                
                for (i = 0; (i < 512) && (stage != 4); i++) {
		    for(j = 0;j < sizeof(radeon_sig)/sizeof(char *);j++) {
                        if (radeon_sig[j][0] == *rom)
                                if (strncmp(radeon_sig[j], rom,
                                            strlen(radeon_sig[j])) == 0) {
                                              stage = 4;
					      break;
					    }
		    }
                        rom++;
                }
                if (stage != 4) {
                        iounmap(rom_base);
                        continue;
                }
                
                return rom_base;
        }
#endif                        
        return NULL;
}



static void radeon_get_pllinfo(struct radeonfb_info *rinfo, char *bios_seg)
{
        void *bios_header;
        void *header_ptr;
        u16 bios_header_offset, pll_info_offset;
        PLL_BLOCK pll;

	if (bios_seg) {
	        bios_header = bios_seg + 0x48L;
       		header_ptr  = bios_header;
        
        	bios_header_offset = readw(header_ptr);
	        bios_header = bios_seg + bios_header_offset;
        	bios_header += 0x30;
        
        	header_ptr = bios_header;
        	pll_info_offset = readw(header_ptr);
        	header_ptr = bios_seg + pll_info_offset;
        
        	memcpy_fromio(&pll, header_ptr, 50);
        
        	rinfo->pll.xclk = (u32)pll.XCLK;
        	rinfo->pll.ref_clk = (u32)pll.PCLK_ref_freq;
        	rinfo->pll.ref_div = (u32)pll.PCLK_ref_divider;
        	rinfo->pll.ppll_min = pll.PCLK_min_freq;
        	rinfo->pll.ppll_max = pll.PCLK_max_freq;
	} else {
		/* no BIOS or BIOS not found, use defaults */

		rinfo->pll.ppll_max = 35000;
		rinfo->pll.ppll_min = 12000;
		rinfo->pll.xclk = 16600;
		rinfo->pll.ref_div = 67;
		rinfo->pll.ref_clk = 2700;
	}
}

static void radeon_engine_init (struct radeonfb_info *rinfo)
{
	u32 temp;

	/* disable 3D engine */
	OUTREG(RB3D_CNTL, 0);

	radeon_engine_reset ();

	radeon_fifo_wait (1);
	OUTREG(DSTCACHE_MODE, 0);

	/* XXX */
	rinfo->pitch = ((rinfo->xres * (rinfo->depth / 8) + 0x3f)) >> 6;

	radeon_fifo_wait (1);
	temp = INREG(DEFAULT_PITCH_OFFSET);
	OUTREG(DEFAULT_PITCH_OFFSET, ((temp & 0xc0000000) | 
				      (rinfo->pitch << 0x16)));

	radeon_fifo_wait (1);
	OUTREGP(DP_DATATYPE, 0, ~HOST_BIG_ENDIAN_EN);

	radeon_fifo_wait (1);
	OUTREG(DEFAULT_SC_BOTTOM_RIGHT, (DEFAULT_SC_RIGHT_MAX |
					 DEFAULT_SC_BOTTOM_MAX));

	temp = radeon_get_dstbpp(rinfo->depth);
	rinfo->dp_gui_master_cntl = ((temp << 8) | GMC_CLR_CMP_CNTL_DIS);
	radeon_fifo_wait (1);
	OUTREG(DP_GUI_MASTER_CNTL, (rinfo->dp_gui_master_cntl |
				    GMC_BRUSH_SOLID_COLOR |
				    GMC_SRC_DATATYPE_COLOR));

	radeon_fifo_wait (7);

	/* clear line drawing regs */
	OUTREG(DST_LINE_START, 0);
	OUTREG(DST_LINE_END, 0);

	/* set brush color regs */
	OUTREG(DP_BRUSH_FRGD_CLR, 0xffffffff);
	OUTREG(DP_BRUSH_BKGD_CLR, 0x00000000);

	/* set source color regs */
	OUTREG(DP_SRC_FRGD_CLR, 0xffffffff);
	OUTREG(DP_SRC_BKGD_CLR, 0x00000000);

	/* default write mask */
	OUTREG(DP_WRITE_MSK, 0xffffffff);

	radeon_engine_idle ();
}



static int __devinit radeon_set_fbinfo (struct radeonfb_info *rinfo)
{
	struct fb_info *info;

	info = &rinfo->info;

	strcpy (info->modename, rinfo->name);
        info->node = -1;
        info->flags = FBINFO_FLAG_DEFAULT;
        info->fbops = &radeon_fb_ops;
        info->display_fg = NULL;
        strncpy (info->fontname, fontname, sizeof (info->fontname));
        info->fontname[sizeof (info->fontname) - 1] = 0;
        info->changevar = NULL;
        info->switch_con = radeonfb_switch;
        info->updatevar = radeonfb_updatevar;
        info->blank = radeonfb_blank;

        if (radeon_init_disp (rinfo) < 0)
                return -1;   

        return 0;
}



static int __devinit radeon_init_disp (struct radeonfb_info *rinfo)
{
        struct fb_info *info;
        struct display *disp;

        info = &rinfo->info;
        disp = &rinfo->disp;
        
        disp->var = radeonfb_default_var;
        info->disp = disp;

        radeon_set_dispsw (rinfo, disp);

	if (noaccel)
	        disp->scrollmode = SCROLL_YREDRAW;
	else
		disp->scrollmode = 0;
        
        rinfo->currcon_display = disp;

        if ((radeon_init_disp_var (rinfo)) < 0)
                return -1;
        
        return 0;
}



static int radeon_init_disp_var (struct radeonfb_info *rinfo)
{
#ifndef MODULE
        if (mode_option)
                fb_find_mode (&rinfo->disp.var, &rinfo->info, mode_option,
                              NULL, 0, NULL, 8);
        else
#endif
                fb_find_mode (&rinfo->disp.var, &rinfo->info, "640x480-8@60",
                              NULL, 0, NULL, 0);

	if (noaccel)
		rinfo->disp.var.accel_flags &= ~FB_ACCELF_TEXT;
	else
		rinfo->disp.var.accel_flags |= FB_ACCELF_TEXT;
 
        return 0;
}


static void radeon_set_dispsw (struct radeonfb_info *rinfo, struct display *disp)
{
	int accel;

	accel = disp->var.accel_flags & FB_ACCELF_TEXT;
        
        disp->dispsw_data = NULL;

	disp->screen_base = (char*)rinfo->fb_base;
        disp->type = FB_TYPE_PACKED_PIXELS;
        disp->type_aux = 0;
        disp->ypanstep = 1;
        disp->ywrapstep = 0;
        disp->can_soft_blank = 1;
        disp->inverse = 0;   

	rinfo->depth = disp->var.bits_per_pixel;	
        switch (disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
                case 8:
                        disp->dispsw = &fbcon_cfb8;
		        disp->visual = FB_VISUAL_PSEUDOCOLOR;
			disp->line_length = disp->var.xres_virtual;
                        break;
#endif			
#ifdef FBCON_HAS_CFB16
                case 16:
                        disp->dispsw = &fbcon_cfb16;
                        disp->dispsw_data = &rinfo->con_cmap.cfb16;
			disp->visual = FB_VISUAL_DIRECTCOLOR;
			disp->line_length = disp->var.xres_virtual * 2;
                        break;
#endif
#ifdef FBCON_HAS_CFB32
                case 24:
                        disp->dispsw = &fbcon_cfb24;
                        disp->dispsw_data = &rinfo->con_cmap.cfb24;
			disp->visual = FB_VISUAL_DIRECTCOLOR;
			disp->line_length = disp->var.xres_virtual * 4;
                        break;
#endif
#ifdef FBCON_HAS_CFB32
                case 32:
                        disp->dispsw = &fbcon_cfb32;
                        disp->dispsw_data = &rinfo->con_cmap.cfb32;
			disp->visual = FB_VISUAL_DIRECTCOLOR;
			disp->line_length = disp->var.xres_virtual * 4;
                        break;
#endif
                default:
                        printk ("radeonfb: setting fbcon_dummy renderer\n");
                        disp->dispsw = &fbcon_dummy;
        }
        
        return;
}



/*
 * fb ops
 */

static int radeonfb_get_fix (struct fb_fix_screeninfo *fix, int con,
                             struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;  
        
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];

        memset (fix, 0, sizeof (struct fb_fix_screeninfo));
	strcpy (fix->id, rinfo->name);
        
        fix->smem_start = rinfo->fb_base_phys;
        fix->smem_len = rinfo->video_ram;

        fix->type = disp->type;
        fix->type_aux = disp->type_aux;
        fix->visual = disp->visual;

        fix->xpanstep = 1;
        fix->ypanstep = 1;
        fix->ywrapstep = 0;
        
        fix->line_length = disp->line_length;
 
        fix->mmio_start = rinfo->mmio_base_phys;
        fix->mmio_len = RADEON_REGSIZE;
	if (noaccel)
	        fix->accel = FB_ACCEL_NONE;
	else
		fix->accel = 40;	/* XXX */
        
        return 0;
}



static int radeonfb_get_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        
        *var = (con < 0) ? rinfo->disp.var : fb_display[con].var;
        
        return 0;
}



static int radeonfb_set_var (struct fb_var_screeninfo *var, int con,
                             struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;
        struct fb_var_screeninfo v;
        int nom, den, accel;
        unsigned chgvar = 0;
 
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];

	accel = var->accel_flags & FB_ACCELF_TEXT;
  
        if (con >= 0) {
                chgvar = ((disp->var.xres != var->xres) ||
                          (disp->var.yres != var->yres) ||
                          (disp->var.xres_virtual != var->xres_virtual) ||
                          (disp->var.yres_virtual != var->yres_virtual) ||
			  (disp->var.bits_per_pixel != var->bits_per_pixel) ||
                          memcmp (&disp->var.red, &var->red, sizeof (var->red)) ||
                          memcmp (&disp->var.green, &var->green, sizeof (var->green)) ||
                          memcmp (&disp->var.blue, &var->blue, sizeof (var->blue)));
        }
                
        memcpy (&v, var, sizeof (v));
        
        switch (v.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
                case 8:
                        nom = den = 1;
                        disp->line_length = v.xres_virtual;
                        disp->visual = FB_VISUAL_PSEUDOCOLOR;
                        v.red.offset = v.green.offset = v.blue.offset = 0;
                        v.red.length = v.green.length = v.blue.length = 8;
			v.transp.offset = v.transp.length = 0;
                        break;
#endif
                          
#ifdef FBCON_HAS_CFB16
                case 16:
                        nom = 2;
                        den = 1;
                        disp->line_length = v.xres_virtual * 2;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;
                        v.red.offset = 11;
                        v.green.offset = 5;
                        v.blue.offset = 0;
			v.red.length = 5;
			v.green.length = 6;
			v.blue.length = 5;
			v.transp.offset = v.transp.length = 0;
                        break;
#endif

#ifdef FBCON_HAS_CFB24    
                case 24:
                        nom = 4;
                        den = 1;
                        disp->line_length = v.xres_virtual * 3;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;  
                        v.red.offset = 16;
                        v.green.offset = 8;
                        v.blue.offset = 0; 
                        v.red.length = v.blue.length = v.green.length = 8;
			v.transp.offset = v.transp.length = 0;
                        break;
#endif
#ifdef FBCON_HAS_CFB32    
                case 32:
                        nom = 4;
                        den = 1;
                        disp->line_length = v.xres_virtual * 4;
                        disp->visual = FB_VISUAL_DIRECTCOLOR;  
                        v.red.offset = 16;
                        v.green.offset = 8;
                        v.blue.offset = 0; 
                        v.red.length = v.blue.length = v.green.length = 8;
			v.transp.offset = 24;
			v.transp.length = 8;
                        break;
#endif
                default:
                        printk ("radeonfb: mode %dx%dx%d rejected, color depth invalid\n",
                                var->xres, var->yres, var->bits_per_pixel);
                        return -EINVAL;
        }

	if (radeonfb_do_maximize(rinfo, var, &v, nom, den) < 0)
		return -EINVAL;
  
        if (v.xoffset < 0)
                v.xoffset = 0;
        if (v.yoffset < 0)
                v.yoffset = 0;
                                
        if (v.xoffset > v.xres_virtual - v.xres)
                v.xoffset = v.xres_virtual - v.xres - 1;
                        
        if (v.yoffset > v.yres_virtual - v.yres)
                v.yoffset = v.yres_virtual - v.yres - 1;
                
        v.red.msb_right = v.green.msb_right = v.blue.msb_right =
                          v.transp.offset = v.transp.length =
                          v.transp.msb_right = 0;
                        
        switch (v.activate & FB_ACTIVATE_MASK) {
                case FB_ACTIVATE_TEST:
                        return 0;
                case FB_ACTIVATE_NXTOPEN:
                case FB_ACTIVATE_NOW:
                        break;
                default:        
                        return -EINVAL;
        }
                        
        memcpy (&disp->var, &v, sizeof (v));

	if (chgvar) {
		radeon_set_dispsw(rinfo, disp);

		if (noaccel)
			disp->scrollmode = SCROLL_YREDRAW;
		else
			disp->scrollmode = 0;

		if (info && info->changevar)
			info->changevar(con);
	}

	radeon_load_video_mode (rinfo, &v);

	do_install_cmap(con, info);

        return 0;
}



static int radeonfb_get_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;
                
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];
        
        if (con == rinfo->currcon) {
                int rc = fb_get_cmap (cmap, kspc, radeon_getcolreg, info);
                return rc;
        } else if (disp->cmap.len)
                fb_copy_cmap (&disp->cmap, cmap, kspc ? 0 : 2);
        else
                fb_copy_cmap (fb_default_cmap (radeon_get_cmap_len (&disp->var)),
                              cmap, kspc ? 0 : 2);
                        
        return 0;
}



static int radeonfb_set_cmap (struct fb_cmap *cmap, int kspc, int con,
                              struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;
        unsigned int cmap_len;
                
        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];
  
        cmap_len = radeon_get_cmap_len (&disp->var);
        if (disp->cmap.len != cmap_len) {
                int err = fb_alloc_cmap (&disp->cmap, cmap_len, 0);
                if (err)
                        return err;
        }
 
        if (con == rinfo->currcon) {
                int rc = fb_set_cmap (cmap, kspc, radeon_setcolreg, info);
                return rc;
        } else
                fb_copy_cmap (cmap, &disp->cmap, kspc ? 0 : 1);
        
        return 0;
}               



static int radeonfb_pan_display (struct fb_var_screeninfo *var, int con,
                                 struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        u32 offset, xoffset, yoffset;

	xoffset = (var->xoffset + 7) & ~7;
	yoffset = var->yoffset;

	if ((xoffset + var->xres > var->xres_virtual) || (yoffset+var->yres >
		var->yres_virtual))
		return -EINVAL;

	offset = ((yoffset * var->xres + xoffset) * var->bits_per_pixel) >> 6;

	OUTREG(CRTC_OFFSET, offset);
        
        return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;

        if (con != rinfo->currcon)
		return;

	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, radeon_setcolreg, info);
	else {
		int size = fb_display[con].var.bits_per_pixel == 8 ? 256 : 32;
		fb_set_cmap(fb_default_cmap(size), 1, radeon_setcolreg, info);
	}
}


static int radeonfb_do_maximize(struct radeonfb_info *rinfo,
				struct fb_var_screeninfo *var,
				struct fb_var_screeninfo *v,
				int nom, int den)
{
	static struct {
		int xres, yres;
	} modes[] = {
		{1600, 1280},
		{1280, 1024},
		{1024, 768},
		{800, 600},
		{640, 480},
		{-1, -1}
	};
	int i;

	/* use highest possible virtual resolution */
	if (v->xres_virtual == -1 && v->yres_virtual == -1) {
		printk("radeonfb: using max availabe virtual resolution\n");
		for (i=0; modes[i].xres != -1; i++) {
			if (modes[i].xres * nom / den * modes[i].yres <
			    rinfo->video_ram / 2)
				break;
		}
		if (modes[i].xres == -1) {
			printk("radeonfb: could not find virtual resolution that fits into video memory!\n");
			return -EINVAL;
		}
		v->xres_virtual = modes[i].xres;
		v->yres_virtual = modes[i].yres;

		printk("radeonfb: virtual resolution set to max of %dx%d\n",
			v->xres_virtual, v->yres_virtual);
	} else if (v->xres_virtual == -1) {
		v->xres_virtual = (rinfo->video_ram * den / 
				(nom * v->yres_virtual * 2)) & ~15;
	} else if (v->yres_virtual == -1) {
		v->xres_virtual = (v->xres_virtual + 15) & ~15;
		v->yres_virtual = rinfo->video_ram * den /
			(nom * v->xres_virtual *2);
	} else {
		if (v->xres_virtual * nom / den * v->yres_virtual >
			rinfo->video_ram) {
			return -EINVAL;
		}
	}

	if (v->xres_virtual * nom / den >= 8192) {
		v->xres_virtual = 8192 * den / nom - 16;
	}

	if (v->xres_virtual < v->xres)
		return -EINVAL;

	if (v->yres_virtual < v->yres)
		return -EINVAL;

	return 0;
}


static int radeonfb_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                           unsigned long arg, int con, struct fb_info *info)
{
	return -EINVAL;
}



static int radeonfb_switch (int con, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
        struct display *disp;
        struct fb_cmap *cmap;
	int switchcon = 0;

        disp = (con < 0) ? rinfo->info.disp : &fb_display[con];
 
        if (rinfo->currcon >= 0) {
                cmap = &(rinfo->currcon_display->cmap);
                if (cmap->len)
                        fb_get_cmap (cmap, 1, radeon_getcolreg, info);
        }

	if ((disp->var.xres != rinfo->xres) ||
	    (disp->var.yres != rinfo->yres) ||
	    (disp->var.pixclock != rinfo->pixclock) ||
	    (disp->var.bits_per_pixel != rinfo->depth))
		switchcon = 1;

	if (switchcon) {
	        rinfo->currcon = con;
       		rinfo->currcon_display = disp;
        	disp->var.activate = FB_ACTIVATE_NOW;

        	radeonfb_set_var (&disp->var, con, info);
        	radeon_set_dispsw (rinfo, disp);
		do_install_cmap(con, info);
	}

	/* XXX absurd hack for X to restore console */
	{
		OUTREG(CRTC_EXT_CNTL, rinfo->hack_crtc_ext_cntl);
		OUTREG(CRTC_V_SYNC_STRT_WID, rinfo->hack_crtc_v_sync_strt_wid);
	}

        return 0;
}



static int radeonfb_updatevar (int con, struct fb_info *info)
{
        int rc;
                
        rc = (con < 0) ? -EINVAL : radeonfb_pan_display (&fb_display[con].var,
                                                         con, info);
        
        return rc;
}

static void radeonfb_blank (int blank, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
	u32 val = INREG(CRTC_EXT_CNTL);

	/* reset it */
	val &= ~(CRTC_DISPLAY_DIS | CRTC_HSYNC_DIS |
		 CRTC_VSYNC_DIS);

	switch (blank) {
		case VESA_NO_BLANKING:
			break;
		case VESA_VSYNC_SUSPEND:
			val |= (CRTC_DISPLAY_DIS | CRTC_VSYNC_DIS);
			break;
		case VESA_HSYNC_SUSPEND:
			val |= (CRTC_DISPLAY_DIS | CRTC_HSYNC_DIS);
			break;
		case VESA_POWERDOWN:
			val |= (CRTC_DISPLAY_DIS | CRTC_VSYNC_DIS |
				CRTC_HSYNC_DIS);
			break;
	}
	if(blank == VESA_NO_BLANKING && rinfo->hasCRTC2)
		    OUTREGP(CRTC_EXT_CNTL,CRTC_CRT_ON, val);
	else OUTREG(CRTC_EXT_CNTL, val);
}



static int radeon_get_cmap_len (const struct fb_var_screeninfo *var)
{
        int rc = 16;            /* reasonable default */
        
        switch (var->bits_per_pixel) {
                case 8:
                        rc = 256;
                        break;
		case 16:
			rc = 64;
			break;
                default:
                        rc = 32;
                        break;
        }
                
        return rc;
}



static int radeon_getcolreg (unsigned regno, unsigned *red, unsigned *green,
                             unsigned *blue, unsigned *transp,
                             struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
	
	if (regno > 255)
		return 1;
     
 	*red = (rinfo->palette[regno].red<<8) | rinfo->palette[regno].red; 
    	*green = (rinfo->palette[regno].green<<8) | rinfo->palette[regno].green;
    	*blue = (rinfo->palette[regno].blue<<8) | rinfo->palette[regno].blue;
    	*transp = 0;

	return 0;
}                            



static int radeon_setcolreg (unsigned regno, unsigned red, unsigned green,
                             unsigned blue, unsigned transp, struct fb_info *info)
{
        struct radeonfb_info *rinfo = (struct radeonfb_info *) info;
	u32 pindex;

	if (regno > 255)
		return 1;

	red >>= 8;
	green >>= 8;
	blue >>= 8;
	rinfo->palette[regno].red = red;
	rinfo->palette[regno].green = green;
	rinfo->palette[regno].blue = blue;

	/* init gamma for hicolor */
	if ((rinfo->depth > 8) && (regno == 0)) {
		int i;

		for (i=0; i<255; i++) {
			OUTREG(PALETTE_INDEX, i);
			OUTREG(PALETTE_DATA, (i << 16) | (i << 8) | i);
		}
	}

	/* default */
	pindex = regno;

	/* XXX actually bpp, fixme */
	if (rinfo->depth == 16)
		pindex  = regno * 8;

	if (rinfo->depth == 16) {
		OUTREG(PALETTE_INDEX, pindex/2);
		OUTREG(PALETTE_DATA, (rinfo->palette[regno/2].red << 16) |
			(green << 8) | (rinfo->palette[regno/2].blue));
		green = rinfo->palette[regno/2].green;
	}

	if ((rinfo->depth == 8) || (regno < 32)) {
		OUTREG(PALETTE_INDEX, pindex);
		OUTREG(PALETTE_DATA, (red << 16) | (green << 8) | blue);
	}

#if 0
	col = (red << 16) | (green << 8) | blue;

	if (rinfo->depth == 16) {
		pindex = regno << 3;

		if ((rinfo->depth == 16) && (regno >= 32)) {
			pindex -= 252;

			col = (rinfo->palette[regno >> 1].red << 16) |
					(green << 8) |
					(rinfo->palette[regno >> 1].blue);
		} else {
			col = (red << 16) | (green << 8) | blue;
		}
	}
	
	OUTREG8(PALETTE_INDEX, pindex);
	radeon_fifo_wait(32);
	OUTREG(PALETTE_DATA, col);
#endif

#if defined(FBCON_HAS_CFB16) || defined(FBCON_HAS_CFB32)
 	if (regno < 32) {
        	switch (rinfo->depth) {
#ifdef FBCON_HAS_CFB16
		        case 16:
        			rinfo->con_cmap.cfb16[regno] = (regno << 11) | (regno << 5) |
				                       	 	  regno;   
			        break;
#endif
#ifdef FBCON_HAS_CFB24
		        case 24:
        			rinfo->con_cmap.cfb24[regno] = (regno << 16) | (regno << 8) | regno;
			        break;
#endif
#ifdef FBCON_HAS_CFB32
	        	case 32: {
            			u32 i;    
   
  		       		i = (regno << 8) | regno;
            			rinfo->con_cmap.cfb32[regno] = (i << 16) | i;
		        	break;
        		}
#endif
		}
        }
#endif
	return 0;
}



static void radeon_save_state (struct radeonfb_info *rinfo,
                               struct radeon_regs *save)
{
	save->crtc_gen_cntl = INREG(CRTC_GEN_CNTL);
	save->crtc_ext_cntl = INREG(CRTC_EXT_CNTL);
	save->dac_cntl = INREG(DAC_CNTL);
        save->crtc_h_total_disp = INREG(CRTC_H_TOTAL_DISP);
        save->crtc_h_sync_strt_wid = INREG(CRTC_H_SYNC_STRT_WID);
        save->crtc_v_total_disp = INREG(CRTC_V_TOTAL_DISP);
        save->crtc_v_sync_strt_wid = INREG(CRTC_V_SYNC_STRT_WID);
	save->crtc_pitch = INREG(CRTC_PITCH);
}



static void radeon_load_video_mode (struct radeonfb_info *rinfo,
                                    struct fb_var_screeninfo *mode)
{
	struct radeon_regs newmode;
	int hTotal, vTotal, hSyncStart, hSyncEnd,
	    hSyncPol, vSyncStart, vSyncEnd, vSyncPol, cSync;
	u8 hsync_adj_tab[] = {0, 0x12, 9, 9, 6, 5};
	u32 dotClock = 1000000000 / mode->pixclock,
	    sync, h_sync_pol, v_sync_pol;
	int freq = dotClock / 10;  /* x 100 */
        int xclk_freq, vclk_freq, xclk_per_trans, xclk_per_trans_precise;
        int useable_precision, roff, ron;
        int min_bits, format = 0;
	int hsync_start, hsync_fudge, bytpp, hsync_wid, vsync_wid;

	rinfo->xres = mode->xres;
	rinfo->yres = mode->yres;
	rinfo->pixclock = mode->pixclock;

	hSyncStart = mode->xres + mode->right_margin;
	hSyncEnd = hSyncStart + mode->hsync_len;
	hTotal = hSyncEnd + mode->left_margin;

	vSyncStart = mode->yres + mode->lower_margin;
	vSyncEnd = vSyncStart + mode->vsync_len;
	vTotal = vSyncEnd + mode->upper_margin;

	sync = mode->sync;
	h_sync_pol = sync & FB_SYNC_HOR_HIGH_ACT ? 0 : 1;
	v_sync_pol = sync & FB_SYNC_VERT_HIGH_ACT ? 0 : 1;

	RTRACE("hStart = %d, hEnd = %d, hTotal = %d\n",
		hSyncStart, hSyncEnd, hTotal);
	RTRACE("vStart = %d, vEnd = %d, vTotal = %d\n",
		vSyncStart, vSyncEnd, vTotal);

	hsync_wid = (hSyncEnd - hSyncStart) / 8;
	vsync_wid = vSyncEnd - vSyncStart;
	if (hsync_wid == 0)
		hsync_wid = 1;
	else if (hsync_wid > 0x3f)	/* max */
		hsync_wid = 0x3f;
	vsync_wid = mode->vsync_len;
	if (vsync_wid == 0)
		vsync_wid = 1;
	else if (vsync_wid > 0x1f)	/* max */
		vsync_wid = 0x1f;

	hSyncPol = mode->sync & FB_SYNC_HOR_HIGH_ACT ? 0 : 1;
	vSyncPol = mode->sync & FB_SYNC_VERT_HIGH_ACT ? 0 : 1;

	cSync = mode->sync & FB_SYNC_COMP_HIGH_ACT ? (1 << 4) : 0;

	switch (mode->bits_per_pixel) {
		case 8:
			format = DST_8BPP;
			bytpp = 1;
			break;
		case 16:
			format = DST_16BPP;
			bytpp = 2;
			break;
		case 24:
			format = DST_24BPP;
			bytpp = 3;
			break;
		case 32:
			format = DST_32BPP;
			bytpp = 4;
			break;
	}

	hsync_fudge = hsync_adj_tab[format-1];
	hsync_start = hSyncStart - 8 + hsync_fudge;

	newmode.crtc_gen_cntl = CRTC_EXT_DISP_EN | CRTC_EN |
				(format << 8);
	if(rinfo->hasCRTC2) 
        /* HACKED: !!! Enable CRT port here !!! */
	     newmode.crtc_ext_cntl = VGA_ATI_LINEAR | XCRT_CNT_EN | CRTC_CRT_ON;
	else newmode.crtc_ext_cntl = VGA_ATI_LINEAR | XCRT_CNT_EN;
	newmode.dac_cntl = INREG(DAC_CNTL) | DAC_MASK_ALL | DAC_VGA_ADR_EN |
			   DAC_8BIT_EN;

	newmode.crtc_h_total_disp = ((((hTotal / 8) - 1) & 0x3ff) |
				     ((((mode->xres / 8) - 1) & 0x1ff) << 16));

	newmode.crtc_h_sync_strt_wid = ((hsync_start & 0x1fff) |
					(hsync_wid << 16) | (h_sync_pol << 23));

	newmode.crtc_v_total_disp = ((vTotal - 1) & 0xffff) |
				    ((mode->yres - 1) << 16);

	newmode.crtc_v_sync_strt_wid = (((vSyncStart - 1) & 0xfff) |
					 (vsync_wid << 16) | (v_sync_pol  << 23));

	newmode.crtc_pitch = (mode->xres >> 3);

#if defined(__BIG_ENDIAN)
	newmode.surface_cntl = SURF_TRANSLATION_DIS;
	switch (mode->bits_per_pixel) {
		case 16:
			newmode.surface_cntl |= NONSURF_AP0_SWP_16BPP;
			break;
		case 24:
		case 32:
			newmode.surface_cntl |= NONSURF_AP0_SWP_32BPP;
			break;
	}
#endif

	rinfo->pitch = ((mode->xres * ((mode->bits_per_pixel + 1) / 8) + 0x3f)
			& ~(0x3f)) / 64;

	RTRACE("h_total_disp = 0x%x\t   hsync_strt_wid = 0x%x\n",
		newmode.crtc_h_total_disp, newmode.crtc_h_sync_strt_wid);
	RTRACE("v_total_disp = 0x%x\t   vsync_strt_wid = 0x%x\n",
		newmode.crtc_v_total_disp, newmode.crtc_v_sync_strt_wid);

	newmode.xres = mode->xres;
	newmode.yres = mode->yres;

	rinfo->bpp = mode->bits_per_pixel;
	rinfo->hack_crtc_ext_cntl = newmode.crtc_ext_cntl;
	rinfo->hack_crtc_v_sync_strt_wid = newmode.crtc_v_sync_strt_wid;

	if (freq > rinfo->pll.ppll_max)
		freq = rinfo->pll.ppll_max;
	if (freq*12 < rinfo->pll.ppll_min)
		freq = rinfo->pll.ppll_min / 12;

	{
		struct {
			int divider;
			int bitvalue;
		} *post_div,
		  post_divs[] = {
			{ 1,  0 },
			{ 2,  1 },
			{ 4,  2 },
			{ 8,  3 },
			{ 3,  4 },
			{ 16, 5 },
			{ 6,  6 },
			{ 12, 7 },
			{ 0,  0 },
		};

		for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
			rinfo->pll_output_freq = post_div->divider * freq;
			if (rinfo->pll_output_freq >= rinfo->pll.ppll_min  &&
			    rinfo->pll_output_freq <= rinfo->pll.ppll_max)
				break;
		}

		rinfo->post_div = post_div->divider;
		rinfo->fb_div = round_div(rinfo->pll.ref_div*rinfo->pll_output_freq,
					  rinfo->pll.ref_clk);
		newmode.ppll_ref_div = rinfo->pll.ref_div;
		newmode.ppll_div_3 = rinfo->fb_div | (post_div->bitvalue << 16);
	}

	RTRACE("post div = 0x%x\n", rinfo->post_div);
	RTRACE("fb_div = 0x%x\n", rinfo->fb_div);
	RTRACE("ppll_div_3 = 0x%x\n", newmode.ppll_div_3);

	/* DDA */
	vclk_freq = round_div(rinfo->pll.ref_clk * rinfo->fb_div,
			      rinfo->pll.ref_div * rinfo->post_div);
	xclk_freq = rinfo->pll.xclk;

	xclk_per_trans = round_div(xclk_freq * 128, vclk_freq * mode->bits_per_pixel);

	min_bits = min_bits_req(xclk_per_trans);
	useable_precision = min_bits + 1;

	xclk_per_trans_precise = round_div((xclk_freq * 128) << (11 - useable_precision),
					   vclk_freq * mode->bits_per_pixel);

	ron = (4 * rinfo->ram.mb + 3 * _max(rinfo->ram.trcd - 2, 0) +
	       2 * rinfo->ram.trp + rinfo->ram.twr + rinfo->ram.cl + rinfo->ram.tr2w +
	       xclk_per_trans) << (11 - useable_precision);
	roff = xclk_per_trans_precise * (32 - 4);

	RTRACE("ron = %d, roff = %d\n", ron, roff);
	RTRACE("vclk_freq = %d, per = %d\n", vclk_freq, xclk_per_trans_precise);

	if ((ron + rinfo->ram.rloop) >= roff) {
		printk("radeonfb: error ron out of range\n");
		return;
	}

	newmode.dda_config = (xclk_per_trans_precise |
			      (useable_precision << 16) |
			      (rinfo->ram.rloop << 20));
	newmode.dda_on_off = (ron << 16) | roff;

	/* do it! */
	radeon_write_mode (rinfo, &newmode);
	/* XXX absurd hack for X to restore console on VE */
	if(rinfo->hasCRTC2 && rinfo->crtDispType == MT_CRT &&
	   (rinfo->dviDispType == MT_NONE || rinfo->dviDispType == MT_STV)) {
		OUTREG(CRTC_EXT_CNTL, rinfo->hack_crtc_ext_cntl);
		OUTREG(CRTC_V_SYNC_STRT_WID, rinfo->hack_crtc_v_sync_strt_wid);
	}
	return;
}


    /*****
      When changing mode with Dual-head card (VE/M6), care must
      be taken for the special order in setting registers. CRTC2 has
      to be set before changing CRTC_EXT register.
      Otherwise we may get a blank screen.
    *****/

static void radeon_write_mode (struct radeonfb_info *rinfo,
                               struct radeon_regs *mode)
{
	int i;

	/* blank screen */
	OUTREG8(CRTC_EXT_CNTL + 1, 4);

	for (i=0; i<9; i++)
		OUTREG(common_regs[i].reg, common_regs[i].val);

	OUTREG(CRTC_GEN_CNTL, mode->crtc_gen_cntl);
	OUTREGP(CRTC_EXT_CNTL, mode->crtc_ext_cntl,
		CRTC_HSYNC_DIS | CRTC_VSYNC_DIS | CRTC_DISPLAY_DIS);
	OUTREGP(DAC_CNTL, mode->dac_cntl, DAC_RANGE_CNTL | DAC_BLANKING);
	OUTREG(CRTC_H_TOTAL_DISP, mode->crtc_h_total_disp);
	OUTREG(CRTC_H_SYNC_STRT_WID, mode->crtc_h_sync_strt_wid);
	OUTREG(CRTC_V_TOTAL_DISP, mode->crtc_v_total_disp);
	OUTREG(CRTC_V_SYNC_STRT_WID, mode->crtc_v_sync_strt_wid);
	OUTREG(CRTC_OFFSET, 0);
	OUTREG(CRTC_OFFSET_CNTL, 0);
	OUTREG(CRTC_PITCH, mode->crtc_pitch);
#if defined(__BIG_ENDIAN)
	/* XXX this code makes degradation of mplayer quality on Radeon VE */
	OUTREG(SURFACE_CNTL, mode->surface_cntl);
#endif
/* Here we should restore FP registers for LCD & DFP monitors */

	while ((INREG(CLOCK_CNTL_INDEX) & PPLL_DIV_SEL_MASK) !=
	       PPLL_DIV_SEL_MASK) {
		OUTREGP(CLOCK_CNTL_INDEX, PPLL_DIV_SEL_MASK, 0xffff);
	}

	OUTPLLP(PPLL_CNTL, PPLL_RESET, 0xffff);

	while ((INPLL(PPLL_REF_DIV) & PPLL_REF_DIV_MASK) !=
	       (mode->ppll_ref_div & PPLL_REF_DIV_MASK)) {
		OUTPLLP(PPLL_REF_DIV, mode->ppll_ref_div, ~PPLL_REF_DIV_MASK);
	}

	while ((INPLL(PPLL_DIV_3) & PPLL_FB3_DIV_MASK) !=
	       (mode->ppll_div_3 & PPLL_FB3_DIV_MASK)) {
		OUTPLLP(PPLL_DIV_3, mode->ppll_div_3, ~PPLL_FB3_DIV_MASK);
	}

	while ((INPLL(PPLL_DIV_3) & PPLL_POST3_DIV_MASK) !=
	       (mode->ppll_div_3 & PPLL_POST3_DIV_MASK)) {
		OUTPLLP(PPLL_DIV_3, mode->ppll_div_3, ~PPLL_POST3_DIV_MASK);
	}

	OUTPLL(HTOTAL_CNTL, 0);

	OUTPLLP(PPLL_CNTL, 0, ~PPLL_RESET);

	OUTREG(DDA_CONFIG, mode->dda_config);
	OUTREG(DDA_ON_OFF, mode->dda_on_off);

	/* unblank screen */
	OUTREG8(CRTC_EXT_CNTL + 1, 0);

	return;
}

#if 0

/*
 * text console acceleration
 */


static void fbcon_radeon_bmove(struct display *p, int srcy, int srcx,
			       int dsty, int dstx, int height, int width)
{
	struct radeonfb_info *rinfo = (struct radeonfb_info *)(p->fb_info);
	u32 dp_cntl = DST_LAST_PEL;

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	dstx *= fontwidth(p);
	dsty *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	if (srcy < dsty) {
		srcy += height - 1;
		dsty += height - 1;
	} else
		dp_cntl |= DST_Y_TOP_TO_BOTTOM;

	if (srcx < dstx) {
		srcx += width - 1;
		dstx += width - 1;
	} else
		dp_cntl |= DST_X_LEFT_TO_RIGHT;

	radeon_fifo_wait(6);
	OUTREG(DP_GUI_MASTER_CNTL, (rinfo->dp_gui_master_cntl |
				    GMC_BRUSH_NONE |
				    GMC_SRC_DATATYPE_COLOR |
				    ROP3_S |
				    DP_SRC_SOURCE_MEMORY));
	OUTREG(DP_WRITE_MSK, 0xffffffff);
	OUTREG(DP_CNTL, dp_cntl);
	OUTREG(SRC_Y_X, (srcy << 16) | srcx);
	OUTREG(DST_Y_X, (dsty << 16) | dstx);
	OUTREG(DST_HEIGHT_WIDTH, (height << 16) | width);
}



static void fbcon_radeon_clear(struct vc_data *conp, struct display *p,
			       int srcy, int srcx, int height, int width)
{
	struct radeonfb_info *rinfo = (struct radeonfb_info *)(p->fb_info);
	u32 clr;

	clr = attr_bgcol_ec(p, conp);
	clr |= (clr << 8);
	clr |= (clr << 16);

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	radeon_fifo_wait(6);
	OUTREG(DP_GUI_MASTER_CNTL, (rinfo->dp_gui_master_cntl |
				    GMC_BRUSH_SOLID_COLOR |
				    GMC_SRC_DATATYPE_COLOR |
				    ROP3_P));
	OUTREG(DP_BRUSH_FRGD_CLR, clr);
	OUTREG(DP_WRITE_MSK, 0xffffffff);
	OUTREG(DP_CNTL, (DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM));
	OUTREG(DST_Y_X, (srcy << 16) | srcx);
	OUTREG(DST_WIDTH_HEIGHT, (width << 16) | height);
}




#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_radeon8 = {
	setup:			fbcon_cfb8_setup,
	bmove:			fbcon_radeon_bmove,
	clear:			fbcon_cfb8_clear,
	putc:			fbcon_cfb8_putc,
	putcs:			fbcon_cfb8_putcs,
	revc:			fbcon_cfb8_revc,
	clear_margins:		fbcon_cfb8_clear_margins,
	fontwidthmask:		FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif

#endif /* 0 */
