/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "config.h"
#include "core/mp_msg.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "video/memcpy_pic.h"

enum mode { PROGRESSIVE, TOP_FIRST, BOTTOM_FIRST,
	    TOP_FIRST_ANALYZE, BOTTOM_FIRST_ANALYZE,
	    ANALYZE, FULL_ANALYZE, AUTO, AUTO_ANALYZE };

#define fixed_mode(p) ((p)<=BOTTOM_FIRST)

struct vf_priv_s
   {
   enum mode mode;
   int verbose;
   unsigned char *buf[3];
   };

/*
 * Copy fields from either current or buffered previous frame to the
 * output and store the current frame unmodified to the buffer.
 */

static void do_plane(unsigned char *to, unsigned char *from,
		     int w, int h, int ts, int fs,
		     unsigned char **bufp, enum mode mode)
   {
   unsigned char *buf, *end;
   int top;

   if(!*bufp)
      {
      mode=PROGRESSIVE;
      if(!(*bufp=malloc(h*w))) return;
      }

   for(end=to+h*ts, buf=*bufp, top=1; to<end; from+=fs, to+=ts, buf+=w, top^=1)
      {
      memcpy(to, mode==(top?BOTTOM_FIRST:TOP_FIRST)?buf:from, w);
      memcpy(buf, from, w);
      }
   }

/*
 * This macro interpolates the value of both fields at a point halfway
 * between lines and takes the squared difference. In field resolution
 * the point is a quarter pixel below a line in one field and a quarter
 * pixel above a line in other.
 *
 * (the result is actually multiplied by 25)
 */

#define diff(a, as, b, bs) (t=((*a-b[bs])<<2)+a[as<<1]-b[-bs], t*t)

/*
 * Find which field combination has the smallest average squared difference
 * between the fields.
 */

static enum mode analyze_plane(unsigned char *old, unsigned char *new,
			       int w, int h, int os, int ns, enum mode mode,
			       int verbose, int fields)
   {
   double bdiff, pdiff, tdiff, scale;
   int bdif, tdif, pdif;
   int top, t;
   unsigned char *end, *rend;

   if(mode==AUTO)
      mode=fields&MP_IMGFIELD_ORDERED?fields&MP_IMGFIELD_TOP_FIRST?
	 TOP_FIRST:BOTTOM_FIRST:PROGRESSIVE;
   else if(mode==AUTO_ANALYZE)
      mode=fields&MP_IMGFIELD_ORDERED?fields&MP_IMGFIELD_TOP_FIRST?
	 TOP_FIRST_ANALYZE:BOTTOM_FIRST_ANALYZE:FULL_ANALYZE;

   if(fixed_mode(mode))
      bdiff=pdiff=tdiff=65536.0;
   else
      {
      bdiff=pdiff=tdiff=0.0;

      for(end=new+(h-2)*ns, new+=ns, old+=os, top=0;
	  new<end; new+=ns-w, old+=os-w, top^=1)
	 {
	 pdif=tdif=bdif=0;

	 switch(mode)
	    {
	    case TOP_FIRST_ANALYZE:
	       if(top)
		  for(rend=new+w; new<rend; new++, old++)
		     pdif+=diff(new, ns, new, ns),
		     tdif+=diff(new, ns, old, os);
	       else
		  for(rend=new+w; new<rend; new++, old++)
		     pdif+=diff(new, ns, new, ns),
		     tdif+=diff(old, os, new, ns);
	       break;

	    case BOTTOM_FIRST_ANALYZE:
	       if(top)
		  for(rend=new+w; new<rend; new++, old++)
		     pdif+=diff(new, ns, new, ns),
		     bdif+=diff(old, os, new, ns);
	       else
		  for(rend=new+w; new<rend; new++, old++)
		     pdif+=diff(new, ns, new, ns),
		     bdif+=diff(new, ns, old, os);
	       break;

	    case ANALYZE:
	       if(top)
		  for(rend=new+w; new<rend; new++, old++)
		     tdif+=diff(new, ns, old, os),
		     bdif+=diff(old, os, new, ns);
	       else
		  for(rend=new+w; new<rend; new++, old++)
		     bdif+=diff(new, ns, old, os),
                     tdif+=diff(old, os, new, ns);
	       break;

	    default: /* FULL_ANALYZE */
	       if(top)
		  for(rend=new+w; new<rend; new++, old++)
		     pdif+=diff(new, ns, new, ns),
		     tdif+=diff(new, ns, old, os),
		     bdif+=diff(old, os, new, ns);
	       else
		  for(rend=new+w; new<rend; new++, old++)
		     pdif+=diff(new, ns, new, ns),
		     bdif+=diff(new, ns, old, os),
		     tdif+=diff(old, os, new, ns);
	    }

	 pdiff+=(double)pdif;
	 tdiff+=(double)tdif;
	 bdiff+=(double)bdif;
	 }

      scale=1.0/(w*(h-3))/25.0;
      pdiff*=scale;
      tdiff*=scale;
      bdiff*=scale;

      if(mode==TOP_FIRST_ANALYZE)
	 bdiff=65536.0;
      else if(mode==BOTTOM_FIRST_ANALYZE)
	 tdiff=65536.0;
      else if(mode==ANALYZE)
	 pdiff=65536.0;

      if(bdiff<pdiff && bdiff<tdiff)
	 mode=BOTTOM_FIRST;
      else if(tdiff<pdiff && tdiff<bdiff)
	 mode=TOP_FIRST;
      else
	 mode=PROGRESSIVE;
      }

   if( mp_msg_test(MSGT_VFILTER,MSGL_V) )
      {
      mp_msg(MSGT_VFILTER, MSGL_INFO, "%c", mode==BOTTOM_FIRST?'b':mode==TOP_FIRST?'t':'p');
      if(tdiff==65536.0) mp_msg(MSGT_VFILTER, MSGL_INFO,"     N/A "); else mp_msg(MSGT_VFILTER, MSGL_INFO," %8.2f", tdiff);
      if(bdiff==65536.0) mp_msg(MSGT_VFILTER, MSGL_INFO,"     N/A "); else mp_msg(MSGT_VFILTER, MSGL_INFO," %8.2f", bdiff);
      if(pdiff==65536.0) mp_msg(MSGT_VFILTER, MSGL_INFO,"     N/A "); else mp_msg(MSGT_VFILTER, MSGL_INFO," %8.2f", pdiff);
      mp_msg(MSGT_VFILTER, MSGL_INFO,"        \n");
      }

   return mode;
   }

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
   {
   enum mode mode;

   struct mp_image *dmpi = vf_alloc_out_image(vf);
   mp_image_copy_attributes(dmpi, mpi);

   int pw[MP_MAX_PLANES] = {0};
   for (int p = 0; p < mpi->num_planes; p++)
       pw[p] = ((mpi->w * mpi->fmt.bpp[p] + 7) / 8) >> mpi->fmt.xs[p];

   mode=vf->priv->mode;

   if(!vf->priv->buf[0])
      mode=PROGRESSIVE;
   else
      mode=analyze_plane(vf->priv->buf[0], mpi->planes[0],
			 pw[0], dmpi->h, pw[0], mpi->stride[0], mode,
			 vf->priv->verbose, mpi->fields);

   for (int p = 0; p < mpi->num_planes; p++) {
      do_plane(dmpi->planes[p], mpi->planes[p], pw[p], dmpi->plane_h[p],
               dmpi->stride[p], mpi->stride[p],
               &vf->priv->buf[p], mode);
   }

   talloc_free(mpi);
   return dmpi;
   }

static void uninit(struct vf_instance *vf)
   {
   if (!vf->priv)
       return;
   free(vf->priv->buf[0]);
   free(vf->priv->buf[1]);
   free(vf->priv->buf[2]);
   free(vf->priv);
   }

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (IMGFMT_IS_HWACCEL(fmt))
        return 0;
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (desc.num_planes > 3)
        return 0;
    return vf_next_query_format(vf, fmt);
}

static int vf_open(vf_instance_t *vf, char *args)
   {
   vf->filter = filter;
   vf->uninit = uninit;
   vf->query_format = query_format;

   if(!(vf->priv = calloc(1, sizeof(struct vf_priv_s))))
      {
      uninit(vf);
      return 0;
      }

   vf->priv->mode=AUTO_ANALYZE;
   vf->priv->verbose=0;

   while(args && *args)
      {
      switch(*args)
	 {
	 case 't': vf->priv->mode=TOP_FIRST;            break;
	 case 'a': vf->priv->mode=AUTO;                 break;
	 case 'b': vf->priv->mode=BOTTOM_FIRST;         break;
	 case 'u': vf->priv->mode=ANALYZE;              break;
	 case 'T': vf->priv->mode=TOP_FIRST_ANALYZE;    break;
	 case 'A': vf->priv->mode=AUTO_ANALYZE;         break;
	 case 'B': vf->priv->mode=BOTTOM_FIRST_ANALYZE; break;
	 case 'U': vf->priv->mode=FULL_ANALYZE;         break;
	 case 'p': vf->priv->mode=PROGRESSIVE;          break;
	 case 'v': vf->priv->verbose=1;                 break;
	 case ':': break;

	 default:
	    uninit(vf);
	    return 0; /* bad args */
	 }

      if( (args=strchr(args, ':')) ) args++;
      }

   return 1;
   }

const vf_info_t vf_info_phase =
   {
   "phase shift fields",
   "phase",
   "Ville Saari",
   "",
   vf_open,
   NULL
   };
