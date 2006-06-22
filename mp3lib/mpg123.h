/*
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

/*
 * mpg123 defines
 * used source: musicout.h from mpegaudio package
 */

#include "config.h"

#ifndef M_PI
#define M_PI		3.141592653589793238462
#endif
#ifndef M_SQRT2
#define M_SQRT2		1.414213562373095048802
#endif
#define REAL_IS_FLOAT
#define NEW_DCT9

#undef MPG123_REMOTE           /* Get rid of this stuff for Win32 */

/*
#  define real float
#  define real long double
#  define real double
#include "audio.h"

// #define              AUDIOBUFSIZE            4096
*/

#define         FALSE                   0
#define         TRUE                    1

#define         MAX_NAME_SIZE           81
#define         SBLIMIT                 32
#define         SCALE_BLOCK             12
#define         SSLIMIT                 18

#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

/* #define MAXOUTBURST 32768 */

/* Pre Shift fo 16 to 8 bit converter table */
#define AUSHIFT (3)

struct al_table
{
  short bits;
  short d;
};

struct frame {
         struct al_table *alloc;
         int (*synth)(real *,int,unsigned char *,int *);
    int (*synth_mono)(real *,unsigned char *,int *);
    int stereo;
    int jsbound;
    int single;
    int II_sblimit;
    int down_sample_sblimit;
         int lsf;
         int mpeg25;
    int down_sample;
         int header_change;
    int lay;
    int error_protection;
    int bitrate_index;
    long sampling_frequency;
    int padding;
    int extension;
    int mode;
         int mode_ext;
    int copyright;
         int original;
         int emphasis;
         long framesize; /* computed framesize */
};


struct gr_info_s {
      int scfsi;
      unsigned part2_3_length;
      unsigned big_values;
      unsigned scalefac_compress;
      unsigned block_type;
      unsigned mixed_block_flag;
      unsigned table_select[3];
      unsigned subblock_gain[3];
      unsigned maxband[3];
      unsigned maxbandl;
      unsigned maxb;
      unsigned region1start;
      unsigned region2start;
      unsigned preflag;
      unsigned scalefac_scale;
      unsigned count1table_select;
      real *full_gain[3];
      real *pow2gain;
};

struct III_sideinfo
{
  unsigned main_data_begin;
  unsigned private_bits;
  struct {
         struct gr_info_s gr[2];
  } ch[2];
};

extern real mp3lib_decwin[(512+32)];
extern real *mp3lib_pnts[];

extern int synth_1to1_pent( real *,int,short * );
extern void make_decode_tables_MMX(long scaleval);
extern int synth_1to1_MMX( real *,int,short * );
extern int synth_1to1_MMX_s(real *, int, short *, short *, int *);

extern void dct36_3dnow(real *,real *,real *,real *,real *);
extern void dct36_3dnowex(real *,real *,real *,real *,real *);
extern void dct36_sse(real *,real *,real *,real *,real *);

typedef int (*synth_func_t)( real *,int,short * );
typedef void (*dct36_func_t)(real *,real *,real *,real *,real *);
