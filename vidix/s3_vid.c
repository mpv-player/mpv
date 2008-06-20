/*
 * VIDIX driver for S3 chipsets.
 *
 * Copyright (C) 2004 Reza Jelveh
 * Thanks to Alex Deucher for Support
 * Trio/Virge support by Michael Kostylev
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>

#include "config.h"
#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"

#include "s3_regs.h"

static void S3SetColorKeyOld (void);
static void S3SetColorKeyNew (void);
static void S3SetColorKey2000 (void); 
static void (*S3SetColorKey) (void) = NULL;

static void S3SetColorOld (void);
static void S3SetColorNew (void);
static void S3SetColor2000 (void); 
static void (*S3SetColor) (void) = NULL;

static void S3DisplayVideoOld (void);
static void S3DisplayVideoNew (void);
static void S3DisplayVideo2000 (void);
static void (*S3DisplayVideo) (void) = NULL;

static void S3InitStreamsOld (void);
static void S3InitStreamsNew (void);
static void S3InitStreams2000 (void);
static void (*S3InitStreams) (void) = NULL;

pciinfo_t pci_info;

struct s3_chip
{
  int arch;
  unsigned long fbsize;
  void (*lock) (struct s3_chip *, int);
};
typedef struct s3_chip s3_chip;

struct s3_info
{
  vidix_video_eq_t eq;
  unsigned int use_colorkey;
  unsigned int colorkey;
  unsigned int vidixcolorkey;
  unsigned int depth;
  unsigned int bpp;
  unsigned int format;
  unsigned int pitch;
  unsigned int blendBase;
  unsigned int displayWidth, displayHeight;
  unsigned int src_w, src_h;
  unsigned int drw_w, drw_h;
  unsigned int wx, wy;
  unsigned int screen_x;
  unsigned int screen_y;
  unsigned long frame_size;
  struct s3_chip chip;
  void *video_base;
  void *control_base;
  unsigned long picture_base;
  unsigned long picture_offset;
  unsigned int num_frames;
  int bps;
};
typedef struct s3_info s3_info;

static s3_info *info;

static vidix_capability_t s3_cap = {
  "S3 BES",
  "Reza Jelveh, Michael Kostylev",
  TYPE_OUTPUT,
  {0, 0, 0, 0},
  4096,
  4096,
  4,
  4,
  -1,
  FLAG_UPSCALER | FLAG_DOWNSCALER,
  VENDOR_S3_INC,
  -1,
  {0, 0, 0, 0}
};

struct s3_cards
{
  unsigned short chip_id;
  unsigned short arch;
};

static struct s3_cards s3_card_ids[] = {
  /* Trio64V */
  {DEVICE_S3_INC_86C764_765_TRIO32_64_64V, S3_TRIO64V},
  {DEVICE_S3_INC_86C767_TRIO_64UV, S3_TRIO64V},
  {DEVICE_S3_INC_86C755_TRIO_64V2_DX, S3_TRIO64V},
  {DEVICE_S3_INC_86C775_86C785_TRIO_64V2_DX, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY2, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY3, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY4, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY5, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY6, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY7, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY8, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY9, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY10, S3_TRIO64V},
  {DEVICE_S3_INC_TRIO_64V_FAMILY11, S3_TRIO64V},
  /* Virge */
  {DEVICE_S3_INC_86C325_VIRGE, S3_VIRGE},
  {DEVICE_S3_INC_86C988_VIRGE_VX, S3_VIRGE},
  {DEVICE_S3_INC_VIRGE_DX_OR_GX, S3_VIRGE},
  {DEVICE_S3_INC_VIRGE_GX2, S3_VIRGE},
  {DEVICE_S3_INC_VIRGE_M3, S3_VIRGE},
  {DEVICE_S3_INC_VIRGE_MX, S3_VIRGE},
  {DEVICE_S3_INC_VIRGE_MX2, S3_VIRGE},
  {DEVICE_S3_INC_VIRGE_MX_MV, S3_VIRGE},
  /* Savage3D */
  {DEVICE_S3_INC_86C794_SAVAGE_3D, S3_SAVAGE3D},
  {DEVICE_S3_INC_86C390_SAVAGE_3D_MV, S3_SAVAGE3D},
  /* Savage4 */
  {DEVICE_S3_INC_SAVAGE_4, S3_SAVAGE4},
  {DEVICE_S3_INC_SAVAGE_42, S3_SAVAGE4},
  /* SavageMX */
  {DEVICE_S3_INC_86C270_294_SAVAGE_MX_MV, S3_SAVAGE_MX},
  {DEVICE_S3_INC_82C270_294_SAVAGE_MX, S3_SAVAGE_MX},
  {DEVICE_S3_INC_86C270_294_SAVAGE_IX_MV, S3_SAVAGE_MX},
  /* SuperSavage */
  {DEVICE_S3_INC_SUPERSAVAGE_MX_128, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_MX_64, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_MX_64C, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_IX_128_SDR, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_IX_128_DDR, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_IX_64_SDR, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_IX_64_DDR, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_IX_C_SDR, S3_SUPERSAVAGE},
  {DEVICE_S3_INC_SUPERSAVAGE_IX_C_DDR, S3_SUPERSAVAGE},
  /* ProSavage */
  {DEVICE_S3_INC_PROSAVAGE_PM133, S3_PROSAVAGE},
  {DEVICE_S3_INC_PROSAVAGE_KM133, S3_PROSAVAGE},
  {DEVICE_S3_INC_86C380_PROSAVAGEDDR_K4M266, S3_PROSAVAGE},
  {DEVICE_S3_INC_VT8636A_PROSAVAGE_KN133, S3_PROSAVAGE},
  {DEVICE_S3_INC_VT8751_PROSAVAGEDDR_P4M266, S3_PROSAVAGE},
  {DEVICE_S3_INC_VT8375_PROSAVAGE8_KM266_KL266, S3_PROSAVAGE},
  /* Savage2000 */
  {DEVICE_S3_INC_86C410_SAVAGE_2000, S3_SAVAGE2000}
};

static unsigned int GetBlendForFourCC (int id)
{
  switch (id)
  {
  case IMGFMT_UYVY:
    return 0;
  case IMGFMT_YUY2:
    return 1;
  case IMGFMT_Y211:
    return 4;
  case IMGFMT_BGR15:
    return 3;
  case IMGFMT_BGR16:
    return 5;
  case IMGFMT_BGR24:
    return 6;
  case IMGFMT_BGR32:
    return 7;
  default:
    return 1;
  }
}

static void S3SetColorOld (void)
{
  char sat = (info->eq.saturation + 1000) * 15 / 2000;
  double hue = info->eq.hue * 3.1415926 / 1000.0;
  char hsx = ((char) (sat * cos (hue))) & 0x1f;
  char hsy = ((char) (sat * sin (hue))) & 0x1f;

  OUTREG (COLOR_ADJUSTMENT_REG, 0x80008000 | hsy << 24 | hsx << 16 |
    ((info->eq.contrast + 1000) * 31 / 2000) << 8 |
    (info->eq.brightness + 1000) * 255 / 2000);
}

static void S3SetColorNew (void)
{
  /* not yet */
}

static void S3SetColor2000 (void)
{
  /* not yet */
}

static void S3SetColorKeyOld (void)
{
  int red, green, blue;

  /* Here, we reset the colorkey and all the controls */

  red = (info->vidixcolorkey & 0x00FF0000) >> 16;
  green = (info->vidixcolorkey & 0x0000FF00) >> 8;
  blue = info->vidixcolorkey & 0x000000FF;

  if (!info->vidixcolorkey)
  {
    OUTREG (COL_CHROMA_KEY_CONTROL_REG, 0);
    OUTREG (CHROMA_KEY_UPPER_BOUND_REG, 0);
    OUTREG (BLEND_CONTROL_REG, 0);
  }
  else
  {
    switch (info->depth)
    {
      // FIXME: isnt fixed yet
    case 8:
      OUTREG (COL_CHROMA_KEY_CONTROL_REG, 0x37000000 | (info->vidixcolorkey & 0xFF));
      OUTREG (CHROMA_KEY_UPPER_BOUND_REG, 0x00000000 | (info->vidixcolorkey & 0xFF));
      break;
    case 15:
      /* 15 bpp 555 */
      red &= 0x1f;
      green &= 0x1f;
      blue &= 0x1f;
      OUTREG (COL_CHROMA_KEY_CONTROL_REG, 0x05000000 | (red << 19) | (green << 11) | (blue << 3));
      OUTREG (CHROMA_KEY_UPPER_BOUND_REG, 0x00000000 | (red << 19) | (green << 11) | (blue << 3));
      break;
    case 16:
      /* 16 bpp 565 */
      red &= 0x1f;
      green &= 0x3f;
      blue &= 0x1f;
      OUTREG (COL_CHROMA_KEY_CONTROL_REG, 0x16000000 | (red << 19) | (green << 10) | (blue << 3));
      OUTREG (CHROMA_KEY_UPPER_BOUND_REG, 0x00020002 | (red << 19) | (green << 10) | (blue << 3));
      break;
    case 24:
      /* 24 bpp 888 */
      OUTREG (COL_CHROMA_KEY_CONTROL_REG, 0x17000000 | (red << 16) | (green << 8) | (blue));
      OUTREG (CHROMA_KEY_UPPER_BOUND_REG, 0x00000000 | (red << 16) | (green << 8) | (blue));
      break;
    }

    /* We use destination colorkey */
    OUTREG (BLEND_CONTROL_REG, 0x05000000);
  }
}

static void S3SetColorKeyNew (void)
{
  /* not yet */
}

static void S3SetColorKey2000 (void)
{
  /* not yet */
}

static void S3DisplayVideoOld (void)
{
  unsigned int ssControl;
  int cr92;

  /* Set surface location and stride */
  OUTREG (SSTREAM_FBADDR0_REG, info->picture_offset);
  OUTREG (SSTREAM_FBADDR1_REG, 0);
  OUTREG (SSTREAM_STRIDE_REG, info->pitch);
  /* Set window parameters */
  OUTREG (SSTREAM_WINDOW_START_REG, OS_XY (info->wx, info->wy));
  OUTREG (SSTREAM_WINDOW_SIZE_REG, OS_WH (info->drw_w, info->drw_h));

  /* Set surface format and adjust scaling */
  if (info->chip.arch <= S3_VIRGE)
  {
    ssControl = ((info->src_w - 1) << 1) - ((info->drw_w - 1) & 0xffff);
    ssControl |= GetBlendForFourCC (info->format) << 24;
    if (info->src_w != info->drw_w)
      ssControl |= 2 << 28; 

    OUTREG (SSTREAM_CONTROL_REG, ssControl);
    OUTREG (SSTREAM_STRETCH_REG, (((info->src_w - info->drw_w) & 0x7ff) << 16) | (info->src_w - 1));
    /* Calculate vertical scale factor */
    OUTREG (K1_VSCALE_REG, info->src_h - 1);
    OUTREG (K2_VSCALE_REG, (info->src_h - info->drw_h) & 0x7ff);
    OUTREG (DDA_VERT_REG, (1 - info->drw_h) & 0xfff);
  }
  else
  {
    ssControl = GetBlendForFourCC (info->format) << 24 | info->src_w;
    if (info->src_w > (info->drw_w << 1))
    {
      /* BUGBUG shouldn't this be >=?  */
      if (info->src_w <= (info->drw_w << 2))
        ssControl |= HDSCALE_4;
      else if (info->src_w > (info->drw_w << 3))
        ssControl |= HDSCALE_8;
      else if (info->src_w > (info->drw_w << 4))
        ssControl |= HDSCALE_16;
      else if (info->src_w > (info->drw_w << 5))
        ssControl |= HDSCALE_32;
      else if (info->src_w > (info->drw_w << 6))
        ssControl |= HDSCALE_64;
    }

    OUTREG (SSTREAM_CONTROL_REG, ssControl);
    OUTREG (SSTREAM_STRETCH_REG, (info->src_w << 15) / info->drw_w);
    OUTREG (SSTREAM_LINES_REG, info->src_h);
    /* Calculate vertical scale factor. */
    OUTREG (SSTREAM_VSCALE_REG, VSCALING (info->src_h, info->drw_h));
  }

  if (info->chip.arch == S3_TRIO64V)
    OUTREG (STREAMS_FIFO_REG, (6 << 10) | (14 << 5) | 16);
  else
  {
    // FIXME: this should actually be enabled
    info->pitch = (info->pitch + 7) / 8;
    VGAOUT8 (vgaCRIndex, 0x92);
    cr92 = VGAIN8 (vgaCRReg);
    VGAOUT8 (vgaCRReg, (cr92 & 0x40) | (info->pitch >> 8) | 0x80);
    VGAOUT8 (vgaCRIndex, 0x93);
    VGAOUT8 (vgaCRReg, info->pitch);
    OUTREG (STREAMS_FIFO_REG, 2 | 25 << 5 | 32 << 11);
  }
}

static void S3DisplayVideoNew (void)
{
  /* not yet */
}

static void S3DisplayVideo2000 (void)
{
  /* not yet */
}

static void S3InitStreamsOld (void)
{
  /*unsigned long jDelta; */
  unsigned long format = 0;

  /*jDelta = pScrn->displayWidth * (pScrn->bitsPerPixel + 7) / 8; */
  switch (info->depth)
  {
  case 8:
    format = 0 << 24;
    break;
  case 15:
    format = 3 << 24;
    break;
  case 16:
    format = 5 << 24;
    break;
  case 24:
    format = 7 << 24;
    break;
  }
//#warning enable this again
  OUTREG (PSTREAM_FBSIZE_REG, info->screen_y * info->screen_x * (info->bpp >> 3));
  OUTREG (PSTREAM_WINDOW_START_REG, OS_XY (0, 0));
  OUTREG (PSTREAM_WINDOW_SIZE_REG, OS_WH (info->screen_x, info->screen_y));
  OUTREG (PSTREAM_FBADDR1_REG, 0);
  /*OUTREG( PSTREAM_STRIDE_REG, jDelta ); */
  OUTREG (PSTREAM_CONTROL_REG, format);
  OUTREG (PSTREAM_FBADDR0_REG, 0);

  OUTREG (COL_CHROMA_KEY_CONTROL_REG, 0);
  OUTREG (SSTREAM_CONTROL_REG, 0);
  OUTREG (CHROMA_KEY_UPPER_BOUND_REG, 0);
  OUTREG (SSTREAM_STRETCH_REG, 0);
  OUTREG (COLOR_ADJUSTMENT_REG, 0);
  OUTREG (BLEND_CONTROL_REG, 1 << 24);
  OUTREG (DOUBLE_BUFFER_REG, 0);
  OUTREG (SSTREAM_FBADDR0_REG, 0);
  OUTREG (SSTREAM_FBADDR1_REG, 0);
  OUTREG (SSTREAM_FBADDR2_REG, 0);
  OUTREG (SSTREAM_FBSIZE_REG, 0);
  OUTREG (SSTREAM_STRIDE_REG, 0);
  OUTREG (SSTREAM_VSCALE_REG, 0);
  OUTREG (SSTREAM_LINES_REG, 0);
  OUTREG (SSTREAM_VINITIAL_REG, 0);
}

static void S3InitStreamsNew (void)
{
  /* not yet */
}

static void S3InitStreams2000 (void)
{
  /* not yet */
}

static void S3StreamsOn (void)
{
  unsigned char jStreamsControl;

  VGAOUT8 (vgaCRIndex, EXT_MISC_CTRL2);

  if (S3_SAVAGE_MOBILE_SERIES (info->chip.arch))
  {
    jStreamsControl = VGAIN8 (vgaCRReg) | ENABLE_STREAM1;
    VerticalRetraceWait ();
    VGAOUT16 (vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2);

    S3InitStreams = S3InitStreamsNew;
    S3SetColor = S3SetColorNew;
    S3SetColorKey = S3SetColorKeyNew;
    S3DisplayVideo = S3DisplayVideoNew;
  }
  else if (info->chip.arch == S3_SAVAGE2000)
  {
    jStreamsControl = VGAIN8 (vgaCRReg) | ENABLE_STREAM1;
    VerticalRetraceWait ();
    VGAOUT16 (vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2);

    S3InitStreams = S3InitStreams2000;
    S3SetColor = S3SetColor2000;
    S3SetColorKey = S3SetColorKey2000;
    S3DisplayVideo = S3DisplayVideo2000;
  }
  else
  {
    jStreamsControl = VGAIN8 (vgaCRReg) | ENABLE_STREAMS_OLD;
    VerticalRetraceWait ();
    VGAOUT16 (vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2);

    S3InitStreams = S3InitStreamsOld;
    S3SetColor = S3SetColorOld;
    S3SetColorKey = S3SetColorKeyOld;
    S3DisplayVideo = S3DisplayVideoOld;
  }

  S3InitStreams ();

  VerticalRetraceWait ();
  /* Turn on secondary stream TV flicker filter, once we support TV. */
  /* SR70 |= 0x10 */
}

static void S3GetScrProp (struct s3_info *info)
{
  unsigned char bpp = 0;

  VGAOUT8 (vgaCRIndex, EXT_MISC_CTRL2);
  bpp = VGAIN8 (vgaCRReg);

  switch (bpp & 0xf0)
  {
  case 0x00:
  case 0x10:
    info->depth = 8;
    info->bpp = 8;
    break;
  case 0x20:
  case 0x30:
    info->depth = 15;
    info->bpp = 16;
    break;
  case 0x40:
  case 0x50:
    info->depth = 16;
    info->bpp = 16;
    break;
  case 0x70:
  case 0xd0:
    info->depth = 24;
    info->bpp = 32;
    break;
  }

  VGAOUT8 (vgaCRIndex, 0x1);
  info->screen_x = (1 + VGAIN8 (vgaCRReg)) << 3;
  VGAOUT8 (vgaCRIndex, 0x12);
  info->screen_y = VGAIN8 (vgaCRReg);
  VGAOUT8 (vgaCRIndex, 0x07);
  info->screen_y |= (VGAIN8 (vgaCRReg) & 0x02) << 7;
  info->screen_y |= (VGAIN8 (vgaCRReg) & 0x40) << 3;
  ++info->screen_y;

  printf ("[s3_vid] x = %d, y = %d, bpp = %d\n", info->screen_x, info->screen_y, info->bpp);
}

static void S3StreamsOff (void)
{
  unsigned char jStreamsControl;

  if (info->chip.arch == S3_TRIO64V)
    OUTREG (STREAMS_FIFO_REG, (20 << 10));

  VGAOUT8 (vgaCRIndex, EXT_MISC_CTRL2);
  if (S3_SAVAGE_MOBILE_SERIES (info->chip.arch) ||
      (info->chip.arch == S3_SUPERSAVAGE) || (info->chip.arch == S3_SAVAGE2000))
    jStreamsControl = VGAIN8 (vgaCRReg) & NO_STREAMS;
  else
    jStreamsControl = VGAIN8 (vgaCRReg) & NO_STREAMS_OLD;

  VerticalRetraceWait ();
  VGAOUT16 (vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2);

  if (S3_SAVAGE_SERIES (info->chip.arch))
  {
    VGAOUT16 (vgaCRIndex, 0x0093);
    VGAOUT8 (vgaCRIndex, 0x92);
    VGAOUT8 (vgaCRReg, VGAIN8 (vgaCRReg) & 0x40);
  }
}

static int find_chip (unsigned chip_id)
{
  unsigned i;

  for (i = 0; i < sizeof (s3_card_ids) / sizeof (struct s3_cards); i++)
    if (chip_id == s3_card_ids[i].chip_id)
      return i;
  return -1;
}

static int s3_probe (int verbose, int force)
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i, num_pci;
  int err;

  if (force)
    printf ("[s3_vid] Warning: forcing not supported yet!\n");
  err = pci_scan (lst, &num_pci);
  if (err)
  {
    printf ("[s3_vid] Error occurred during pci scan: %s\n", strerror (err));
    return err;
  }
  else
  {
    err = ENXIO;
    for (i = 0; i < num_pci; i++)
    {
      if (lst[i].vendor == VENDOR_S3_INC)
      {
        int idx;
        const char *dname;
        idx = find_chip (lst[i].device);
        if (idx == -1)
          continue;
        dname = pci_device_name (lst[i].vendor, lst[i].device);
        dname = dname ? dname : "Unknown chip";
        printf ("[s3_vid] Found chip: %s\n", dname);
        // FIXME: whats wrong here?
        if ((lst[i].command & PCI_COMMAND_IO) == 0)
        {
          printf ("[s3_vid] Device is disabled, ignoring\n");
          continue;
        }
        s3_cap.device_id = lst[i].device;
        err = 0;
        memcpy (&pci_info, &lst[i], sizeof (pciinfo_t));
        break;
      }
    }
  }
  if (err && verbose)
    printf ("[s3_vid] Can't find chip\n");
  return err;
}

static int s3_init (void)
{
  unsigned char cr36;
  int mtrr, videoRam;
  static unsigned char RamTrioVirge[] = { 4, 0, 3, 8, 2, 6, 1, 0 };
  static unsigned char RamSavage3D[] = { 8, 4, 4, 2 };
  static unsigned char RamSavage4[] = { 2, 4, 8, 12, 16, 32, 64, 32 };
  static unsigned char RamSavageMX[] = { 2, 8, 4, 16, 8, 16, 4, 16 };
  static unsigned char RamSavageNB[] = { 0, 2, 4, 8, 16, 32, 16, 2 };

  enable_app_io ();

  info = calloc (1, sizeof (s3_info));

  info->chip.arch = s3_card_ids[find_chip (pci_info.device)].arch;

  /* Switch to vga registers */
  OUTPORT8 (0x3c3, INPORT8 (0x3c3) | 0x01);
  OUTPORT8 (0x3c2, INPORT8 (0x3cc) | 0x01);
  /* Unlock extended registers */
  OUTPORT8 (vgaCRIndex, 0x38);
  OUTPORT8 (vgaCRReg, 0x48);
  OUTPORT8 (vgaCRIndex, 0x39);
  OUTPORT8 (vgaCRReg, 0xa0);

  if (info->chip.arch <= S3_VIRGE)
  {
    /* TODO: Improve detecting code */

    /* Enable LFB */
    OUTPORT8 (vgaCRIndex, LIN_ADDR_CTRL);
    OUTPORT8 (vgaCRReg,  INPORT8 (vgaCRReg) | ENABLE_LFB);
    /* Enable NewMMIO */
    OUTPORT8 (vgaCRIndex, EXT_MEM_CTRL1);
    OUTPORT8 (vgaCRReg,  INPORT8 (vgaCRReg) | ENABLE_NEWMMIO);
  }

  if (info->chip.arch < S3_SAVAGE3D)
    info->control_base = map_phys_mem (pci_info.base0 + S3_NEWMMIO_REGBASE, S3_NEWMMIO_REGSIZE);
  else if (info->chip.arch == S3_SAVAGE3D)
    info->control_base = map_phys_mem (pci_info.base0 + S3_NEWMMIO_REGBASE, S3_NEWMMIO_REGSIZE_SAVAGE);
  else
    info->control_base = map_phys_mem (pci_info.base0, S3_NEWMMIO_REGSIZE_SAVAGE);

  /* Unlock CRTC[0-7] */
  VGAOUT8 (vgaCRIndex, 0x11);
  VGAOUT8 (vgaCRReg, VGAIN8 (vgaCRReg) & 0x7f);
  /* Unlock sequencer */
  VGAOUT16 (0x3c4, 0x0608);
  /* Detect amount of installed ram */
  VGAOUT8 (vgaCRIndex, 0x36);
  cr36 = VGAIN8 (vgaCRReg);

  switch (info->chip.arch)
  {
  case S3_TRIO64V:
  case S3_VIRGE:
    videoRam = RamTrioVirge[(cr36 & 0xE0) >> 5] * 1024;
    break;

  case S3_SAVAGE3D:
    videoRam = RamSavage3D[(cr36 & 0xC0) >> 6] * 1024;
    break;

  case S3_SAVAGE4:
    /* 
     * The Savage4 has one ugly special case to consider.  On
     * systems with 4 banks of 2Mx32 SDRAM, the BIOS says 4MB
     * when it really means 8MB.  Why do it the same when you
     * can do it different...
     */
    VGAOUT8 (vgaCRIndex, 0x68);
    if ((VGAIN8 (vgaCRReg) & 0xC0) == (0x01 << 6))
      RamSavage4[1] = 8;

  case S3_SAVAGE2000:
    videoRam = RamSavage4[(cr36 & 0xE0) >> 5] * 1024;
    break;

  case S3_SAVAGE_MX:
    videoRam = RamSavageMX[(cr36 & 0x0E) >> 1] * 1024;
    break;

  case S3_PROSAVAGE:
    videoRam = RamSavageNB[(cr36 & 0xE0) >> 5] * 1024;
    break;

  default:
    /* How did we get here? */
    videoRam = 0;
    break;
  }

  printf ("[s3_vid] VideoRam = %d\n", videoRam);
  info->chip.fbsize = videoRam * 1024;

  if (info->chip.arch <= S3_SAVAGE3D)
    mtrr = mtrr_set_type (pci_info.base0, info->chip.fbsize, MTRR_TYPE_WRCOMB);
  else
    mtrr = mtrr_set_type (pci_info.base1, info->chip.fbsize, MTRR_TYPE_WRCOMB);

  if (mtrr != 0)
    printf ("[s3_vid] Unable to setup MTRR: %s\n", strerror (mtrr));
  else
    printf ("[s3_vid] MTRR set up\n");

  S3GetScrProp (info);
  S3StreamsOn ();

  return 0;
}

static void s3_destroy (void)
{
  unmap_phys_mem (info->video_base, info->chip.fbsize);
  if (S3_SAVAGE_SERIES (info->chip.arch))
    unmap_phys_mem (info->control_base, S3_NEWMMIO_REGSIZE_SAVAGE);
  else
    unmap_phys_mem (info->control_base, S3_NEWMMIO_REGSIZE);

  free (info);
}

static int s3_get_caps (vidix_capability_t * to)
{
  memcpy (to, &s3_cap, sizeof (vidix_capability_t));
  return 0;
}

static int is_supported_fourcc (uint32_t fourcc)
{
  switch (fourcc)
  {
//FIXME: Burst Command Interface should be used 
// for planar to packed conversion
//    case IMGFMT_YV12:
//    case IMGFMT_I420:
  case IMGFMT_UYVY:
  case IMGFMT_YUY2:
  case IMGFMT_Y211:
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
  case IMGFMT_BGR24:
  case IMGFMT_BGR32:
    return 1;
  default:
    return 0;
  }
}

static int s3_query_fourcc (vidix_fourcc_t * to)
{
  if (is_supported_fourcc (to->fourcc))
  {
    to->depth = VID_DEPTH_ALL;
    to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
    return 0;
  }
  else
    to->depth = to->flags = 0;

  return ENOSYS;
}

#if 0
static int s3_get_gkeys (vidix_grkey_t * grkey)
{
  return 0;
}
#endif

static int s3_set_gkeys (const vidix_grkey_t * grkey)
{
  if (grkey->ckey.op == CKEY_FALSE)
  {
    info->use_colorkey = 0;
    info->vidixcolorkey = 0;
    printf ("[s3_vid] Colorkeying disabled\n");
  }
  else
  {
    info->use_colorkey = 1;
    info->vidixcolorkey = ((grkey->ckey.red << 16) | (grkey->ckey.green << 8) | grkey->ckey.blue);
    printf ("[s3_vid] Set colorkey 0x%x\n", info->vidixcolorkey);
  }
  if (S3SetColorKey)
    S3SetColorKey ();
  return 0;
}

static int s3_get_eq (vidix_video_eq_t * eq)
{
  memcpy (eq, &(info->eq), sizeof (vidix_video_eq_t));
  return 0;
}

static int s3_set_eq (const vidix_video_eq_t * eq)
{
  if (eq->cap & VEQ_CAP_BRIGHTNESS)
    info->eq.brightness = eq->brightness;
  if (eq->cap & VEQ_CAP_CONTRAST)
    info->eq.contrast = eq->contrast;
  if (eq->cap & VEQ_CAP_SATURATION)
    info->eq.saturation = eq->saturation;
  if (eq->cap & VEQ_CAP_HUE)
    info->eq.hue = eq->hue;
  if (S3SetColor)
    S3SetColor ();
  return 0;
}

static int s3_config_playback (vidix_playback_t * vinfo)
{
  unsigned int i, bpp;

  if (!is_supported_fourcc (vinfo->fourcc))
    return -1;

  info->src_w = vinfo->src.w;
  info->src_h = vinfo->src.h;

  info->drw_w = vinfo->dest.w;
  info->drw_h = vinfo->dest.h;

  info->wx = vinfo->dest.x;
  info->wy = vinfo->dest.y;
  info->format = vinfo->fourcc;

  info->eq.cap = VEQ_CAP_BRIGHTNESS | VEQ_CAP_CONTRAST |
                 VEQ_CAP_SATURATION | VEQ_CAP_HUE;
  info->eq.brightness = 0;
  info->eq.contrast = 0;
  info->eq.saturation = 0;
  info->eq.hue = 0;

  vinfo->offset.y = 0;
  vinfo->offset.v = 0;
  vinfo->offset.u = 0;

  vinfo->dest.pitch.y = 32;
  vinfo->dest.pitch.u = 32;
  vinfo->dest.pitch.v = 32;

  switch (vinfo->fourcc)
  {
  case IMGFMT_Y211:
    bpp = 1;
    break;
  case IMGFMT_BGR24:
    bpp = 3;
    break;
  case IMGFMT_BGR32:
    bpp = 4;
    break;
  default:
    bpp = 2;
    break;
  }

  info->pitch = ((info->src_w * bpp) + 15) & ~15;
  info->pitch |= ((info->pitch / bpp) << 16);

  vinfo->frame_size = (info->pitch & 0xffff) * info->src_h;
  info->frame_size = vinfo->frame_size;

  info->picture_offset = info->screen_x * info->screen_y * (info->bpp >> 3);
  if (info->picture_offset > (info->chip.fbsize - vinfo->frame_size))
  {
    printf ("[s3_vid] Not enough memory for overlay\n");
    return -1;
  }

  if (info->chip.arch <= S3_SAVAGE3D)
    info->video_base = map_phys_mem (pci_info.base0, info->chip.fbsize);
  else
    info->video_base = map_phys_mem (pci_info.base1, info->chip.fbsize);

  if (info->video_base == NULL)
  {
    printf ("[s3_vid] errno = %s\n", strerror (errno));
    return -1;
  }

  info->picture_base = (uint32_t) info->video_base + info->picture_offset;

  vinfo->dga_addr = (void *) (info->picture_base);

  vinfo->num_frames = (info->chip.fbsize - info->picture_offset) / vinfo->frame_size;
  if (vinfo->num_frames > VID_PLAY_MAXFRAMES)
    vinfo->num_frames = VID_PLAY_MAXFRAMES;

  for (i = 0; i < vinfo->num_frames; i++)
    vinfo->offsets[i] = vinfo->frame_size * i;

  return 0;
}

static int s3_playback_on (void)
{
  S3DisplayVideo ();
  return 0;
}

static int s3_playback_off (void)
{
  S3StreamsOff ();
  return 0;
}

static int s3_frame_sel (unsigned int frame)
{
  OUTREG (SSTREAM_FBADDR0_REG, info->picture_offset + (info->frame_size * frame));
  return 0;
}

VDXDriver s3_drv = {
  "s3",
  NULL,
  .probe = s3_probe,
  .get_caps = s3_get_caps,
  .query_fourcc = s3_query_fourcc,
  .init = s3_init,
  .destroy = s3_destroy,
  .config_playback = s3_config_playback,
  .playback_on = s3_playback_on,
  .playback_off = s3_playback_off,
  .frame_sel = s3_frame_sel,
  .get_eq = s3_get_eq,
  .set_eq = s3_set_eq,
  .set_gkey = s3_set_gkeys,
};
