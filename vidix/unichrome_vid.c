/*
 * VIDIX driver for VIA CLE266/Unichrome chipsets.
 *
 * Copyright (C) 2004 Timothy Lee
 * Thanks to Gilles Frattini for bugfixes
 * Doxygen documentation by Benjamin Zores <ben@geexbox.org>
 * h/w revision detection by Timothy Lee <timothy.lee@siriushk.com>
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

#include "config.h"
#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"

#include "unichrome_regs.h"

/**
 * @brief Information on PCI device.
 */
static pciinfo_t pci_info;

/**
 * @brief Unichrome driver colorkey settings.
 */
static vidix_grkey_t uc_grkey;

static int frames[VID_PLAY_MAXFRAMES];
static uint8_t *vio;
static uint8_t *uc_mem;
static uint8_t mclk_save[3];
static uint8_t hwrev;

#define VIA_OUT(hwregs, reg, val)	*(volatile uint32_t *)((hwregs) + (reg)) = (val)
#define VIA_IN(hwregs, reg)		*(volatile uint32_t *)((hwregs) + (reg))
#define VGA_OUT8(hwregs, reg, val)	*(volatile uint8_t *)((hwregs) + (reg) + 0x8000) = (val)
#define VGA_IN8(hwregs, reg)		*(volatile uint8_t *)((hwregs) + (reg) + 0x8000)
#define VIDEO_OUT(hwregs, reg, val)	VIA_OUT((hwregs)+0x200, reg, val)
#define VIDEO_IN(hwregs, reg)		VIA_IN((hwregs)+0x200, reg)

#define outb(val,reg)	OUTPORT8(reg,val)
#define inb(reg)	INPORT8(reg)

#define ALIGN_TO(v, n) (((v) + (n-1)) & ~(n-1))
#define UC_MAP_V1_FIFO_CONTROL(depth, pre_thr, thr) \
    (((depth)-1) | ((thr) << 8) | ((pre_thr) << 24))

#define VIDEOMEMORY_SIZE	(8 * 1024 * 1024)
#define FRAMEBUFFER_SIZE	0x200000
#define FRAMEBUFFER_START	(VIDEOMEMORY_SIZE - FRAMEBUFFER_SIZE)

#ifdef DEBUG_LOGFILE
static FILE *logfile = 0;
#define LOGWRITE(x) {if(logfile) fprintf(logfile,x);}
#else
#define LOGWRITE(x)
#endif

/**
 * @brief Unichrome driver vidix capabilities.
 */
static vidix_capability_t uc_cap = {
  "VIA CLE266 Unichrome driver",
  "Timothy Lee <timothy@siriushk.com>",
  TYPE_OUTPUT,
  {0, 0, 0, 0},
  4096,
  4096,
  4,
  4,
  -1,
  FLAG_UPSCALER | FLAG_DOWNSCALER,
  VENDOR_VIA2,
  -1,
  {0, 0, 0, 0}
};

/**
 * @brief list of card IDs compliant with the Unichrome driver .
 */
static unsigned short uc_card_ids[] = {
  DEVICE_VIA2_VT8623_APOLLO_CLE266,
  DEVICE_VIA2_VT8378_S3_UNICHROME
};

/**
 * @brief Find chip index in Unichrome compliant devices list.
 *
 * @param chip_id PCI device ID.
 *
 * @returns index position in uc_card_ids if successful.
 *          -1 if chip_id is not a compliant chipset ID.
 */
static int
find_chip (unsigned chip_id)
{
  unsigned i;
  for (i = 0; i < sizeof (uc_card_ids) / sizeof (unsigned short); i++)
    {
      if (chip_id == uc_card_ids[i])
	return i;
    }
  return -1;
}

/**
 * @brief Map hardware settings for vertical scaling.
 *
 * @param sh source height.
 * @param dh destination height.
 * @param zoom will hold vertical setting of zoom register.
 * @param mini will hold vertical setting of mini register.
 *
 * @returns 1 if successful.
 *          0 if the zooming factor is too large or small.
 *
 * @note Derived from VIA's V4L driver.
 *       See ddover.c, DDOVER_HQVCalcZoomHeight()
 */
static int
uc_ovl_map_vzoom (uint32_t sh, uint32_t dh, uint32_t * zoom, uint32_t * mini)
{
  uint32_t sh1, tmp, d;
  int zoom_ok = 1;

  if (sh == dh) /* No zoom */
    {
      /* Do nothing */
    }
  else if (sh < dh) /* Zoom in */
    {
      tmp = (sh * 0x0400) / dh;
      zoom_ok = !(tmp > 0x3ff);

      *zoom |= (tmp & 0x3ff) | V1_Y_ZOOM_ENABLE;
      *mini |= V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY;
    }
  else /* sw > dh - Zoom out */
    {
      /* Find a suitable divider (1 << d) = {2, 4, 8 or 16} */
      sh1 = sh;
      for (d = 1; d < 5; d++)
	{
	  sh1 >>= 1;
	  if (sh1 <= dh)
	    break;
	}
      if (d == 5) /* too small */
	{
	  d = 4;
	  zoom_ok = 0;
	}

      *mini |= ((d << 1) - 1) << 16;	/* <= {1,3,5,7} << 16 */

      /* Add scaling */
      if (sh1 < dh)
	{
	  tmp = (sh1 * 0x400) / dh;
	  *zoom |= ((tmp & 0x3ff) | V1_Y_ZOOM_ENABLE);
	  *mini |= V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY;
	}
    }

  return zoom_ok;
}

/**
 * @brief Map hardware settings for horizontal scaling.
 *
 * @param sw source width.
 * @param dw destination width.
 * @param zoom will hold horizontal setting of zoom register.
 * @param mini will hold horizontal setting of mini register.
 * @param falign will hold fetch aligment.
 * @param dcount will hold display count.
 *
 * @returns 1 if successful.
 *          0 if the zooming factor is too large or small.
 *
 * @note Derived from VIA's V4L driver.
 *       See ddover.c, DDOVER_HQVCalcZoomWidth() and DDOver_GetDisplayCount()
 */
static int
uc_ovl_map_hzoom (uint32_t sw, uint32_t dw, uint32_t * zoom, uint32_t * mini,
		  int *falign, int *dcount)
{
  uint32_t tmp, sw1, d;
  int md; /* Minify-divider */
  int zoom_ok = 1;

  md = 1;
  *falign = 0;

  if (sw == dw) /* no zoom */
    {
      /* Do nothing */
    }
  else if (sw < dw) /* zoom in */
    {
      tmp = (sw * 0x0800) / dw;
      zoom_ok = !(tmp > 0x7ff);

      *zoom |= ((tmp & 0x7ff) << 16) | V1_X_ZOOM_ENABLE;
      *mini |= V1_X_INTERPOLY;
    }
  else /* sw > dw - Zoom out */
    {
      /* Find a suitable divider (1 << d) = {2, 4, 8 or 16} */
      sw1 = sw;
      for (d = 1; d < 5; d++)
	{
	  sw1 >>= 1;
	  if (sw1 <= dw)
	    break;
	}
      if (d == 5) /* too small */
	{
	  d = 4;
	  zoom_ok = 0;
	}

      md = 1 << d; /* <= {2,4,8,16} */
      *falign = ((md << 1) - 1) & 0xf; /* <= {3,7,15,15} */
      *mini |= V1_X_INTERPOLY;
      *mini |= ((d << 1) - 1) << 24; /* <= {1,3,5,7} << 24 */

      /* Add scaling */
      if (sw1 < dw)
	{
	  /* CLE bug */
	  /* tmp = sw1*0x0800 / dw; */
	  tmp = (sw1 - 2) * 0x0800 / dw;
	  *zoom |= ((tmp & 0x7ff) << 16) | V1_X_ZOOM_ENABLE;
	}
    }

  *dcount = sw - md;
  return zoom_ok;
}

/**
 * @brief qword fetch register setting.
 *
 * @param format overlay pixel format.
 * @param sw source width.
 *
 * @return qword fetch register setting
 *
 * @note Derived from VIA's V4L driver. See ddover.c, DDOver_GetFetch()
 * @note Only call after uc_ovl_map_hzoom()
 */
static uint32_t
uc_ovl_map_qwfetch (uint32_t format, int sw)
{
  uint32_t fetch = 0;

  switch (format)
    {
    case IMGFMT_YV12:
    case IMGFMT_I420:
      fetch = ALIGN_TO (sw, 32) >> 4;
      break;
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
    case IMGFMT_YUY2:
      fetch = (ALIGN_TO (sw << 1, 16) >> 4) + 1;
      break;
    case IMGFMT_BGR15:
    case IMGFMT_BGR16:
      fetch = (ALIGN_TO (sw << 1, 16) >> 4) + 1;
      break;
    case IMGFMT_BGR32:
      fetch = (ALIGN_TO (sw << 2, 16) >> 4) + 1;
      break;
    default:
      printf ("[unichrome] Unexpected pixelformat!");
      break;
    }

  if (fetch < 4)
    fetch = 4;

  return fetch;
}

/**
 * @brief Map pixel format.
 *
 * @param format pixel format.
 *
 * @return the mapped pixel format.
 *
 * @note Derived from VIA's V4L driver. See ddover.c, DDOver_GetV1Format()
 */
static uint32_t
uc_ovl_map_format (uint32_t format)
{
  switch (format)
    {
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
    case IMGFMT_YUY2:
      return V1_COLORSPACE_SIGN | V1_YUV422;
    case IMGFMT_IYUV:
      return V1_COLORSPACE_SIGN | V1_YCbCr420 | V1_SWAP_SW;
    case IMGFMT_YV12:
    case IMGFMT_I420:
      return V1_COLORSPACE_SIGN | V1_YCbCr420;
    case IMGFMT_BGR15:
      return V1_RGB15;
    case IMGFMT_BGR16:
      return V1_RGB16;
    case IMGFMT_BGR32:
      return V1_RGB32;
    default:
      printf ("[unichrome] Unexpected pixelformat!");
      return V1_YUV422;
    }
}

/**
 * @brief Calculate V1 control and fifo-control register values.
 *
 * @param format pixel format.
 * @param sw source width.
 * @param hwrev CLE266 hardware revision.
 * @param extfifo_on set this 1 if the extended FIFO is enabled.
 * @param control will hold value for V1_CONTROL.
 * @param fifo will hold value for V1_FIFO_CONTROL.
 */
static void
uc_ovl_map_v1_control (uint32_t format, int sw,
		       int hwrev, int extfifo_on,
		       uint32_t * control, uint32_t * fifo)
{
  *control = V1_BOB_ENABLE | uc_ovl_map_format (format);

  if (hwrev == 0x10)
    {
      *control |= V1_EXPIRE_NUM_F;
    }
  else
    {
      if (extfifo_on)
	{
	  *control |= V1_EXPIRE_NUM_A | V1_FIFO_EXTENDED;
	}
      else
	{
	  *control |= V1_EXPIRE_NUM;
	}
    }

  if ((format == IMGFMT_YV12) || (format == IMGFMT_I420))
    {
      /* Minified video will be skewed without this workaround. */
      if (sw <= 80) /* Fetch count <= 5 */
	{
	  *fifo = UC_MAP_V1_FIFO_CONTROL (16, 0, 0);
	}
      else
	{
	  if (hwrev == 0x10)
	    *fifo = UC_MAP_V1_FIFO_CONTROL (64, 56, 56);
	  else
	    *fifo = UC_MAP_V1_FIFO_CONTROL (16, 12, 8);
	}
    }
  else
    {
      if (hwrev == 0x10)
	{
	  *fifo = UC_MAP_V1_FIFO_CONTROL (64, 56, 56); /* Default rev 0x10 */
	}
      else
	{
	  if (extfifo_on)
	    *fifo = UC_MAP_V1_FIFO_CONTROL (48, 40, 40);
	  else
	    *fifo = UC_MAP_V1_FIFO_CONTROL (32, 29, 16); /* Default */
	}
    }
}

/**
 * @brief Setup extended FIFO.
 *
 * @param extfifo_on pointer determining if extended fifo is enable or not.
 * @param dst_w destination width.
 */
static void
uc_ovl_setup_fifo (int *extfifo_on, int dst_w)
{
  if (dst_w <= 1024) /* Disable extended FIFO */
    {
      outb (0x16, 0x3c4);
      outb (mclk_save[0], 0x3c5);
      outb (0x17, 0x3c4);
      outb (mclk_save[1], 0x3c5);
      outb (0x18, 0x3c4);
      outb (mclk_save[2], 0x3c5);
      *extfifo_on = 0;
    }
  else /* Enable extended FIFO */
    {
      outb (0x17, 0x3c4);
      outb (0x2f, 0x3c5);
      outb (0x16, 0x3c4);
      outb ((mclk_save[0] & 0xf0) | 0x14, 0x3c5);
      outb (0x18, 0x3c4);
      outb (0x56, 0x3c5);
      *extfifo_on = 1;
    }
}

static void
uc_ovl_vcmd_wait (volatile uint8_t * vio)
{
  while ((VIDEO_IN (vio, V_COMPOSE_MODE)
	  & (V1_COMMAND_FIRE | V3_COMMAND_FIRE)));
}

/**
 * @brief Probe hardware to find some useable chipset.
 *
 * @param verbose specifies verbose level.
 * @param force specifies force mode : driver should ignore
 *              device_id (danger but useful for new devices)
 *
 * @returns 0 if it can handle something in PC.
 *          a negative error code otherwise.
 */
static int
unichrome_probe (int verbose, int force)
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i, num_pci;
  int err;
  err = pci_scan (lst, &num_pci);
  if (err)
    {
      printf ("[unichrome] Error occurred during pci scan: %s\n",
	      strerror (err));
      return err;
    }
  else
    {
      err = ENXIO;
      for (i = 0; i < num_pci; i++)
	{
	  if (lst[i].vendor == VENDOR_VIA2)
	    {
	      int idx;
	      const char *dname;
	      idx = find_chip (lst[i].device);
	      if (idx == -1)
		continue;
	      dname = pci_device_name (VENDOR_VIA2, lst[i].device);
	      dname = dname ? dname : "Unknown chip";
	      printf ("[unichrome] Found chip: %s\n", dname);
	      if ((lst[i].command & PCI_COMMAND_IO) == 0)
		{
		  printf ("[unichrome] Device is disabled, ignoring\n");
		  continue;
		}
	      uc_cap.device_id = lst[i].device;
	      err = 0;
	      memcpy (&pci_info, &lst[i], sizeof (pciinfo_t));
	      break;
	    }
	}
    }

  if (err && verbose)
    printf ("[unichrome] Can't find chip\n");
  return err;
}

/**
 * @brief Initializes driver.
 *
 * @returns 0 if ok.
 *          a negative error code otherwise.
 */
static int
unichrome_init (void)
{
  long tmp;
  uc_mem = map_phys_mem (pci_info.base0, VIDEOMEMORY_SIZE);
  enable_app_io ();

  outb (0x2f, 0x3c4);
  tmp = inb (0x3c5) << 0x18;
  vio = map_phys_mem (tmp, 0x1000);

  outb (0x16, 0x3c4);
  mclk_save[0] = inb (0x3c5);
  outb (0x17, 0x3c4);
  mclk_save[1] = inb (0x3c5);
  outb (0x18, 0x3c4);
  mclk_save[2] = inb (0x3c5);

  uc_grkey.ckey.blue = 0x00;
  uc_grkey.ckey.green = 0x00;
  uc_grkey.ckey.red = 0x00;

  /* Detect whether we have a CLE266Ax or CLE266Cx */
  outb (0x4f, 0x3d4);
  tmp = inb (0x3d5);
  outb (0x4f, 0x3d4);
  outb (0x55, 0x3d5);
  outb (0x4f, 0x3d4);
  if (0x55 == inb (0x3d5))
  {
    /* Only CLE266Cx supports CR4F */
    hwrev = 0x11;
  }
  else
  {
    /* Otherwise assume to be a CLE266Ax */
    hwrev = 0x00;
  }
  outb (0x4f, 0x3d4);
  outb (tmp, 0x3d5);

#ifdef DEBUG_LOGFILE
  logfile = fopen ("/tmp/uc_vidix.log", "w");
#endif
  return 0;
}

/**
 * @brief Destroys driver.
 */
static void
unichrome_destroy (void)
{
#ifdef DEBUG_LOGFILE
  if (logfile)
    fclose (logfile);
#endif
  outb (0x16, 0x3c4);
  outb (mclk_save[0], 0x3c5);
  outb (0x17, 0x3c4);
  outb (mclk_save[1], 0x3c5);
  outb (0x18, 0x3c4);
  outb (mclk_save[2], 0x3c5);

  disable_app_io ();
  unmap_phys_mem (uc_mem, VIDEOMEMORY_SIZE);
  unmap_phys_mem (vio, 0x1000);
}

/**
 * @brief Get chipset's hardware capabilities.
 *
 * @param to Pointer to the vidix_capability_t structure to be filled.
 *
 * @returns 0.
 */
static int
unichrome_get_caps (vidix_capability_t * to)
{
  memcpy (to, &uc_cap, sizeof (vidix_capability_t));
  return 0;
}

/**
 * @brief Report if the video FourCC is supported by hardware.
 *
 * @param fourcc input image format.
 *
 * @returns 1 if the fourcc is supported.
 *          0 otherwise.
 */
static int
is_supported_fourcc (uint32_t fourcc)
{
  switch (fourcc)
    {
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
    case IMGFMT_YUY2:
    case IMGFMT_BGR15:
    case IMGFMT_BGR16:
    case IMGFMT_BGR32:
      return 1;
    default:
      return 0;
    }
}

/**
 * @brief Try to configure video memory for given fourcc.
 *
 * @param to Pointer to the vidix_fourcc_t structure to be filled.
 *
 * @returns 0 if ok.
 *          errno otherwise.
 */
static int
unichrome_query_fourcc (vidix_fourcc_t * to)
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

/**
 * @brief Get the GrKeys
 *
 * @param grkey Pointer to the vidix_grkey_t structure to be filled by driver.
 *
 * @return 0.
 */
static int
unichrome_get_gkey (vidix_grkey_t * grkey)
{
  memcpy (grkey, &uc_grkey, sizeof (vidix_grkey_t));
  return 0;
}

/**
 * @brief Set the GrKeys
 *
 * @param grkey Colorkey to be set.
 *
 * @return 0.
 */
static int
unichrome_set_gkey (const vidix_grkey_t * grkey)
{
  unsigned long dwCompose = VIDEO_IN (vio, V_COMPOSE_MODE) & ~0x0f;
  memcpy (&uc_grkey, grkey, sizeof (vidix_grkey_t));
  if (uc_grkey.ckey.op != CKEY_FALSE)
    {
      /* Set colorkey (how do I detect BPP in hardware ??) */
      unsigned long ckey;
      if (1) /* Assume 16-bit graphics */
	{
	  ckey = (grkey->ckey.blue & 0x1f)
	    | ((grkey->ckey.green & 0x3f) << 5)
	    | ((grkey->ckey.red & 0x1f) << 11);
	}
      else
	{
	  ckey = (grkey->ckey.blue)
	    | (grkey->ckey.green << 8) | (grkey->ckey.red << 16);
	}
      VIDEO_OUT (vio, V_COLOR_KEY, ckey);
      dwCompose |= SELECT_VIDEO_IF_COLOR_KEY;
    }

  /* Execute the changes */
  VIDEO_OUT (vio, V_COMPOSE_MODE, dwCompose | V1_COMMAND_FIRE);
  return 0;
}

/**
 * @brief Unichrome driver equalizer capabilities.
 */
static vidix_video_eq_t equal = {
  VEQ_CAP_BRIGHTNESS | VEQ_CAP_SATURATION | VEQ_CAP_HUE,
  300, 100, 0, 0, 0, 0, 0, 0
};


/**
 * @brief Get the equalizer capabilities.
 *
 * @param eq Pointer to the vidix_video_eq_t structure to be filled by driver.
 *
 * @return 0.
 */
static int
unichrome_get_eq (vidix_video_eq_t * eq)
{
  memcpy (eq, &equal, sizeof (vidix_video_eq_t));
  return 0;
}

/**
 * @brief Set the equalizer capabilities for color correction
 *
 * @param eq equalizer capabilities to be set.
 *
 * @return 0.
 */
static int
unichrome_set_eq (const vidix_video_eq_t * eq)
{
  return 0;
}

/**
 * @brief Y, U, V offsets.
 */
static int YOffs, UOffs, VOffs;

static int unichrome_frame_select (unsigned int frame);

/**
 * @brief Configure driver for playback. Driver should prepare BES.
 *
 * @param info configuration description for playback.
 *
 * @returns  0 in case of success.
 *          -1 otherwise.
 */
static int
unichrome_config_playback (vidix_playback_t * info)
{
  int src_w, drw_w;
  int src_h, drw_h;
  long base0, pitch = 0;
  int uv_size = 0, swap_uv;
  unsigned int i;
  int extfifo_on;

  /* Overlay register settings */
  uint32_t win_start, win_end;
  uint32_t zoom, mini;
  uint32_t dcount, falign, qwfetch;
  uint32_t v_ctrl, fifo_ctrl;

  if (!is_supported_fourcc (info->fourcc))
    return -1;

  src_w = info->src.w;
  src_h = info->src.h;

  drw_w = info->dest.w;
  drw_h = info->dest.h;

  /* Setup FIFO */
  uc_ovl_setup_fifo (&extfifo_on, src_w);

  /* Get image format, FIFO size, etc. */
  uc_ovl_map_v1_control (info->fourcc, src_w, hwrev, extfifo_on,
			 &v_ctrl, &fifo_ctrl);

  /* Setup layer window */
  win_start = (info->dest.x << 16) | info->dest.y;
  win_end = ((info->dest.x + drw_w - 1) << 16) | (info->dest.y + drw_h - 1);

  /* Get scaling and data-fetch parameters */
  zoom = 0;
  mini = 0;
  uc_ovl_map_vzoom (src_h, drw_h, &zoom, &mini);
  uc_ovl_map_hzoom (src_w, drw_w, &zoom, &mini, (int *) &falign, (int *) &dcount);
  qwfetch = uc_ovl_map_qwfetch (info->fourcc, src_w);

  /* Calculate buffer sizes */
  swap_uv = 0;
  switch (info->fourcc)
    {
    case IMGFMT_YV12:
      swap_uv = 1;
    case IMGFMT_I420:
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
      pitch = ALIGN_TO (src_w, 32);
      uv_size = (pitch >> 1) * (src_h >> 1);
      break;

    case IMGFMT_YUY2:
    case IMGFMT_BGR15:
    case IMGFMT_BGR16:
      pitch = ALIGN_TO (src_w << 1, 32);
      uv_size = 0;
      break;

    case IMGFMT_BGR32:
      pitch = ALIGN_TO (src_w << 2, 32);
      uv_size = 0;
      break;
    }
  if ((src_w > 4096) || (src_h > 4096) ||
      (src_w < 32) || (src_h < 1) || (pitch > 0x1fff))
    {
      printf ("[unichrome] Layer size out of bounds\n");
    }

  /* Calculate offsets */
  info->offset.y = 0;
  info->offset.v = info->offset.y + pitch * src_h;
  info->offset.u = info->offset.v + uv_size;
  info->frame_size = info->offset.u + uv_size;
  YOffs = info->offset.y;
  UOffs = (swap_uv ? info->offset.v : info->offset.u);
  VOffs = (swap_uv ? info->offset.u : info->offset.v);

  /* Assume we have 2 MB to play with */
  info->num_frames = FRAMEBUFFER_SIZE / info->frame_size;
  if (info->num_frames > VID_PLAY_MAXFRAMES)
    info->num_frames = VID_PLAY_MAXFRAMES;

  /* Start at 6 MB. Let's hope it's not in use. */
  base0 = FRAMEBUFFER_START;
  info->dga_addr = uc_mem + base0;

  info->dest.pitch.y = 32;
  info->dest.pitch.u = 32;
  info->dest.pitch.v = 32;

  for (i = 0; i < info->num_frames; i++)
    {
      info->offsets[i] = info->frame_size * i;
      frames[i] = base0 + info->offsets[i];
    }

  /* Write to the hardware */
  uc_ovl_vcmd_wait (vio);

  /* Configure diy_pitchlay parameters now */
  if (v_ctrl & V1_COLORSPACE_SIGN)
    {
      if (hwrev >= 0x10)
	{
	  VIDEO_OUT (vio, V1_ColorSpaceReg_2, ColorSpaceValue_2_3123C0);
	  VIDEO_OUT (vio, V1_ColorSpaceReg_1, ColorSpaceValue_1_3123C0);
	}
      else
	{
      VIDEO_OUT (vio, V1_ColorSpaceReg_2, ColorSpaceValue_2);
      VIDEO_OUT (vio, V1_ColorSpaceReg_1, ColorSpaceValue_1);
    }
    }

  VIDEO_OUT (vio, V1_CONTROL, v_ctrl);
  VIDEO_OUT (vio, V_FIFO_CONTROL, fifo_ctrl);

  VIDEO_OUT (vio, V1_WIN_START_Y, win_start);
  VIDEO_OUT (vio, V1_WIN_END_Y, win_end);

  VIDEO_OUT (vio, V1_SOURCE_HEIGHT, (src_h << 16) | dcount);

  VIDEO_OUT (vio, V12_QWORD_PER_LINE, qwfetch << 20);
  VIDEO_OUT (vio, V1_STRIDE, pitch | ((pitch >> 1) << 16));

  VIDEO_OUT (vio, V1_MINI_CONTROL, mini);
  VIDEO_OUT (vio, V1_ZOOM_CONTROL, zoom);

  /* Configure buffer address and execute the changes now! */
  unichrome_frame_select (0);

  return 0;
}

/**
 * @brief Set playback on : driver should activate BES on this call.
 *
 * @return 0.
 */
static int
unichrome_playback_on (void)
{
  LOGWRITE ("Enable overlay\n");

  /* Turn on overlay */
  VIDEO_OUT (vio, V1_CONTROL, VIDEO_IN (vio, V1_CONTROL) | V1_ENABLE);

  /* Execute the changes */
  VIDEO_OUT (vio, V_COMPOSE_MODE,
	     VIDEO_IN (vio, V_COMPOSE_MODE) | V1_COMMAND_FIRE);

  return 0;
}

/**
 * @brief Set playback off : driver should deactivate BES on this call.
 *
 * @return 0.
 */
static int
unichrome_playback_off (void)
{
  LOGWRITE ("Disable overlay\n");

  uc_ovl_vcmd_wait (vio);

  /* Restore FIFO */
  VIDEO_OUT (vio, V_FIFO_CONTROL, UC_MAP_V1_FIFO_CONTROL (16, 12, 8));

  /* Turn off overlay */
  VIDEO_OUT (vio, V1_CONTROL, VIDEO_IN (vio, V1_CONTROL) & ~V1_ENABLE);

  /* Execute the changes */
  VIDEO_OUT (vio, V_COMPOSE_MODE,
	     VIDEO_IN (vio, V_COMPOSE_MODE) | V1_COMMAND_FIRE);

  return 0;
}

/**
 * @brief Driver should prepare and activate corresponded frame.
 *
 * @param frame the frame index.
 *
 * @return 0.
 *
 * @note This function is used only for double and triple buffering
 *       and never used for single buffering playback.
 */
static int
unichrome_frame_select (unsigned int frame)
{
  LOGWRITE ("Frame select\n");

  uc_ovl_vcmd_wait (vio);

  /* Configure buffer address */
  VIDEO_OUT (vio, V1_STARTADDR_Y0, frames[frame] + YOffs);
  VIDEO_OUT (vio, V1_STARTADDR_CB0, frames[frame] + UOffs);
  VIDEO_OUT (vio, V1_STARTADDR_CR0, frames[frame] + VOffs);

  /* Execute the changes */
  VIDEO_OUT (vio, V_COMPOSE_MODE,
	     VIDEO_IN (vio, V_COMPOSE_MODE) | V1_COMMAND_FIRE);

  return 0;
}

VDXDriver unichrome_drv = {
  "unichrome",
  NULL,
  .probe = unichrome_probe,
  .get_caps = unichrome_get_caps,
  .query_fourcc = unichrome_query_fourcc,
  .init = unichrome_init,
  .destroy = unichrome_destroy,
  .config_playback = unichrome_config_playback,
  .playback_on = unichrome_playback_on,
  .playback_off = unichrome_playback_off,
  .frame_sel = unichrome_frame_select,
  .get_eq = unichrome_get_eq,
  .set_eq = unichrome_set_eq,
  .get_gkey = unichrome_get_gkey,
  .set_gkey = unichrome_set_gkey,
};
