/*
 * vf_eq2.c
 *
 * Extended software equalizer (brightness, contrast, gamma)
 *
 * Hampa Hug <hhug@student.ethz.ch>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"


typedef struct vf_priv_s {
  unsigned char *buf;
  int           buf_w;
  int           buf_h;

  double        contrast;
  double        bright;
  double        gamma;

  unsigned char lut[256];
} vf_eq2_t;


static
void create_lut (vf_eq2_t *eq2)
{
  unsigned i;
  double   c, b, g;
  double   v;

  c = eq2->contrast;
  b = eq2->bright;
  g = eq2->gamma;

  if ((g < 0.001) || (g > 1000.0)) {
    g = 1.0;
  }

  fprintf (stderr, "vf_eq2: c=%.2f b=%.2f g=%.4f\n", c, b, g);

  g = 1.0 / g;

  for (i = 0; i < 256; i++) {
    v = (double) i / 255.0;
    v = c * (v - 0.5) + 0.5 + b;

    if (v <= 0.0) {
      eq2->lut[i] = 0;
    }
    else {
      v = pow (v, g);

      if (v >= 1.0) {
        eq2->lut[i] = 255;
      }
      else {
        eq2->lut[i] = (unsigned char) (256.0 * v);
      }
    }
  }
}

/* could inline this */
static
void process (unsigned char *dst, int dstride, unsigned char *src, int sstride,
  int w, int h, unsigned char lut[256])
{
  int i, j;

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      *(dst++) = lut[*(src++)];
    }

    src += sstride - w;
    dst += dstride - w;
  }
}

static
int put_image (vf_instance_t *vf, mp_image_t *src)
{
  mp_image_t *dst;
  vf_eq2_t   *eq2;

  eq2 = vf->priv;

  if ((eq2->buf == NULL) || (eq2->buf_w != src->stride[0]) || (eq2->buf_h != src->h)) {
    eq2->buf = (unsigned char *) realloc (eq2->buf, src->stride[0] * src->h);
    eq2->buf_w = src->stride[0];
    eq2->buf_h = src->h;
  }

  dst = vf_get_image (vf->next, src->imgfmt, MP_IMGTYPE_EXPORT, 0, src->w, src->h);

  dst->stride[0] = src->stride[0];
  dst->stride[1] = src->stride[1];
  dst->stride[2] = src->stride[2];
  dst->planes[0] = vf->priv->buf;
  dst->planes[1] = src->planes[1];
  dst->planes[2] = src->planes[2];

  process (
    dst->planes[0], dst->stride[0], src->planes[0], src->stride[0],
    src->w, src->h, eq2->lut
  );

  return vf_next_put_image (vf, dst);
}

static
int control (vf_instance_t *vf, int request, void *data)
{
  vf_equalizer_t *eq;

  switch (request) {
    case VFCTRL_SET_EQUALIZER:
      eq = (vf_equalizer_t *) data;

      if (strcmp (eq->item, "gamma") == 0) {
        vf->priv->gamma = exp (log (8.0) * eq->value / 100.0);
        create_lut (vf->priv);
        return CONTROL_TRUE;
      }
      else if (strcmp (eq->item, "contrast") == 0) {
        vf->priv->contrast = (1.0 / 100.0) * (eq->value + 100);
        create_lut (vf->priv);
        return CONTROL_TRUE;
      }
      else if (strcmp (eq->item, "brightness") == 0) {
        vf->priv->bright = (1.0 / 100.0) * eq->value;
        create_lut (vf->priv);
        return CONTROL_TRUE;
      }
      break;

    case VFCTRL_GET_EQUALIZER:
      eq = (vf_equalizer_t *) data;
      if (strcmp (eq->item, "gamma") == 0) {
        eq->value = (int) (100.0 * log (vf->priv->gamma) / log (8.0));
        return CONTROL_TRUE;
      }
      else if (strcmp (eq->item, "contrast") == 0) {
        eq->value = (int) (100.0 * vf->priv->contrast) - 100;
        return CONTROL_TRUE;
      }
      else if (strcmp (eq->item, "brightness") == 0) {
        eq->value = (int) (100.0 * vf->priv->bright);
        return CONTROL_TRUE;
      }
      break;
  }

  return vf_next_control (vf, request, data);
}

static
int query_format (vf_instance_t *vf, unsigned fmt)
{
  switch (fmt) {
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_CLPL:
    case IMGFMT_Y800:
    case IMGFMT_Y8:
    case IMGFMT_NV12:
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_411P:
      return vf_next_query_format (vf, fmt);
  }

  return 0;
}

static
void uninit (vf_instance_t *vf)
{
  if (vf->priv != NULL) {
    free (vf->priv->buf);
    free (vf->priv);
  }
}

static
int open (vf_instance_t *vf, char *args)
{
  vf_eq2_t *eq2;

  vf->control = control;
  vf->query_format = query_format;
  vf->put_image = put_image;
  vf->uninit = uninit;

  vf->priv = (vf_eq2_t *) malloc (sizeof (vf_eq2_t));
  eq2 = vf->priv;

  eq2->buf = NULL;
  eq2->buf_w = 0;
  eq2->buf_h = 0;

  eq2->gamma = 1.0;
  eq2->contrast = 1.0;
  eq2->bright = 0.0;

  if (args != NULL) {
    sscanf (args, "%lf:%lf:%lf", &eq2->gamma, &eq2->contrast, &eq2->bright);
  }

  create_lut (eq2);

  return 1;
}

vf_info_t vf_info_eq2 = {
  "extended software equalizer",
  "eq2",
  "Hampa Hug",
  "",
  &open
};
