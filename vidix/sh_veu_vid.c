/*
 * VIDIX driver for SuperH Mobile VEU hardware block.
 * Copyright (C) 2008, 2009 Magnus Damm
 *
 * Requires a kernel that exposes the VEU hardware block to user space
 * using UIO. Available in upstream linux-2.6.27 or later.
 *
 * Tested using WVGA and QVGA panels with sh7722 VEU and sh7723 VEU2H.
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
 * You should have received a copy of the GNU General Public License
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "vidix.h"
#include "fourcc.h"

#include "dha.h"

static int fgets_with_openclose(char *fname, char *buf, size_t maxlen)
{
    FILE *fp;

    fp = fopen(fname, "r");
    if (!fp)
        return -1;

    fgets(buf, maxlen, fp);
    fclose(fp);
    return strlen(buf);
}

struct uio_device {
    char *name;
    char *path;
    int fd;
};

#define MAXNAMELEN 256

static int locate_uio_device(char *name, struct uio_device *udp)
{
    char fname[MAXNAMELEN], buf[MAXNAMELEN];
    char *tmp;
    int uio_id;

    uio_id = -1;
    do {
        uio_id++;
        snprintf(fname, MAXNAMELEN, "/sys/class/uio/uio%d/name", uio_id);
        if (fgets_with_openclose(fname, buf, MAXNAMELEN) < 0)
            return -1;

    } while (strncmp(name, buf, strlen(name)));

    tmp = strchr(buf, '\n');
    if (tmp)
        *tmp = '\0';

    udp->name = strdup(buf);
    udp->path = strdup(fname);
    udp->path[strlen(udp->path) - 5] = '\0';

    snprintf(buf, MAXNAMELEN, "/dev/uio%d", uio_id);
    udp->fd = open(buf, O_RDWR | O_SYNC);

    if (udp->fd < 0)
        return -1;

    return 0;
}

struct uio_map {
    unsigned long address;
    unsigned long size;
    void *iomem;
};

static int setup_uio_map(struct uio_device *udp, int nr, struct uio_map *ump)
{
    char fname[MAXNAMELEN], buf[MAXNAMELEN];

    snprintf(fname, MAXNAMELEN, "%s/maps/map%d/addr", udp->path, nr);
    if (fgets_with_openclose(fname, buf, MAXNAMELEN) <= 0)
        return -1;

    ump->address = strtoul(buf, NULL, 0);

    snprintf(fname, MAXNAMELEN, "%s/maps/map%d/size", udp->path, nr);
    if (fgets_with_openclose(fname, buf, MAXNAMELEN) <= 0)
        return -1;

    ump->size = strtoul(buf, NULL, 0);
    ump->iomem = mmap(0, ump->size,
                      PROT_READ|PROT_WRITE, MAP_SHARED,
                      udp->fd, nr * getpagesize());

    if (ump->iomem == MAP_FAILED)
        return -1;

    return 0;
}

struct fb_info {
    unsigned long width;
    unsigned long height;
    unsigned long bpp;
    unsigned long line_length;

    unsigned long address;
    unsigned long size;
};

static int get_fb_info(char *device, struct fb_info *fip)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    void *iomem;
    int fd;

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("ioctl(FBIOGET_VSCREENINFO)");
        return -1;
    }

    fip->width = vinfo.xres;
    fip->height = vinfo.yres;
    fip->bpp = vinfo.bits_per_pixel;

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("ioctl(FBIOGET_FSCREENINFO)");
        return -1;
    }

    fip->address = finfo.smem_start;
    fip->size = finfo.smem_len;
    fip->line_length = finfo.line_length;

    iomem = mmap(0, fip->size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (iomem == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    /* clear framebuffer */
    memset(iomem, 0, fip->line_length * fip->height);
    munmap(iomem, fip->size);

    return fd;
}

#define VESTR 0x00 /* start register */
#define VESWR 0x10 /* src: line length */
#define VESSR 0x14 /* src: image size */
#define VSAYR 0x18 /* src: y/rgb plane address */
#define VSACR 0x1c /* src: c plane address */
#define VBSSR 0x20 /* bundle mode register */
#define VEDWR 0x30 /* dst: line length */
#define VDAYR 0x34 /* dst: y/rgb plane address */
#define VDACR 0x38 /* dst: c plane address */
#define VTRCR 0x50 /* transform control */
#define VRFCR 0x54 /* resize scale */
#define VRFSR 0x58 /* resize clip */
#define VENHR 0x5c /* enhance */
#define VFMCR 0x70 /* filter mode */
#define VVTCR 0x74 /* lowpass vertical */
#define VHTCR 0x78 /* lowpass horizontal */
#define VAPCR 0x80 /* color match */
#define VECCR 0x84 /* color replace */
#define VAFXR 0x90 /* fixed mode */
#define VSWPR 0x94 /* swap */
#define VEIER 0xa0 /* interrupt mask */
#define VEVTR 0xa4 /* interrupt event */
#define VSTAR 0xb0 /* status */
#define VBSRR 0xb4 /* reset */

#define VMCR00 0x200 /* color conversion matrix coefficient 00 */
#define VMCR01 0x204 /* color conversion matrix coefficient 01 */
#define VMCR02 0x208 /* color conversion matrix coefficient 02 */
#define VMCR10 0x20c /* color conversion matrix coefficient 10 */
#define VMCR11 0x210 /* color conversion matrix coefficient 11 */
#define VMCR12 0x214 /* color conversion matrix coefficient 12 */
#define VMCR20 0x218 /* color conversion matrix coefficient 20 */
#define VMCR21 0x21c /* color conversion matrix coefficient 21 */
#define VMCR22 0x220 /* color conversion matrix coefficient 22 */
#define VCOFFR 0x224 /* color conversion offset */
#define VCBR   0x228 /* color conversion clip */

#define VRPBR  0xc8 /* resize passband */

/* Helper functions for reading registers. */

static unsigned long read_reg(struct uio_map *ump, int reg_offs)
{
    volatile unsigned long *reg = ump->iomem;

    return reg[reg_offs / 4];
}

static void write_reg(struct uio_map *ump, unsigned long value, int reg_offs)
{
    volatile unsigned long *reg = ump->iomem;

    reg[reg_offs / 4] = value;
}

static vidix_capability_t sh_veu_cap = {
    "SuperH VEU driver",
    "Magnus Damm",
    TYPE_OUTPUT,
    { 0, 0, 0, 0 },
    2560,
    1920,
    16,
    16,
    -1,
    FLAG_UPSCALER|FLAG_DOWNSCALER,
    -1,
    -1,
    { 0, 0, 0, 0 }
};

/* global variables yuck */

static struct fb_info fbi;
static struct uio_device uio_dev;
static struct uio_map uio_mmio, uio_mem;

struct sh_veu_plane {
    unsigned long width;
    unsigned long height;
    unsigned long stride;
    unsigned long pos_x;
    unsigned long pos_y;
};

static struct sh_veu_plane _src, _dst;
static vidix_playback_t my_info;
static int fb_fd;

static int sh_veu_probe(int verbose, int force)
{
    int ret;

    ret = get_fb_info("/dev/fb0", &fbi);
    if (ret < 0)
        return ret;
    fb_fd = ret;

    if (fbi.bpp != 16) {
        printf("sh_veu: only 16bpp supported\n");
        return -1;
    }

    ret = locate_uio_device("VEU", &uio_dev);
    if (ret < 0) {
        printf("sh_veu: unable to locate matching UIO device\n");
        return ret;
    }

    ret = setup_uio_map(&uio_dev, 0, &uio_mmio);
    if (ret < 0) {
        printf("sh_veu: cannot setup MMIO\n");
        return ret;
    }

    ret = setup_uio_map(&uio_dev, 1, &uio_mem);
    if (ret < 0) {
        printf("sh_veu: cannot setup contiguous memory\n");
        return ret;
    }

    printf("sh_veu: Using %s at %s on %lux%lu %ldbpp /dev/fb0\n",
           uio_dev.name, uio_dev.path,
           fbi.width, fbi.height, fbi.bpp);

    return ret;
}

static void sh_veu_wait_irq(vidix_playback_t *info)
{
    unsigned long n_pending;

    /* Wait for an interrupt */
    read(uio_dev.fd, &n_pending, sizeof(unsigned long));

    write_reg(&uio_mmio, 0x100, VEVTR); /* ack int, write 0 to bit 0 */

    /* flush framebuffer to handle deferred io case */
    fsync(fb_fd);
}

static int sh_veu_is_veu2h(void)
{
    return uio_mmio.size == 0x27c;
}

static int sh_veu_is_veu3f(void)
{
    return uio_mmio.size == 0xcc;
}

static unsigned long sh_veu_do_scale(struct uio_map *ump,
                                     int vertical, int size_in,
                                     int size_out, int crop_out)
{
    unsigned long fixpoint, mant, frac, value, rep, vb;

    /* calculate FRAC and MANT */
    do {
        rep = mant = frac = 0;

        if (size_in == size_out) {
            if (crop_out != size_out)
                mant = 1; /* needed for cropping */
            break;
        }

        /* VEU2H special upscale */
        if (sh_veu_is_veu2h() && size_out > size_in) {
            fixpoint = (4096 * size_in) / size_out;

            mant = fixpoint / 4096;
            frac = fixpoint - (mant * 4096);
            frac &= ~0x07;

            switch (frac) {
            case 0x800:
                rep = 1;
                break;
            case 0x400:
                rep = 3;
                break;
            case 0x200:
                rep = 7;
                break;
            }

            if (rep)
                break;
        }

        fixpoint = (4096 * (size_in - 1)) / (size_out + 1);
        mant = fixpoint / 4096;
        frac = fixpoint - (mant * 4096);

        if (frac & 0x07) {
            frac &= ~0x07;
            if (size_out > size_in)
                frac -= 8; /* round down if scaling up */
            else
                frac += 8; /* round up if scaling down */
        }

    } while (0);

    /* set scale */
    value = read_reg(ump, VRFCR);
    if (vertical) {
        value &= ~0xffff0000;
        value |= ((mant << 12) | frac) << 16;
    } else {
        value &= ~0xffff;
        value |= (mant << 12) | frac;
    }
    write_reg(ump, value, VRFCR);

    /* set clip */
    value = read_reg(ump, VRFSR);
    if (vertical) {
        value &= ~0xffff0000;
        value |= ((rep << 12) | crop_out) << 16;
    } else {
        value &= ~0xffff;
        value |= (rep << 12) | crop_out;
    }
    write_reg(ump, value, VRFSR);

    /* VEU3F needs additional VRPBR register handling */
    if (sh_veu_is_veu3f()) {
        if (size_out > size_in)
            vb = 64;
        else {
            if ((mant >= 8) && (mant < 16))
                value = 4;
            else if ((mant >= 4) && (mant < 8))
                value = 2;
            else
                value = 1;

            vb = 64 * 4096 * value;
            vb /= 4096 * mant + frac;
        }

        /* set resize passband register */
        value = read_reg(ump, VRPBR);
        if (vertical) {
            value &= ~0xffff0000;
            value |= vb << 16;
        } else {
            value &= ~0xffff;
            value |= vb;
        }
        write_reg(ump, value, VRPBR);
    }

    return (((size_in * crop_out) / size_out) + 0x03) & ~0x03;
}

static void sh_veu_setup_planes(vidix_playback_t *info,
                                struct sh_veu_plane *src,
                                struct sh_veu_plane *dst)
{
    unsigned long addr, real_w, real_h;

    src->width = info->src.w;
    src->height = info->src.h;
    src->stride = (info->src.w+15) & ~15;

    dst->width = real_w = info->dest.w;
    dst->height = real_h = info->dest.h;
    dst->stride = fbi.line_length;
    dst->pos_x = info->dest.x & ~0x03;
    dst->pos_y = info->dest.y;

    if ((dst->width + dst->pos_x) > fbi.width)
        dst->width = fbi.width - dst->pos_x;

    if ((dst->height + dst->pos_y) > fbi.height)
        dst->height = fbi.height - dst->pos_y;

    addr = fbi.address;
    addr += dst->pos_x * (fbi.bpp / 8);
    addr += dst->pos_y * dst->stride;

    src->width = sh_veu_do_scale(&uio_mmio, 0, src->width,
                                 real_w, dst->width);
    src->height = sh_veu_do_scale(&uio_mmio, 1, src->height,
                                  real_h, dst->height);

    write_reg(&uio_mmio, src->stride, VESWR);
    write_reg(&uio_mmio, src->width | (src->height << 16), VESSR);
    write_reg(&uio_mmio, 0, VBSSR); /* not using bundle mode */

    write_reg(&uio_mmio, dst->stride, VEDWR);
    write_reg(&uio_mmio, addr, VDAYR);
    write_reg(&uio_mmio, 0, VDACR); /* unused for RGB */

    write_reg(&uio_mmio, 0x67, VSWPR);
    write_reg(&uio_mmio, (6 << 16) | (0 << 14) | 2 | 4, VTRCR);

    if (sh_veu_is_veu2h()) {
        write_reg(&uio_mmio, 0x0cc5, VMCR00);
        write_reg(&uio_mmio, 0x0950, VMCR01);
        write_reg(&uio_mmio, 0x0000, VMCR02);

        write_reg(&uio_mmio, 0x397f, VMCR10);
        write_reg(&uio_mmio, 0x0950, VMCR11);
        write_reg(&uio_mmio, 0x3ccd, VMCR12);

        write_reg(&uio_mmio, 0x0000, VMCR20);
        write_reg(&uio_mmio, 0x0950, VMCR21);
        write_reg(&uio_mmio, 0x1023, VMCR22);

        write_reg(&uio_mmio, 0x00800010, VCOFFR);
    }

    write_reg(&uio_mmio, 1, VEIER); /* enable interrupt in VEU */
}

static void sh_veu_blit(vidix_playback_t *info, int frame)
{
    unsigned long enable = 1;
    unsigned long addr;

    addr = uio_mem.address + info->offsets[frame];

    write_reg(&uio_mmio, addr + info->offset.y, VSAYR);
    write_reg(&uio_mmio, addr + info->offset.u, VSACR);

    /* Enable interrupt in UIO driver */
    write(uio_dev.fd, &enable, sizeof(unsigned long));

    write_reg(&uio_mmio, 1, VESTR); /* start operation */
}

static int sh_veu_init(void)
{
    write_reg(&uio_mmio, 0x100, VBSRR); /* reset VEU */
    return 0;
}

static void sh_veu_destroy(void)
{
    close(fb_fd);
}

static int sh_veu_get_caps(vidix_capability_t *to)
{
    memcpy(to, &sh_veu_cap, sizeof(vidix_capability_t));
    return 0;
}

static int sh_veu_query_fourcc(vidix_fourcc_t *to)
{
    if (to->fourcc == IMGFMT_NV12) {
        to->depth = VID_DEPTH_ALL;
        to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK;
        return 0;
    }
    to->depth = to->flags = 0;
    return ENOSYS;
}


static int sh_veu_config_playback(vidix_playback_t *info)
{
    unsigned int i, y_pitch;

    switch (info->fourcc) {
    case IMGFMT_NV12:
        y_pitch = (info->src.w + 15) & ~15;

        info->offset.y = 0;
        info->offset.u = y_pitch * info->src.h;
        info->frame_size = info->offset.u + info->offset.u / 2;
        break;
    default:
        return ENOTSUP;
    }

    info->num_frames = uio_mem.size / info->frame_size;
    if (info->num_frames > VID_PLAY_MAXFRAMES)
        info->num_frames = VID_PLAY_MAXFRAMES;

    if (!info->num_frames) {
        printf("sh_veu: %d is not enough memory for %d bytes frame\n",
               (int)uio_mem.size, (int)info->frame_size);
        return ENOMEM;
    }

    info->dga_addr = uio_mem.iomem;
    info->dest.pitch.y = info->dest.pitch.u = info->dest.pitch.v = 16;

    for (i = 0; i < info->num_frames; i++)
        info->offsets[i] = info->frame_size * i;

    my_info = *info;

    printf("sh_veu: %d frames * %d bytes, total size = %d\n",
           (int)info->num_frames, (int)info->frame_size,
           (int)uio_mem.size);

    sh_veu_setup_planes(info, &_src, &_dst);

    printf("sh_veu: %dx%d->%dx%d@%dx%d -> %dx%d->%dx%d@%dx%d \n",
           (int)info->src.w, (int)info->src.h,
           (int)info->dest.w, (int)info->dest.h,
           (int)info->dest.x, (int)info->dest.y,
           (int)_src.width, (int)_src.height,
           (int)_dst.width, (int)_dst.height,
           (int)_dst.pos_x, (int)_dst.pos_y);
    return 0;
}


static int sh_veu_playback_on(void)
{
    return 0;
}

static int sh_veu_playback_off(void)
{
    return 0;
}

static int sh_veu_first_frame = 1;

static int sh_veu_frame_sel(unsigned int frame)
{
    if (!sh_veu_first_frame)
        sh_veu_wait_irq(&my_info);

    sh_veu_blit(&my_info, frame);
    sh_veu_first_frame = 0;
    return 0;
}

VDXDriver sh_veu_drv = {
    "sh_veu",
    NULL,
    .probe = sh_veu_probe,
    .get_caps = sh_veu_get_caps,
    .query_fourcc = sh_veu_query_fourcc,
    .init = sh_veu_init,
    .destroy = sh_veu_destroy,
    .config_playback = sh_veu_config_playback,
    .playback_on = sh_veu_playback_on,
    .playback_off = sh_veu_playback_off,
    .frame_sel = sh_veu_frame_sel,
};
