#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"
#include "../postproc/rgb2rgb.h"

struct vf_priv_s {
    int x1,y1,x2,y2;
    int limit;
};

static int checkline(unsigned char* src,int stride,int len,int bpp){
    int total=0;
    int div=len;
    int x;
    switch(bpp){
    case 1:
	while(--len>=0){
	    total+=src[0]; src+=stride;
	}
	break;
    case 3:
    case 4:
	while(--len>=0){
	    total+=src[0]+src[1]+src[2]; src+=stride;
	}
	div*=3;
	break;
    }
    total/=div;
//    printf("total=%d\n",total);
    return total;
}

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    vf->priv->x1=width;
    vf->priv->y1=height;
    vf->priv->x2=0;
    vf->priv->y2=0;
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;
    int bpp=mpi->bpp/8;
    int x,y;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_EXPORT, 0,
	mpi->w, mpi->h);

    dmpi->planes[0]=mpi->planes[0];
    dmpi->planes[1]=mpi->planes[1];
    dmpi->planes[2]=mpi->planes[2];
    dmpi->stride[0]=mpi->stride[0];
    dmpi->stride[1]=mpi->stride[1];
    dmpi->stride[2]=mpi->stride[2];
    dmpi->width=mpi->width;
    dmpi->height=mpi->height;

//static int checkline(unsigned char* src,int stride,int len,int bpp){
    
    for(y=0;y<vf->priv->y1;y++){
	if(checkline(mpi->planes[0]+mpi->stride[0]*y,bpp,mpi->w,bpp)>vf->priv->limit){
	    vf->priv->y1=y;
	    break;
	}
    }

    for(y=mpi->h-1;y>vf->priv->y2;y--){
	if(checkline(mpi->planes[0]+mpi->stride[0]*y,bpp,mpi->w,bpp)>vf->priv->limit){
	    vf->priv->y2=y;
	    break;
	}
    }

    for(y=0;y<vf->priv->x1;y++){
	if(checkline(mpi->planes[0]+bpp*y,mpi->stride[0],mpi->h,bpp)>vf->priv->limit){
	    vf->priv->x1=y;
	    break;
	}
    }

    for(y=mpi->w-1;y>vf->priv->x2;y--){
	if(checkline(mpi->planes[0]+bpp*y,mpi->stride[0],mpi->h,bpp)>vf->priv->limit){
	    vf->priv->x2=y;
	    break;
	}
    }

    x=(vf->priv->x1+1)&(~1);
    y=(vf->priv->y1+1)&(~1);
    
    printf("crop area: X: %d..%d  Y: %d..%d  (-vop crop=%d:%d:%d:%d)\n",
	vf->priv->x1,vf->priv->x2,
	vf->priv->y1,vf->priv->y2,
	(vf->priv->x2+1-x)&(~1),(vf->priv->y2+1-y)&(~1),x,y
	  );

    vf_next_put_image(vf,dmpi);
}

//===========================================================================//

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->limit=24; // should be option
    return 1;
}

vf_info_t vf_info_cropdetect = {
    "autodetect crop size",
    "cropdetect",
    "A'rpi",
    "",
    open
};

//===========================================================================//
