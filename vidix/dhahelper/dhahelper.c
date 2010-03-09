/*
 * Direct Hardware Access (DHA) kernel helper
 *
 * Copyright (C) 2002 Alex Beregszaszi <alex@fsn.hu>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
    Accessing hardware from userspace as USER (no root needed!)

    Tested on 2.2.x (2.2.19) and 2.4.x (2.4.3,2.4.17).

    WARNING! THIS MODULE VIOLATES SEVERAL SECURITY LINES! DON'T USE IT
    ON PRODUCTION SYSTEMS, ONLY AT HOME, ON A "SINGLE-USER" SYSTEM.
    NO WARRANTY!

    Tech:
	Communication between userspace and kernelspace goes over character
	device using ioctl.

    Usage:
	mknod -m 666 /dev/dhahelper c 180 0

	Also you can change the major number, setting the "dhahelper_major"
	module parameter, the default is 180, specified in dhahelper.h.

	Note: do not use other than minor==0, the module forbids it.

    TODO:
	* do memory mapping without fops:mmap
	* implement unmap memory
	* select (request?) a "valid" major number (from Linux project? ;)
	* make security
	* is pci handling needed? (libdha does this with lowlevel port funcs)
	* is mttr handling needed?
	* test on older kernels (2.0.x (?))
*/

#ifndef MODULE
#define MODULE
#endif

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/config.h>

#ifdef CONFIG_MODVERSION
#define MODVERSION
#include <linux/modversions.h>
#endif

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
#include <linux/ioport.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>

#include <linux/mman.h>

#include <linux/fs.h>
#include <linux/unistd.h>

#include "dhahelper.h"

MODULE_AUTHOR("Alex Beregszaszi <alex@fsn.hu>");
MODULE_DESCRIPTION("Provides userspace access to hardware (security violation!)");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

static int dhahelper_major = DEFAULT_MAJOR;
MODULE_PARM(dhahelper_major, "i");
MODULE_PARM_DESC(dhahelper_major, "Major number of dhahelper characterdevice");

/* 0 = silent */
/* 1 = report errors (default) */
/* 2 = debug */
static int dhahelper_verbosity = 1;
MODULE_PARM(dhahelper_verbosity, "i");
MODULE_PARM_DESC(dhahelper_verbosity, "Level of verbosity (0 = silent, 1 = only errors, 2 = debug)");

static dhahelper_memory_t last_mem_request;


static int dhahelper_open(struct inode *inode, struct file *file)
{
    if (dhahelper_verbosity > 1)
	printk(KERN_DEBUG "dhahelper: device opened\n");

    if (MINOR(inode->i_rdev) != 0)
	return -ENXIO;

    MOD_INC_USE_COUNT;

    return 0;
}

static int dhahelper_release(struct inode *inode, struct file *file)
{
    if (dhahelper_verbosity > 1)
	printk(KERN_DEBUG "dhahelper: device released\n");

    if (MINOR(inode->i_rdev) != 0)
	return -ENXIO;

    MOD_DEC_USE_COUNT;

    return 0;
}

static int dhahelper_ioctl(struct inode *inode, struct file *file,
    unsigned int cmd, unsigned long arg)
{
    if (dhahelper_verbosity > 1)
	printk(KERN_DEBUG "dhahelper: ioctl(cmd=%x, arg=%lx)\n",
	    cmd, arg);

    if (MINOR(inode->i_rdev) != 0)
	return -ENXIO;

    switch(cmd)
    {
	case DHAHELPER_GET_VERSION:
	{
	    int version = API_VERSION;

	    if (copy_to_user((int *)arg, &version, sizeof(int)))
	    {
		if (dhahelper_verbosity > 0)
		    printk(KERN_ERR "dhahelper: failed copy to userspace\n");
		return -EFAULT;
	    }

	    break;
	}
	case DHAHELPER_PORT:
	{
	    dhahelper_port_t port;

	    if (copy_from_user(&port, (dhahelper_port_t *)arg, sizeof(dhahelper_port_t)))
	    {
		if (dhahelper_verbosity > 0)
		    printk(KERN_ERR "dhahelper: failed copy from userspace\n");
		return -EFAULT;
	    }

	    switch(port.operation)
	    {
		case PORT_OP_READ:
		{
		    switch(port.size)
		    {
			case 1:
			    port.value = inb(port.addr);
			    break;
			case 2:
			    port.value = inw(port.addr);
			    break;
			case 4:
			    port.value = inl(port.addr);
			    break;
			default:
			    if (dhahelper_verbosity > 0)
				printk(KERN_ERR "dhahelper: invalid port read size (%d)\n",
				    port.size);
			    return -EINVAL;
		    }
		    break;
		}
		case PORT_OP_WRITE:
		{
		    switch(port.size)
		    {
			case 1:
			    outb(port.value, port.addr);
			    break;
			case 2:
			    outw(port.value, port.addr);
			    break;
			case 4:
			    outl(port.value, port.addr);
			    break;
			default:
			    if (dhahelper_verbosity > 0)
				printk(KERN_ERR "dhahelper: invalid port write size (%d)\n",
				    port.size);
			    return -EINVAL;
		    }
		    break;
		}
		default:
		    if (dhahelper_verbosity > 0)
		        printk(KERN_ERR "dhahelper: invalid port operation (%d)\n",
		    	    port.operation);
		    return -EINVAL;
	    }

	    /* copy back only if read was performed */
	    if (port.operation == PORT_OP_READ)
	    if (copy_to_user((dhahelper_port_t *)arg, &port, sizeof(dhahelper_port_t)))
	    {
		if (dhahelper_verbosity > 0)
		    printk(KERN_ERR "dhahelper: failed copy to userspace\n");
		return -EFAULT;
	    }

	    break;
	}
	case DHAHELPER_MEMORY:
	{
	    dhahelper_memory_t mem;

	    if (copy_from_user(&mem, (dhahelper_memory_t *)arg, sizeof(dhahelper_memory_t)))
	    {
		if (dhahelper_verbosity > 0)
		    printk(KERN_ERR "dhahelper: failed copy from userspace\n");
		return -EFAULT;
	    }

	    switch(mem.operation)
	    {
		case MEMORY_OP_MAP:
		{
#if 1
		    memcpy(&last_mem_request, &mem, sizeof(dhahelper_memory_t));
#else
		    mem.ret = do_mmap(file, mem.start, mem.size, PROT_READ|PROT_WRITE,
			MAP_SHARED, mem.offset);
#endif

		    break;
		}
		case MEMORY_OP_UNMAP:
		    break;
		default:
		    if (dhahelper_verbosity > 0)
			printk(KERN_ERR "dhahelper: invalid memory operation (%d)\n",
			    mem.operation);
		    return -EINVAL;
	    }

	    if (copy_to_user((dhahelper_memory_t *)arg, &mem, sizeof(dhahelper_memory_t)))
	    {
		if (dhahelper_verbosity > 0)
		    printk(KERN_ERR "dhahelper: failed copy to userspace\n");
		return -EFAULT;
	    }

	    break;
	}
	default:
    	    if (dhahelper_verbosity > 0)
		printk(KERN_ERR "dhahelper: invalid ioctl (%x)\n", cmd);
	    return -EINVAL;
    }

    return 0;
}

static int dhahelper_mmap(struct file *file, struct vm_area_struct *vma)
{
    if (last_mem_request.operation != MEMORY_OP_MAP)
    {
	if (dhahelper_verbosity > 0)
	    printk(KERN_ERR "dhahelper: mapping not requested before mmap\n");
	return -EFAULT;
    }

    if (dhahelper_verbosity > 1)
	printk(KERN_INFO "dhahelper: mapping %x (size: %x)\n",
	    last_mem_request.start+last_mem_request.offset, last_mem_request.size);

    if (remap_page_range(0, last_mem_request.start + last_mem_request.offset,
	last_mem_request.size, vma->vm_page_prot))
    {
	if (dhahelper_verbosity > 0)
	    printk(KERN_ERR "dhahelper: error mapping memory\n");
	return -EFAULT;
    }

    return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
static struct file_operations dhahelper_fops =
{
    /*llseek*/	NULL,
    /*read*/	NULL,
    /*write*/	NULL,
    /*readdir*/	NULL,
    /*poll*/	NULL,
    /*ioctl*/	dhahelper_ioctl,
    /*mmap*/	dhahelper_mmap,
    /*open*/	dhahelper_open,
    /*flush*/	NULL,
    /*release*/	dhahelper_release,
    /* zero out the last 5 entries too ? */
};
#else
static struct file_operations dhahelper_fops =
{
    owner:	THIS_MODULE,
    ioctl:	dhahelper_ioctl,
    mmap:	dhahelper_mmap,
    open:	dhahelper_open,
    release:	dhahelper_release
};
#endif

#if KERNEL_VERSION < KERNEL_VERSION(2,4,0)
int init_module(void)
#else
static int __init init_dhahelper(void)
#endif
{
    printk(KERN_INFO "Direct Hardware Access kernel helper (C) Alex Beregszaszi\n");

    if(register_chrdev(dhahelper_major, "dhahelper", &dhahelper_fops))
    {
    	if (dhahelper_verbosity > 0)
	    printk(KERN_ERR "dhahelper: unable to register character device (major: %d)\n",
		dhahelper_major);
	return -EIO;
    }

    return 0;
}

#if KERNEL_VERSION < KERNEL_VERSION(2,4,0)
void cleanup_module(void)
#else
static void __exit exit_dhahelper(void)
#endif
{
    unregister_chrdev(dhahelper_major, "dhahelper");
}

EXPORT_NO_SYMBOLS;

#if KERNEL_VERSION >= KERNEL_VERSION(2,4,0)
module_init(init_dhahelper);
module_exit(exit_dhahelper);
#endif
