/*
 *
 * sis_vid.c
 *
 * Copyright (C) 2000 Aaron Holtzman
 * 
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 */

// video4linux interface disabled by A'rpi/ESP-team


//It's entirely possible this major conflicts with something else
/* mknod /dev/mga_vid c 178 0 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10)
#include <linux/malloc.h>
#else
#include <linux/slab.h>
#endif

#include <linux/pci.h>
#include <linux/init.h>
//#include <linux/videodev.h>

#include "sis_vid.h"

#ifdef CONFIG_MTRR 
#include <asm/mtrr.h>
#endif

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>

#define TRUE 1
#define FALSE 0

#define MGA_VID_MAJOR 178


#ifndef PCI_DEVICE_ID_SI_6323 
#define PCI_DEVICE_ID_SI_6323 0x6326
#endif


MODULE_AUTHOR("Aaron Holtzman <aholtzma@engr.uvic.ca>");


typedef struct bes_registers_s
{
	//base address of yuv framebuffer
	uint32_t yuv_base;
	uint32_t u_base;
	uint32_t v_base;
	uint32_t fb_end;;

	//frame buffer pitch 
	uint32_t pitch;

	//window boundaries
	uint32_t left;
	uint32_t right;
	uint32_t top;
	uint32_t bottom;

	//control registers
	uint32_t misc_0;
	uint32_t misc_1;
	uint32_t misc_3;
	uint32_t misc_4;

	//key overlay mode
	uint32_t key_mode;

} bes_registers_t;

static bes_registers_t regs;
static uint32_t mga_vid_in_use = 0;
static uint32_t vid_src_ready = 0;
static uint32_t vid_overlay_on = 0;

static uint8_t *mga_mmio_base = 0;
static uint32_t mga_mem_base = 0; 
static uint32_t mga_src_base = 0;

static uint32_t mga_ram_size = 0;

static struct pci_dev *pci_dev;

//static struct video_window mga_win;
static mga_vid_config_t mga_config; 



// Backend Scaler registers

#define MISC_0 0x98
#define MISC_1 0x99
#define MISC_3 0x9d
#define MISC_4 0xb6




static void mga_vid_frame_sel(int frame)
{
	//we don't need the vcount protection as we're only hitting
	//one register (and it doesn't seem to be double buffered)
	//regs.besctl = (regs.besctl & ~0x07000000) + (frame << 25);
	//writel( regs.besctl, mga_mmio_base + BESCTL ); 
}


#define WRITE_REG(x,y,z) {outb((y),(x));outb((z),(x+1));}
#define READ_REG(x,y) (outb((y),(x)),inb(x+1))
#define VIDEO_ACCEL 0x3d4

static void mga_vid_write_regs(void)
{
	uint32_t foo;

	//unlock the video accel registers
	WRITE_REG(VIDEO_ACCEL,0x80,0x86);
	foo = READ_REG(VIDEO_ACCEL,0x80);

	if(foo != 0xa1)
		return; //something bad happened

	//setup the horizontal window bounds
	WRITE_REG(VIDEO_ACCEL,0x81,regs.left & 0xff);
	WRITE_REG(VIDEO_ACCEL,0x82,regs.right & 0xff);
	WRITE_REG(VIDEO_ACCEL,0x83,(regs.left >> 8) | ((regs.right>>4) & 0x70));

	//setup the vertical window bounds
	WRITE_REG(VIDEO_ACCEL,0x84,regs.top & 0xff);
	WRITE_REG(VIDEO_ACCEL,0x85,regs.bottom & 0xff);
	WRITE_REG(VIDEO_ACCEL,0x86,(regs.top >> 8) | ((regs.bottom>>4) & 0x70));

	//setup the framebuffer base addresses
	WRITE_REG(VIDEO_ACCEL,0x8a,regs.yuv_base & 0xff);
	WRITE_REG(VIDEO_ACCEL,0x8b,(regs.yuv_base >> 8) & 0xff);
	WRITE_REG(VIDEO_ACCEL,0x89,(regs.yuv_base >> 12) & 0xf0);

	WRITE_REG(VIDEO_ACCEL,0xb7,regs.u_base & 0xff);
	WRITE_REG(VIDEO_ACCEL,0xb8,(regs.u_base >> 8) & 0xff);

	WRITE_REG(VIDEO_ACCEL,0xba,regs.v_base & 0xff);
	WRITE_REG(VIDEO_ACCEL,0xbb,(regs.v_base >> 8) & 0xff);
	WRITE_REG(VIDEO_ACCEL,0xb9,((regs.v_base >> 12) & 0xf0) + ((regs.u_base >> 16) & 0xf));

	WRITE_REG(VIDEO_ACCEL,0x8d,regs.fb_end);

	//setup framebuffer pitch
	WRITE_REG(VIDEO_ACCEL,0x8c,regs.pitch & 0xff);
	WRITE_REG(VIDEO_ACCEL,0x8e,(regs.pitch >> 8) & 0x0f);
	WRITE_REG(VIDEO_ACCEL,0xbc,(regs.pitch) & 0xff);
	WRITE_REG(VIDEO_ACCEL,0xbd,((regs.pitch) >> 8) & 0x0f);


	//write key overlay register
	WRITE_REG(VIDEO_ACCEL,0xa9,regs.key_mode);

	WRITE_REG(VIDEO_ACCEL,0x93,0x40);
	WRITE_REG(VIDEO_ACCEL,0x94,1);
	WRITE_REG(VIDEO_ACCEL,0x9e,0);
	WRITE_REG(VIDEO_ACCEL,0x9f,0);

	//write config registers
	WRITE_REG(VIDEO_ACCEL,MISC_0,regs.misc_0);
	WRITE_REG(VIDEO_ACCEL,MISC_1,regs.misc_1);
	WRITE_REG(VIDEO_ACCEL,MISC_3,regs.misc_3);
	WRITE_REG(VIDEO_ACCEL,MISC_4,regs.misc_4);

	//setup the video line buffer
	WRITE_REG(VIDEO_ACCEL,0xa0,(regs.right - regs.left)/ 8);

}

static int mga_vid_set_config(mga_vid_config_t *config)
{
	uint32_t x, y, frame_size;

	x = config->x_org;
	y = config->y_org;

	regs.left =  x;
	regs.right= config->src_width + x;
	regs.top =  y;
	regs.bottom = config->src_height + y;

	printk(KERN_DEBUG "mga_vid: Setting up a %dx%d+%d+%d video window (src %dx%d)\n",
	       config->dest_width, config->dest_height, config->x_org, config->y_org, 
				 config->src_width, config->src_height);

	
	regs.pitch = ((config->src_width + 31) & ~31) / 4 ; 

	//frame size in pixels
	frame_size =  regs.pitch * config->src_height * 4;

	regs.yuv_base = (mga_src_base) >> 2;
	regs.u_base = (mga_src_base + frame_size) >> 2;
	regs.v_base = (mga_src_base + frame_size/4) >> 2;
	regs.fb_end = (mga_src_base + (3*frame_size)/2) >> 14;

	//disable video capture, enable video display, enable graphics display,
	//select yuv format, odd parity
	regs.misc_0 = (1 << 1) + (1<<6) + (1<<4);

	//disable dithering, no filtering, no interrupts
	regs.misc_1 = 0;

	//select 2's complement format YUV for playback
	regs.misc_3 = (1<<1);

	//select 4:2:0 video format, + yuv4:2:2 cpu writes
	regs.misc_4 = (1<<2);

	//disable keying
	regs.key_mode = 0xf;

	mga_vid_write_regs();
	return 0;
}


static int mga_vid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int frame;

	switch(cmd) 
	{
		case MGA_VID_CONFIG:
			//FIXME remove
			printk(KERN_DEBUG "mga_mmio_base = %p\n",mga_mmio_base);
			printk(KERN_DEBUG "mga_mem_base = %08x\n",mga_mem_base);
			//FIXME remove

			printk(KERN_DEBUG "mga_vid: Received configuration\n");

 			if(copy_from_user(&mga_config,(mga_vid_config_t*) arg,sizeof(mga_vid_config_t)))
			{
				printk(KERN_ERR "mga_vid: failed copy from userspace\n");
				return(-EFAULT);
			}

			mga_config.ram_size = mga_ram_size;
			//XXX make it look like a g400
			mga_config.card_type = MGA_G400;;

			if (copy_to_user((mga_vid_config_t *) arg, &mga_config, sizeof(mga_vid_config_t)))
			{
				printk(KERN_ERR "mga_vid: failed copy to userspace\n");
				return(-EFAULT);
			}
			return mga_vid_set_config(&mga_config);	
		break;

		case MGA_VID_ON:
			printk(KERN_DEBUG "mga_vid: Video ON\n");
			vid_src_ready = 1;
			if(vid_overlay_on)
			{
				//regs.besctl |= 1;
				mga_vid_write_regs();
			}
		break;

		case MGA_VID_OFF:
			printk(KERN_DEBUG "mga_vid: Video OFF\n");
			vid_src_ready = 0;   
			//regs.besctl &= ~1;
			mga_vid_write_regs();
		break;
			
		case MGA_VID_FSEL:
			if(copy_from_user(&frame,(int *) arg,sizeof(int)))
			{
				printk(KERN_ERR "mga_vid: FSEL failed copy from userspace\n");
				return(-EFAULT);
			}

			mga_vid_frame_sel(frame);
		break;

	        default:
			printk(KERN_ERR "mga_vid: Invalid ioctl\n");
			return (-EINVAL);
	}
       
	return 0;
}


static int mga_vid_find_card(void)
{
	struct pci_dev *dev = NULL;

	if((dev = pci_find_device(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_6323, NULL)))
	{
		printk(KERN_DEBUG "sis_vid: Found SiS 6326\n");
	}
	else
	{
		printk(KERN_ERR "sis_vid: No supported cards found\n");
		return FALSE;   
	}

	pci_dev = dev;
	
#if LINUX_VERSION_CODE >= 0x020300
	mga_mmio_base = ioremap_nocache(dev->resource[1].start,0x10000);
	mga_mem_base =  dev->resource[0].start;
#else
	mga_mmio_base = ioremap_nocache(dev->base_address[1] & PCI_BASE_ADDRESS_MEM_MASK,0x10000);
	mga_mem_base =  dev->base_address[0] & PCI_BASE_ADDRESS_MEM_MASK;
#endif
	printk(KERN_DEBUG "mga_vid: MMIO at 0x%p\n", mga_mmio_base);
	printk(KERN_DEBUG "mga_vid: Frame Buffer at 0x%08x\n", mga_mem_base);

	//FIXME set ram size properly
	mga_ram_size = 4;
	mga_src_base = (mga_ram_size - 1) * 0x100000;
	
	//FIXME remove
	if(1)
	{
		mga_vid_config_t config ={0,0,256,256,256,256,10,10,0,0,0,0};

		mga_vid_set_config(&config);
		mga_vid_write_regs();
		//regs.misc_0 ^= 2;
		//mga_vid_write_regs();
	}
	//FIXME remove

	return TRUE;
}


static ssize_t mga_vid_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t mga_vid_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static int mga_vid_mmap(struct file *file, struct vm_area_struct *vma)
{

	printk(KERN_DEBUG "mga_vid: mapping video memory into userspace\n");
	if(remap_page_range(vma->vm_start, mga_mem_base + mga_src_base,
		 vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
	{
		printk(KERN_ERR "mga_vid: error mapping video memory\n");
		return(-EAGAIN);
	}

	return(0);
}

static int mga_vid_release(struct inode *inode, struct file *file)
{
	//Close the window just in case
	vid_src_ready = 0;   
	regs.misc_0 &= 0xed;
	mga_vid_write_regs();
	mga_vid_in_use = 0;

	//FIXME put back in!
	//MOD_DEC_USE_COUNT;
	return 0;
}

static long long mga_vid_lseek(struct file *file, long long offset, int origin)
{
	return -ESPIPE;
}					 

static int mga_vid_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);

	if(minor != 0)
	 return(-ENXIO);

	if(mga_vid_in_use == 1) 
		return(-EBUSY);

	mga_vid_in_use = 1;
	//FIXME turn me back on!
	//MOD_INC_USE_COUNT;
	return(0);
}

#if LINUX_VERSION_CODE >= 0x020400
static struct file_operations mga_vid_fops =
{
	llseek:		mga_vid_lseek,
	read:			mga_vid_read,
	write:		mga_vid_write,
	ioctl:		mga_vid_ioctl,
	mmap:			mga_vid_mmap,
	open:			mga_vid_open,
	release: 	mga_vid_release
};
#else
static struct file_operations mga_vid_fops =
{
	mga_vid_lseek,
	mga_vid_read,
	mga_vid_write,
	NULL,
	NULL,
	mga_vid_ioctl,
	mga_vid_mmap,
	mga_vid_open,
	NULL,
	mga_vid_release
};
#endif

#if 0
static long mga_v4l_read(struct video_device *v, char *buf, unsigned long count, 
	int noblock)
{
	return -EINVAL;
}

static long mga_v4l_write(struct video_device *v, const char *buf, unsigned long count, int noblock)
{
	return -EINVAL;
}

static int mga_v4l_open(struct video_device *dev, int mode)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static void mga_v4l_close(struct video_device *dev)
{
	//regs.besctl &= ~1;
	mga_vid_write_regs();
	vid_overlay_on = 0;
	MOD_DEC_USE_COUNT;
	return;
}

static int mga_v4l_init_done(struct video_device *dev)
{
	return 0;
}

static int mga_v4l_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	switch(cmd)
	{
		case VIDIOCGCAP:
		{
			struct video_capability b;
			strcpy(b.name, "Matrox G200/400");
			b.type = VID_TYPE_SCALES|VID_TYPE_OVERLAY|VID_TYPE_CHROMAKEY;
			b.channels = 0;
			b.audios = 0;
			b.maxwidth = 1024;	/* GUESS ?? */
			b.maxheight = 768;
			b.minwidth = 32;
			b.minheight = 16;	/* GUESS ?? */
			if(copy_to_user(arg,&b,sizeof(b)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCGPICT:
		{
			/*
			 *	Default values.. if we can change this we
			 *	can add the feature later
			 */
			struct video_picture vp;
			vp.brightness = 0x8000;
			vp.hue = 0x8000;
			vp.colour = 0x8000;
			vp.whiteness = 0x8000;
			vp.depth = 8;
			/* Format is a guess */
			vp.palette = VIDEO_PALETTE_YUV420P;
			if(copy_to_user(arg, &vp, sizeof(vp)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCSPICT:
		{
			return -EINVAL;
		}
		case VIDIOCSWIN:
		{
			struct video_window vw;
			if(copy_from_user(&vw, arg, sizeof(vw)))
				return -EFAULT;
			if(vw.x <0 || vw.y <0 || vw.width < 32 
				|| vw.height < 16)
				return -EINVAL;
			memcpy(&mga_win, &vw, sizeof(mga_win));

			mga_config.x_org = vw.x;
			mga_config.y_org = vw.y;
			mga_config.dest_width = vw.width;
			mga_config.dest_height = vw.height;

			/* 
			 * May have to add 
			 *
			 * #define VIDEO_WINDOW_CHROMAKEY 16 
			 *
			 * to <linux/videodev.h> 
			 */

			//add it here for now
			#define VIDEO_WINDOW_CHROMAKEY 16 

			if (vw.flags & VIDEO_WINDOW_CHROMAKEY)
				mga_config.colkey_on = 1;
			else 
				mga_config.colkey_on = 0;

			mga_config.colkey_red   = (vw.chromakey >> 24) & 0xFF;
			mga_config.colkey_green = (vw.chromakey >> 16) & 0xFF;
			mga_config.colkey_blue  = (vw.chromakey >> 8)  & 0xFF;
			mga_vid_set_config(&mga_config);
			return 0;
				
		}
		case VIDIOCGWIN:
		{
			if(copy_to_user(arg, &mga_win, sizeof(mga_win)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCCAPTURE:
		{
			int v;
			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			vid_overlay_on = v;
			if(vid_overlay_on && vid_src_ready)
			{
				//regs.besctl |= 1;
				mga_vid_write_regs();
			}
			else
			{
				//regs.besctl &= ~1;
				mga_vid_write_regs();
			}
			return 0;
		}
		default:
			return -ENOIOCTLCMD;
	}
}

static struct video_device mga_v4l_dev =
{
	"Matrox G200/G400",
	VID_TYPE_CAPTURE,
	VID_HARDWARE_BT848,		/* This is a lie for now */
	mga_v4l_open,
	mga_v4l_close,
	mga_v4l_read,
	mga_v4l_write,
	NULL,
	mga_v4l_ioctl,
	NULL,
	mga_v4l_init_done,
	NULL,
	0,
	0
};

#endif

/* 
 * Main Initialization Function 
 */


static int mga_vid_initialize(void)
{
	mga_vid_in_use = 0;

	printk(KERN_DEBUG "SiS 6326 YUV Video interface v0.01 (c) Aaron Holtzman \n");
	if(register_chrdev(MGA_VID_MAJOR, "mga_vid", &mga_vid_fops))
	{
		printk(KERN_ERR "sis_vid: unable to get major: %d\n", MGA_VID_MAJOR);
		return -EIO;
	}

	if (!mga_vid_find_card())
	{
		printk(KERN_ERR "sis_vid: no supported devices found\n");
		unregister_chrdev(MGA_VID_MAJOR, "mga_vid");
		return -EINVAL;
	}
	
#if 0
	if (video_register_device(&mga_v4l_dev, VFL_TYPE_GRABBER)<0)
	{
		printk("sis_vid: unable to register.\n");
		unregister_chrdev(MGA_VID_MAJOR, "mga_vid");
		if(mga_mmio_base)
			iounmap(mga_mmio_base);
		mga_mmio_base = 0;
		return -EINVAL;
	}
#endif

	return(0);
}

int init_module(void)
{
	return mga_vid_initialize();
}

void cleanup_module(void)
{
//	video_unregister_device(&mga_v4l_dev);
	if(mga_mmio_base)
		iounmap(mga_mmio_base);

	//FIXME turn off BES
	printk(KERN_DEBUG "mga_vid: Cleaning up module\n");
	unregister_chrdev(MGA_VID_MAJOR, "mga_vid");
}

