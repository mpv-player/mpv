/*
 * Copyright (C) 1999 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mga_vid.
 *
 * mga_vid is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mga_vid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mga_vid; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

//#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <string.h>
#include "mga_vid.h"

mga_vid_config_t config;
uint8_t *mga_vid_base;
uint32_t is_g400;

#define SRC_IMAGE_WIDTH 256
#define SRC_IMAGE_HEIGHT 256

uint8_t y_image[SRC_IMAGE_WIDTH * SRC_IMAGE_HEIGHT];
uint8_t cr_image[SRC_IMAGE_WIDTH * SRC_IMAGE_HEIGHT];
uint8_t cb_image[SRC_IMAGE_WIDTH * SRC_IMAGE_HEIGHT];


void
write_frame_g200(uint8_t *y,uint8_t *cr, uint8_t *cb)
{
    uint8_t *dest;
    uint32_t bespitch,h,w;

    dest = mga_vid_base;
    bespitch = (config.src_width + 31) & ~31;

    for(h=0; h < config.src_height; h++)
    {
        memcpy(dest, y, config.src_width);
        y += config.src_width;
        dest += bespitch;
    }

    for(h=0; h < config.src_height/2; h++)
    {
        for(w=0; w < config.src_width/2; w++)
        {
            *dest++ = *cb++;
            *dest++ = *cr++;
        }
        dest += bespitch - config.src_width;
    }
}

void
write_frame_g400(uint8_t *y,uint8_t *cr, uint8_t *cb)
{
    uint8_t *dest;
    uint32_t bespitch,h;

    dest = mga_vid_base;
    bespitch = (config.src_width + 31) & ~31;

    for(h=0; h < config.src_height; h++)
    {
        memcpy(dest, y, config.src_width);
        y += config.src_width;
        dest += bespitch;
    }

    for(h=0; h < config.src_height/2; h++)
    {
        memcpy(dest, cb, config.src_width/2);
        cb += config.src_width/2;
        dest += bespitch/2;
    }

    for(h=0; h < config.src_height/2; h++)
    {
        memcpy(dest, cr, config.src_width/2);
        cr += config.src_width/2;
        dest += bespitch/2;
    }
}

void write_frame(uint8_t *y,uint8_t *cr, uint8_t *cb)
{
    if(is_g400)
        write_frame_g400(y,cr,cb);
    else
        write_frame_g200(y,cr,cb);
}

void
draw_cool_pattern(void)
{
    int i,x,y;

    i = 0;
    for (y=0; y<config.src_height; y++) {
        for (x=0; x<config.src_width; x++) {
            y_image[i++] = x*x/2 + y*y/2 - 128;
        }
    }

    i = 0;
    for (y=0; y<config.src_height/2; y++)
        for (x=0; x<config.src_width/2; x++)
        {
                cr_image[i++] = x - 128;
        }

    i = 0;
    for (y=0; y<config.src_height/2; y++)
        for (x=0; x<config.src_width/2; x++)
        {
                cb_image[i++] = y - 128;
        }
}

void
draw_color_blend(void)
{
    int i,x,y;

    i = 0;
    for (y=0; y<config.src_height; y++) {
        for (x=0; x<config.src_width; x++) {
            y_image[i++] = 0;
        }
    }

    i = 0;
    for (y=0; y<config.src_height/2; y++)
        for (x=0; x<config.src_width/2; x++)
        {
                cr_image[i++] = x - 128;
        }

    i = 0;
    for (y=0; y<config.src_height/2; y++)
        for (x=0; x<config.src_width/2; x++)
        {
                cb_image[i++] = y - 128;
        }
}


int
main(void)
{
    int f;

    f = open("/dev/mga_vid",O_RDWR);

    if(f == -1)
    {
        fprintf(stderr,"Couldn't open driver\n");
        exit(1);
    }

    config.version = MGA_VID_VERSION;
    config.src_width = SRC_IMAGE_WIDTH;
    config.src_height= SRC_IMAGE_HEIGHT;
    config.dest_width = SRC_IMAGE_WIDTH;
    config.dest_height = SRC_IMAGE_HEIGHT;
    config.x_org= 10;
    config.y_org= 10;
    config.colkey_on = 0;
    config.format = MGA_VID_FORMAT_YV12;
    config.frame_size=SRC_IMAGE_WIDTH*SRC_IMAGE_HEIGHT*2;
    config.num_frames=1;

    if (ioctl(f,MGA_VID_CONFIG,&config))
    {
        perror("Error in config ioctl");
    }

    if (config.card_type == MGA_G200)
    {
        printf("Testing MGA G200 Backend Scaler with %d MB of RAM\n", config.ram_size);
      is_g400 = 0;
    }
    else
    {
        printf("Testing MGA G400 Backend Scaler with %d MB of RAM\n", config.ram_size);
      is_g400 = 1;
    }

    ioctl(f,MGA_VID_ON,0);
    mga_vid_base = (uint8_t*)mmap(0,256 * 4096,PROT_WRITE,MAP_SHARED,f,0);
    printf("mga_vid_base = %8p\n",mga_vid_base);


    //memset(y_image,80,256 * 128);
    //memset(cr_image,80,256/2 * 20);
    //memset(cb_image,80,256/2 * 20);
    write_frame(y_image,cr_image,cb_image);
    printf("(1) There should be a green square, offset by 10 pixels from\n"
               "    the upper left corner displayed\n");
    sleep(3);


    draw_cool_pattern();
    write_frame(y_image,cr_image,cb_image);
    printf("(2) There should be a cool mosaic like pattern now.\n");
    sleep(3);

    draw_color_blend();
    write_frame(y_image,cr_image,cb_image);
    printf("(3) There should be a color blend with black, red, purple, blue\n"
               "    corners (starting top left going CW)\n");
    sleep(3);

    ioctl(f,MGA_VID_OFF,0);

    close(f);
    return 0;
}
