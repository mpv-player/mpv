/*
        RoQ A/V decoder for the MPlayer program
        by Mike Melanson
        based on Dr. Tim Ferguson's RoQ document and accompanying source
        code found at:
          http://www.csse.monash.edu.au/~timf/videocodec.html
*/

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "bswap.h"
#include "mp_msg.h"
#include "mp_image.h"

#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

#define CLAMP_S16(x)  if (x < -32768) x = -32768; \
  else if (x > 32767) x = 32767;
#define SE_16BIT(x)  if (x & 0x8000) x -= 0x10000;

// RoQ chunk types
#define RoQ_INFO           0x1001
#define RoQ_QUAD_CODEBOOK  0x1002
#define RoQ_QUAD_VQ        0x1011
#define RoQ_SOUND_MONO     0x1020
#define RoQ_SOUND_STEREO   0x1021

#define MAX_ROQ_CODEBOOK_SIZE 256

// codebook entry for 2x2 vector
typedef struct
{
  // upper and lower luminance value pairs of 2x2 vector: [y0 y1], [y2 y3]
  unsigned short v2_y_u;
  unsigned short v2_y_l;

  // chrominance components
  unsigned char u, v;

  // maintain separate bytes for the luminance values as well
  unsigned char y0, y1, y2, y3;
} roq_v2_codebook;

// codebook entry for 4x4 vector
typedef struct
{
  // these variables are for rendering a 4x4 block built from 4 2x2
  // vectors [va vb vc vd]; e.g.:
  // v4_y_row1 = [va.y0 va.y1 vb.y0 vb.y1]
  // v4_y_row4 = [vc.y2 vc.y3 vd.y2 vd.y3]
  unsigned long v4_y_row1;
  unsigned long v4_y_row2;
  unsigned long v4_y_row3;
  unsigned long v4_y_row4;
  // ex: v4_u_row1 = [va.u vb.u]
  //     v4_u_row2 = [vc.u vd.u]
  unsigned short v4_u_row1;
  unsigned short v4_u_row2;
  unsigned short v4_v_row1;
  unsigned short v4_v_row2;

  // these variables are for rendering a 4x4 block doublesized to an
  // 8x8 block
  // ex: v4d_y_rows_12_l contains the 4 luminance values used to paint
  // the left half (4 pixels) of rows 1 and 2 of the 8x8 block, which
  // will be comprised from the original 2x2 vectors as
  // [va.y0 va.y0 va.y1 va.y1]
  unsigned long v4d_y_rows_12_l;
  unsigned long v4d_y_rows_12_r;
  unsigned long v4d_y_rows_34_l;
  unsigned long v4d_y_rows_34_r;
  unsigned long v4d_y_rows_56_l;
  unsigned long v4d_y_rows_56_r;
  unsigned long v4d_y_rows_78_l;
  unsigned long v4d_y_rows_78_r;
  // doublesized chrominance values
  // ex: v4d_u_rows_12 = [va.u va.u vb.u vb.u]
  unsigned long v4d_u_rows_12;
  unsigned long v4d_u_rows_34;
  unsigned long v4d_v_rows_12;
  unsigned long v4d_v_rows_34;
} roq_v4_codebook;

typedef struct
{
  roq_v2_codebook v2[MAX_ROQ_CODEBOOK_SIZE];
  roq_v4_codebook v4[MAX_ROQ_CODEBOOK_SIZE];
  mp_image_t *prev_frame;
} roqvideo_info;


// This function fills in the missing information for a v2 vector after
// loading the Y, U and V values.
inline void prep_v2(roq_v2_codebook *v2)
{
  v2->v2_y_u = be2me_16((v2->y0 << 8) | v2->y1);
  v2->v2_y_l = be2me_16((v2->y2 << 8) | v2->y3);
}

// This function fills in the missing information for a v4 vector based
// on 4 v2 indices.
void prep_v4(roq_v4_codebook *v4,
  roq_v2_codebook *v2_a, roq_v2_codebook *v2_b,
  roq_v2_codebook *v2_c, roq_v2_codebook *v2_d)
{
  // fill in the v4 variables
  v4->v4_y_row1 = be2me_32((v2_a->v2_y_u << 16) | v2_b->v2_y_u);
  v4->v4_y_row2 = be2me_32((v2_a->v2_y_l << 16) | v2_b->v2_y_l);
  v4->v4_y_row3 = be2me_32((v2_c->v2_y_u << 16) | v2_d->v2_y_u);
  v4->v4_y_row4 = be2me_32((v2_c->v2_y_l << 16) | v2_d->v2_y_l);

  v4->v4_u_row1 = be2me_16((v2_a->u << 8) | v2_b->u);
  v4->v4_u_row2 = be2me_16((v2_c->u << 8) | v2_d->u);

  v4->v4_v_row1 = be2me_16((v2_a->v << 8) | v2_b->v);
  v4->v4_v_row2 = be2me_16((v2_c->v << 8) | v2_d->v);

  // fill in the doublesized v4 variables
  v4->v4d_y_rows_12_l = be2me_32((v2_a->y0 << 24) | (v2_a->y0 << 16) |
    (v2_a->y1 << 8) | v2_a->y1);
  v4->v4d_y_rows_12_r = be2me_32((v2_b->y0 << 24) | (v2_b->y0 << 16) |
    (v2_b->y1 << 8) | v2_b->y1);

  v4->v4d_y_rows_34_l = be2me_32((v2_a->y2 << 24) | (v2_a->y2 << 16) |
    (v2_a->y3 << 8) | v2_a->y3);
  v4->v4d_y_rows_34_r = be2me_32((v2_b->y2 << 24) | (v2_b->y2 << 16) |
    (v2_b->y3 << 8) | v2_b->y3);

  v4->v4d_y_rows_56_l = be2me_32((v2_c->y0 << 24) | (v2_c->y0 << 16) |
    (v2_c->y1 << 8) | v2_c->y1);
  v4->v4d_y_rows_56_r = be2me_32((v2_d->y0 << 24) | (v2_d->y0 << 16) |
    (v2_d->y1 << 8) | v2_d->y1);

  v4->v4d_y_rows_78_l = be2me_32((v2_c->y2 << 24) | (v2_c->y2 << 16) |
    (v2_d->y3 << 8) | v2_d->y3);
  v4->v4d_y_rows_78_r = be2me_32((v2_c->y2 << 24) | (v2_c->y2 << 16) |
    (v2_d->y3 << 8) | v2_d->y3);

  v4->v4d_u_rows_12 = be2me_32((v2_a->u << 24) | (v2_a->u << 16) |
    (v2_b->u << 8) | v2_b->u);
  v4->v4d_u_rows_34 = be2me_32((v2_c->u << 24) | (v2_c->u << 16) |
    (v2_d->u << 8) | v2_d->u);

  v4->v4d_v_rows_12 = be2me_32((v2_a->v << 24) | (v2_a->v << 16) |
    (v2_b->v << 8) | v2_b->v);
  v4->v4d_v_rows_34 = be2me_32((v2_c->v << 24) | (v2_c->v << 16) |
    (v2_d->v << 8) | v2_d->v);
}

// This function copies the 4x4 block from the prev_*_planes to the
// current *_planes.
inline void copy_4x4_block(
  unsigned char *y_plane,
  unsigned char *u_plane,
  unsigned char *v_plane,
  unsigned char *prev_y_plane,
  unsigned char *prev_u_plane,
  unsigned char *prev_v_plane,
  unsigned int y_stride,
  unsigned int u_stride,
  unsigned int v_stride)
{
  int i;

  // copy over the luminance components (4 rows, 2 uints each)
  for (i = 0; i < 4; i++)
  {
    *(unsigned int *)y_plane = *(unsigned int *)prev_y_plane;
    y_plane += y_stride;
    prev_y_plane += y_stride;
  }

  // copy the chrominance values
  for (i = 0; i < 2; i++)
  {
    *(unsigned short*)u_plane = *(unsigned short*)prev_u_plane;
    u_plane += u_stride;
    *(unsigned short*)v_plane = *(unsigned short*)prev_v_plane;
    v_plane += v_stride;
  }
}

// This function copies the 8x8 block from the prev_*_planes to the
// current *_planes.
inline void copy_8x8_block(
  unsigned char *y_plane,
  unsigned char *u_plane,
  unsigned char *v_plane,
  unsigned char *prev_y_plane,
  unsigned char *prev_u_plane,
  unsigned char *prev_v_plane,
  unsigned int y_stride,
  unsigned int u_stride,
  unsigned int v_stride)
{
  int i;

  // copy over the luminance components (8 rows, 2 uints each)
  for (i = 0; i < 8; i++)
  {
    ((unsigned int *)y_plane)[0] = ((unsigned int *)prev_y_plane)[0];
    ((unsigned int *)y_plane)[1] = ((unsigned int *)prev_y_plane)[1];
    y_plane += y_stride;
    prev_y_plane += y_stride;
  }

  // copy the chrominance values
  for (i = 0; i < 4; i++)
  {
    *(unsigned int*)u_plane = *(unsigned int*)prev_u_plane;
    u_plane += u_stride;
    *(unsigned int*)v_plane = *(unsigned int*)prev_v_plane;
    v_plane += v_stride;
  }
}

// This function creates storage space for the vector codebooks.
void *roq_decode_video_init(void)
{
  roqvideo_info *info =
    (roqvideo_info *)malloc(sizeof(roqvideo_info));

  info->prev_frame = NULL;

  return info;
}

#define EMPTY_ROQ_CODEWORD 0xFFFF0000

#define FETCH_NEXT_CODE() \
  if (current_roq_codeword == EMPTY_ROQ_CODEWORD) \
  { \
    if (stream_ptr + 2 > encoded_size) \
    { \
      mp_msg(MSGT_DECVIDEO, MSGL_WARN,  \
        "RoQ video: stream pointer just went out of bounds (1)\n"); \
      return; \
    } \
    current_roq_codeword = (0x0000FFFF) | \
      (encoded[stream_ptr + 0] << 16) | \
      (encoded[stream_ptr + 1] << 24); \
    stream_ptr += 2; \
  } \
  roq_code = ((current_roq_codeword >> 30) & 0x03); \
  current_roq_codeword <<= 2;

#define FETCH_NEXT_ARGUMENT() \
  if (stream_ptr + 1 > encoded_size) \
  { \
    mp_msg(MSGT_DECVIDEO, MSGL_WARN,  \
      "RoQ video: stream pointer just went out of bounds (2)\n"); \
    return; \
  } \
  argument = encoded[stream_ptr++];

#define CHECK_PREV_FRAME() \
  if (!info->prev_frame) \
  { \
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, \
      "RoQ video: can't handle motion vector when there's no previous frame\n"); \
    return; \
  }

void roq_decode_video(void *context, unsigned char *encoded,
  int encoded_size, mp_image_t *mpi)
{
  roqvideo_info *info = (roqvideo_info *)context;

  int stream_ptr = 0;
  int i, j, k;
  int chunk_length;
  int v2_count;
  int v4_count;
  int v2_ia, v2_ib, v2_ic, v2_id;

  int roq_code;
  unsigned int current_roq_codeword = EMPTY_ROQ_CODEWORD;
  unsigned char argument = 0;
  int mean_motion_x;
  int mean_motion_y;
  int mx, my; // for calculating the motion vector

  int mblock_x = 0;
  int mblock_y = 0;
  int quad8_x, quad8_y;  // for pointing to 8x8 blocks in a macroblock
  int quad4_x, quad4_y;  // for pointing to 4x4 blocks in an 8x8 block
  int quad2_x, quad2_y;  // for pointing to 2x2 blocks in a 4x4 block

  unsigned char *y_plane;
  unsigned char *u_plane;
  unsigned char *v_plane;
  unsigned char *prev_y_plane;
  unsigned char *prev_u_plane;
  unsigned char *prev_v_plane;
  unsigned int y_stride = mpi->stride[0];
  unsigned int u_stride = mpi->stride[1];
  unsigned int v_stride = mpi->stride[2];

  roq_v4_codebook v4;
  roq_v2_codebook v2;

int debugger = 0;


  // make sure the encoded chunk is of minimal acceptable length
  if (encoded_size < 8)
  {
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, 
      "RoQ video: chunk isn't even 8 bytes long (minimum acceptable length)\n");
    return;
  }

  // make sure the resolution checks out
  if ((mpi->width % 16 != 0) || (mpi->height % 16 != 0))
  {
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, 
      "RoQ video resolution: %d x %d; expected dimensions divisible by 16\n");
    return;
  }

  if (LE_16(&encoded[stream_ptr]) == RoQ_QUAD_CODEBOOK)
  {
if (debugger)
printf ("parsing codebook\n");
    stream_ptr += 2;
    chunk_length = LE_32(&encoded[stream_ptr]);
    stream_ptr += 4;
    v4_count = encoded[stream_ptr++];
    v2_count = encoded[stream_ptr++];
    if (v2_count == 0)
      v2_count = 256;
    if ((v4_count == 0) && (v2_count * 6 < chunk_length))
      v4_count = 256;

    // make sure the lengths agree with each other
    if (((v2_count * 6) + (v4_count * 4)) != chunk_length)
    {
      mp_msg(MSGT_DECVIDEO, MSGL_WARN,
        "RoQ video: encountered quad codebook chunk with weird lengths (1)\n");
      return;
    }
    if ((v2_count * 6) > (encoded_size - stream_ptr))
    {
      mp_msg(MSGT_DECVIDEO, MSGL_WARN,
        "RoQ video: encountered quad codebook chunk with weird lengths (2)\n");
      return;
    }

    // load the 2x2 vectors
    for (i = 0; i < v2_count; i++)
    {
      info->v2[i].y0 = encoded[stream_ptr++];
      info->v2[i].y1 = encoded[stream_ptr++];
      info->v2[i].y2 = encoded[stream_ptr++];
      info->v2[i].y3 = encoded[stream_ptr++];
      info->v2[i].u = encoded[stream_ptr++];
      info->v2[i].v = encoded[stream_ptr++];
      prep_v2(&info->v2[i]);
    }

    if ((v4_count * 4) > (encoded_size - stream_ptr))
    {
      mp_msg(MSGT_DECVIDEO, MSGL_WARN,
        "RoQ video: encountered quad codebook chunk with weird lengths (3)\n");
      return;
    }

    // load the 4x4 vectors
    for (i = 0; i < v4_count; i++)
    {
      v2_ia = encoded[stream_ptr++];
      v2_ib = encoded[stream_ptr++];
      v2_ic = encoded[stream_ptr++];
      v2_id = encoded[stream_ptr++];
      prep_v4(&info->v4[i], &info->v2[v2_ia], &info->v2[v2_ib],
        &info->v2[v2_ic], &info->v2[v2_id]);
    }
  }

  if (LE_16(&encoded[stream_ptr]) == RoQ_QUAD_VQ)
  {
if (debugger)
printf ("parsing quad vq\n");
    stream_ptr += 2;
    chunk_length = LE_32(&encoded[stream_ptr]);
    stream_ptr += 4;
    mean_motion_y = encoded[stream_ptr++];
    mean_motion_x = encoded[stream_ptr++];
if (debugger){
for (i = 0; i < 16; i++)
  printf (" %02X", encoded[stream_ptr + i]);
printf("\n");}

    // iterate through the 16x16 macroblocks
    for (mblock_y = 0; mblock_y < mpi->height; mblock_y += 16)
    {
      for (mblock_x = 0; mblock_x < mpi->width; mblock_x += 16)
      {
        // iterate through the 4 quadrants of the macroblock
        for (i = 0; i < 4; i++)
        {
          quad8_x = mblock_x;
          quad8_y = mblock_y;
          if (i & 0x01) quad8_x += 8;
          if (i & 0x02) quad8_y += 8;

          // set up the planes
          y_plane = mpi->planes[0] + quad8_y * y_stride + quad8_x;
          u_plane = mpi->planes[1] + (quad8_y / 2) * u_stride + (quad8_x / 2);
          v_plane = mpi->planes[2] + (quad8_y / 2) * v_stride + (quad8_x / 2);

          // decide how to handle this 8x8 quad
          FETCH_NEXT_CODE();
if (debugger)
printf ("  (%d, %d), %d\n", quad8_x, quad8_y, roq_code);
          switch(roq_code)
          {
            // 8x8 block is painted with the same block as the last frame
            case 0:
              CHECK_PREV_FRAME();
              // prepare the pointers to the planes in the previous frame
              prev_y_plane = info->prev_frame->planes[0] +
                quad8_y * y_stride + quad8_x;
              prev_u_plane = info->prev_frame->planes[1] +
                (quad8_y / 2) * u_stride + (quad8_x / 2);
              prev_v_plane = info->prev_frame->planes[2] +
                (quad8_y / 2) * v_stride + (quad8_x / 2);

// sanity check before rendering
              copy_8x8_block(
                y_plane,
                u_plane,
                v_plane,
                prev_y_plane,
                prev_u_plane,
                prev_v_plane,
                y_stride,
                u_stride,
                v_stride
              );
              break;

            // 8x8 block is painted with an 8x8 block from the last frame
            // (i.e., motion compensation)
            case 1:
              CHECK_PREV_FRAME();

              // prepare the pointers to the planes in the previous frame
              FETCH_NEXT_ARGUMENT();  // argument contains motion vectors

              // figure out the motion vectors
              mx = quad8_x + 8 - (argument >> 4) - mean_motion_x;
              my = quad8_y + 8 - (argument & 0x0F) - mean_motion_y;

              prev_y_plane = info->prev_frame->planes[0] +
                my * y_stride + mx;
              prev_u_plane = info->prev_frame->planes[1] +
                (my / 2) * u_stride + (mx + 1) / 2;
              prev_v_plane = info->prev_frame->planes[2] +
                (my / 2) * v_stride + (mx + 1) / 2;

// sanity check before rendering
              copy_8x8_block(
                y_plane,
                u_plane,
                v_plane,
                prev_y_plane,
                prev_u_plane,
                prev_v_plane,
                y_stride,
                u_stride,
                v_stride
              );
              break;

            // 8x8 block is painted with a doublesized 4x4 vector
            case 2:
              FETCH_NEXT_ARGUMENT();
              v4 = info->v4[argument];
if (debugger)
printf ("    vector: %d, %08X %08X %08X %08X  %08X %08X\n", argument,
  v4.v4d_y_rows_12_l, v4.v4d_y_rows_12_r,
  v4.v4d_y_rows_34_l, v4.v4d_y_rows_34_r,
  v4.v4d_u_rows_12, v4.v4d_u_rows_34);

// sanity check before rendering
              // take care of the 8 luminance rows
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_12_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_12_r;
              y_plane += y_stride;
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_12_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_12_r;

              y_plane += y_stride;
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_34_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_34_r;
              y_plane += y_stride;
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_34_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_34_r;

              y_plane += y_stride;
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_56_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_56_r;
              y_plane += y_stride;
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_56_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_56_r;

              y_plane += y_stride;
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_78_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_78_r;
              y_plane += y_stride;
              ((unsigned int*)y_plane)[0] = v4.v4d_y_rows_78_l;
              ((unsigned int*)y_plane)[1] = v4.v4d_y_rows_78_r;

              // then the 4 U & V chrominance rows
              *(unsigned int*)u_plane = v4.v4d_u_rows_12;
              u_plane += u_stride;
              *(unsigned int*)u_plane = v4.v4d_u_rows_12;
              u_plane += u_stride;
              *(unsigned int*)u_plane = v4.v4d_u_rows_34;
              u_plane += u_stride;
              *(unsigned int*)u_plane = v4.v4d_u_rows_34;

              *(unsigned int*)v_plane = v4.v4d_v_rows_12;
              v_plane += v_stride;
              *(unsigned int*)v_plane = v4.v4d_v_rows_12;
              v_plane += v_stride;
              *(unsigned int*)v_plane = v4.v4d_v_rows_34;
              v_plane += v_stride;
              *(unsigned int*)v_plane = v4.v4d_v_rows_34;

              break;

            // 8x8 block is broken down into 4 4x4 blocks and painted using
            // 4 different codes.
            case 3:
              // iterate through 4 4x4 blocks
              for (j = 0; j < 4; j++)
              {
                quad4_x = quad8_x;
                quad4_y = quad8_y;
                if (j & 0x01) quad4_x += 4;
                if (j & 0x02) quad4_y += 4;

                // set up the planes
                y_plane = mpi->planes[0] + quad4_y * y_stride + quad4_x;
                u_plane = mpi->planes[1] + 
                  (quad4_y / 2) * u_stride + (quad4_x / 2);
                v_plane = mpi->planes[2] + 
                  (quad4_y / 2) * v_stride + (quad4_x / 2);

                // decide how to handle this 4x4 quad
                FETCH_NEXT_CODE();
if (debugger)
printf ("    (%d, %d), %d\n", quad4_x, quad4_y, roq_code);
                switch(roq_code)
                {
                  // 4x4 block is the same as in the previous frame
                  case 0:
                    CHECK_PREV_FRAME();
      
                    // prepare the pointers to the planes in the previous frame
                    prev_y_plane = info->prev_frame->planes[0] +
                      quad4_y * y_stride + quad4_x;
                    prev_u_plane = info->prev_frame->planes[1] +
                      (quad4_y / 2) * u_stride + (quad4_x / 2);
                    prev_v_plane = info->prev_frame->planes[2] +
                      (quad4_y / 2) * v_stride + (quad4_x / 2);

// sanity check before rendering
                    copy_4x4_block(
                      y_plane,
                      u_plane,
                      v_plane,
                      prev_y_plane,
                      prev_u_plane,
                      prev_v_plane,
                      y_stride,
                      u_stride,
                      v_stride
                    );
                    break;

                  // 4x4 block is motion compensated from the previous frame
                  case 1:
                    CHECK_PREV_FRAME();
                    // prepare the pointers to the planes in the previous frame
                    FETCH_NEXT_ARGUMENT();  // argument contains motion vectors

                    // figure out the motion vectors
                    mx = quad4_x + 8 - (argument >> 4) - mean_motion_x;
                    my = quad4_y + 8 - (argument & 0x0F) - mean_motion_y;

                    prev_y_plane = info->prev_frame->planes[0] +
                      my * y_stride + mx;
                    prev_u_plane = info->prev_frame->planes[1] +
                     (my / 2) * u_stride + (mx + 1) / 2;
                    prev_v_plane = info->prev_frame->planes[2] +
                     (my / 2) * u_stride + (mx + 1) / 2;

// sanity check before rendering
                    copy_4x4_block(
                      y_plane,
                      u_plane,
                      v_plane,
                      prev_y_plane,
                      prev_u_plane,
                      prev_v_plane,
                      y_stride,
                      u_stride,
                      v_stride
                    );
                    break;

                  // 4x4 block is copied directly from v4 vector table
                  case 2:
                    FETCH_NEXT_ARGUMENT();
                    v4 = info->v4[argument];

                    // copy the 4 luminance rows
                    *(unsigned int*)y_plane = v4.v4_y_row1;
                    y_plane += y_stride;
                    *(unsigned int*)y_plane = v4.v4_y_row2;
                    y_plane += y_stride;
                    *(unsigned int*)y_plane = v4.v4_y_row3;
                    y_plane += y_stride;
                    *(unsigned int*)y_plane = v4.v4_y_row4;
                    
                    // copy the U & V chrominance rows
                    *(unsigned short*)u_plane = v4.v4_u_row1;
                    u_plane += u_stride;
                    *(unsigned short*)u_plane = v4.v4_u_row2;

                    *(unsigned short*)v_plane = v4.v4_v_row1;
                    v_plane += v_stride;
                    *(unsigned short*)v_plane = v4.v4_v_row2;

                    break;

                  // 4x4 block is built from 4 2x2 vectors
                  case 3:
                    // iterate through 4 2x2 blocks
                    for (k = 0; k < 4; k++)
                    {
                      quad2_x = quad4_x;
                      quad2_y = quad4_y;
                      if (k & 0x01) quad2_x += 2;
                      if (k & 0x02) quad2_y += 2;

                      // set up the planes
                      y_plane = mpi->planes[0] + quad2_y * y_stride + quad2_x;
                      u_plane = mpi->planes[1] + 
                        (quad2_y / 2) * u_stride + (quad2_x / 2);
                      v_plane = mpi->planes[2] + 
                        (quad2_y / 2) * v_stride + (quad2_x / 2);

                      // fetch the next index into the v2 vector table
                      FETCH_NEXT_ARGUMENT();
if (debugger)
printf ("      (%d, %d), %d\n", quad2_x, quad2_y, argument);
                      v2 = info->v2[argument];

                      // copy the luminance components
                      *(unsigned short*)y_plane = v2.v2_y_u;
                      y_plane += y_stride;
                      *(unsigned short*)y_plane = v2.v2_y_l;
                      
                      // copy the U and V bytes
                      u_plane[0] = v2.u;
                      v_plane[0] = v2.v;
                    }
                    
                    break;
                }
              }
              break;
          }
        }
      }
    }
  }

  // one last sanity check on the way out
  // (apparently, it's not unusual to have 2 bytes left over after decode)
  if (stream_ptr < encoded_size - 2)
  {
      mp_msg(MSGT_DECVIDEO, MSGL_WARN,
        "RoQ video: completed frame decode with bytes left over (%d < %d)\n",
          stream_ptr, encoded_size);
  }

  // save the current frame as the previous frame for the next iteration
  info->prev_frame = mpi;
}

// Initialize the RoQ audio decoder, which is to say, initialize the table
// of squares.
void *roq_decode_audio_init(void)
{
  short *square_array;
  short square;
  int i;

  square_array = (short *)malloc(256 * sizeof(short));
  if (!square_array)
    return NULL;

  for (i = 0; i < 128; i++)
  {
    square = i * i;
    square_array[i] = square;
    square_array[i + 128] = -square;
  }

  return square_array;
}

int roq_decode_audio(
  unsigned short *output,
  unsigned char *input,
  int encoded_size,
  int channels,
  void *context)
{
  short *square_array = (short *)context;
  int i;
  int predictor[2];
  int channel_number = 0;

  // prepare the initial predictors
  if (channels == 1)
    predictor[0] = LE_16(&input[0]);
  else
  {
    predictor[0] = input[1] << 8;
    predictor[1] = input[0] << 8;
  }
  SE_16BIT(predictor[0]);
  SE_16BIT(predictor[1]);

  // decode the samples
  for (i = 2; i < encoded_size; i++)
  {
    predictor[channel_number] += square_array[input[i]];
    CLAMP_S16(predictor[channel_number]);
    output[i - 2] = predictor[channel_number];

    // toggle channel
    channel_number ^= channels - 1;
  }

  // return the number of samples decoded
  return (encoded_size - 2);
}
