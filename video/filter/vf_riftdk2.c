#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "config.h"
#include "common/msg.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"


// the pointers to the lookup tables for the Y and UV plane
struct vf_priv_s {
    int *Y;
    int *UV;
};

static void fill_half_of_lookup_table(int width, int height,
                                      int **lookup_table, int left)
{
    // default: left side of the image -> center of that is 
    // width/4, height/2
    int xmid = width / 4;
    int ymid = height / 2;
    // start at upper left corner
    int startx = 0;
    int starty = 0;
    // stop at the middle of the lowest line
    int stopx = width / 2;
    int stopy = height;
    // not the left side -> start in the middle in x direction
    if (left != 1) {
        // right side -> center of that is 3*width/4, height/2
        xmid = 3 * width / 4;
        // start at the middle of the first line
        startx = width / 2;
        // stop at the lower right corner
        stopx = width;
    }
    
    for (int x = startx; x < stopx; x++) {
        for (int y = starty; y < stopy; y++) {
            // inspired by: http://jsfiddle.net/s175ozts/4/
            int myX = xmid-x;
            int myY = ymid-y;
            // according to the author on the website above, we need to
            // normalize the radius for proper calculation in the atan,
            // sin and cos functions
            float maxR = sqrt(pow((stopx - startx) / 2.0, 2) +
                              pow(ymid / 2.0, 2));
            float myR = sqrt(pow(myX, 2) + pow(myY, 2));
            float myRN = myR / maxR;
            // use the distortion function
            float newR = myR * (0.24 * pow(myRN, 4) + 
                                0.22 * pow(myRN, 2) + 1);
            // then calculate back to x/y coordinates
            float alpha = atan2(myY, myX);
            float newXf = fabs(cos(alpha) * newR - xmid);
            float newYf = fabs(sin(alpha) * newR - ymid);
            // calculate the new radius to doublecheck (otherwise there
            // were some parts of the picture projected to the outside
            // the wished "barrel")
            float gnRadius = sqrt(pow(xmid - newXf, 2) + 
                                  pow(ymid - newYf, 2));
            int newX = (int) (newXf + 0.5);
            int newY = (int) (newYf + 0.5);
            // the above function gives us a double-concarve projection,
            // it is the inverse function of what we want but we use the
            // inverse to find out what would go outside the original
            // border. Those pixels need then to be set to black; the
            // real barrel projection is then created by swapping x and
            // newX when accessing the array values
            if(floor(newR) == floor(gnRadius) && newX >= startx &&
               newX < stopx && newY >= starty && newY < stopy)
            {
                (*lookup_table)[2 * y * width + 2 * x + 0] = newX;
                (*lookup_table)[2 * y * width + 2 * x + 1] = newY;
            } else {
                // use -1 to mark as invalid
                (*lookup_table)[2 * y * width + 2 * x + 0] = -1;
                (*lookup_table)[2 * y * width + 2 * x + 1] = -1;
            }
        }
    }       
}

static int init_lookup_table(int width, int height, int **lookup_table)
{
    // conservative check for possible overflow
    int maxPixels = sqrt(INT_MAX / sizeof(int) / 2);
    if (width > maxPixels || height > maxPixels)
        return 0;
    // we store x and y value -> "*2"
    *lookup_table = malloc(width * height * 2 * sizeof(int));
    fill_half_of_lookup_table(width, height, lookup_table, 1);
    fill_half_of_lookup_table(width, height, lookup_table, 0);
    return 1;
}

static int config(struct vf_instance *vf, int width, int height,
          int d_width, int d_height,
          unsigned int flags, unsigned int fmt)
{
    // In query_format, we accept only the IMGFMT_420P ->
    // we know we are working with YUV -> init both tables
    struct vf_priv_s *tables = vf->priv;
    if (!init_lookup_table(width, height, &(tables->Y)))
        return 0;
    if (!init_lookup_table(width / 2, height / 2, &(tables->UV)))
        return 0;
    return vf_next_config(vf, width, height, d_width, d_height, flags, fmt);
}

static void uninit(struct vf_instance *vf)
{
    struct vf_priv_s *tables = vf->priv;
	free(tables->Y);
	free(tables->UV);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mp_image_t *dmpi = vf_alloc_out_image(vf);
    if (!dmpi)
        return NULL;
    mp_image_copy_attributes(dmpi, mpi);
    
    for (int p = 0; p < mpi->num_planes; p++) {
        struct vf_priv_s *tables = vf->priv;
        int* lookup_table = p ? tables->UV : tables->Y;
        int curPlaneWidth = mpi->plane_w[p];
        int curPlaneHeight = mpi->plane_h[p];
        for (int y = 0; y < curPlaneHeight; y++) {
            // in lookup_table, we stored the double concarve projection
            // but now we need the inverse, convex projection -> use
            // newX,newY in src image and copy that pixel to x,y in the
            // destination
            uint8_t *p_dst = dmpi->planes[p] + dmpi->stride[p] * y;         
            for (int x = 0; x < curPlaneWidth; x++) {
                int newX = *lookup_table++;
                int newY = *lookup_table++;
                uint8_t *p_src = mpi->planes[p] + mpi->stride[p] * newY;
                if (newX == -1) {
                    // set to "black" for Y and UV
                    p_dst[x] = (p==0) ? 0 : 128;
                } else {
                    p_dst[x] = p_src[newX];
                }
            }
        }
    }

    talloc_free(mpi);
    return dmpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (fmt != IMGFMT_420P)
        return 0;
    return vf_next_query_format(vf, fmt);
}

static int vf_open(vf_instance_t *vf){
    vf->config=config;
    vf->filter=filter;
    vf->query_format=query_format;
    vf->uninit=uninit;
    return 1;
}

const vf_info_t vf_info_riftdk2 = {
    .description = "oculus rift dk2",
    .name = "riftdk2",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
};
