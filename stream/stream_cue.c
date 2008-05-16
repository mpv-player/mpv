//=================== VideoCD BinCue ==========================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"

#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"
#include "libavutil/avstring.h"

#define byte    unsigned char
#define SIZERAW 2352
#define SIZEISO_MODE1 2048
#define SIZEISO_MODE2_RAW 2352
#define SIZEISO_MODE2_FORM1 2048
#define SIZEISO_MODE2_FORM2 2336
#define AUDIO 0
#define MODE1 1
#define MODE2 2
#define MODE1_2352 10
#define MODE2_2352 20
#define MODE1_2048 30
#define MODE2_2336 40
#define UNKNOWN -1

static struct stream_priv_s {
  char* filename;
} stream_priv_dflts = {
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  { "string", ST_OFF(filename), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static const struct m_struct_st stream_opts = {
  "cue",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

static FILE* fd_cue;
static int fd_bin = 0;

static char bin_filename[256];

static char cue_filename[256];
static char bincue_path[256];


typedef struct track
{
   unsigned short mode;
   unsigned short minute;
   unsigned short second;
   unsigned short frame;

   /* (min*60 + sec) * 75 + fps   */

   unsigned long start_sector;

   /* = the sizes in bytes off all tracks bevor this one */
   /* its needed if there are mode1 tracks befor the mpeg tracks */
   unsigned long start_offset;

   /*   unsigned char num[3]; */
} tTrack;

/* max 99 tracks on a cd */
static tTrack tracks[100];

static struct cue_track_pos {
  int track;
  unsigned short mode;
  unsigned short minute;
  unsigned short second;
  unsigned short frame;
} cue_current_pos;

/* number of tracks on the cd */
static int nTracks = 0;

static int digits2int(char s[2], int errval) {
  uint8_t a = s[0] - '0';
  uint8_t b = s[1] - '0';
  if (a > 9 || b > 9)
    return errval;
  return a * 10 + b;
}

/* presumes Line is preloaded with the "current" line of the file */
static int cue_getTrackinfo(char *Line, tTrack *track)
{
  int already_set = 0;

  /* Get the 'mode' */
  if (strncmp(&Line[2], "TRACK ", 6)==0)
  {
/*    strncpy(track->num, &Line[8], 2); track->num[2] = '\0'; */

    track->mode = UNKNOWN;
    if(strncmp(&Line[11], "AUDIO", 5)==0) track->mode = AUDIO;
    if(strncmp(&Line[11], "MODE1/2352", 10)==0) track->mode = MODE1_2352;
    if(strncmp(&Line[11], "MODE1/2048", 10)==0) track->mode = MODE1_2048;
    if(strncmp(&Line[11], "MODE2/2352", 10)==0) track->mode = MODE2_2352;
    if(strncmp(&Line[11], "MODE2/2336", 10)==0) track->mode = MODE2_2336;
  }
  else return 1;

  /* Get the track indexes */
  while(1) {
    if(! fgets( Line, 256, fd_cue ) ) { break;}

    if (strncmp(&Line[2], "TRACK ", 6)==0)
    {
      /* next track starting */
      break;
    }

    /* Track 0 or 1, take the first an get fill the values*/
    if (strncmp(&Line[4], "INDEX ", 6)==0)
    {
      /* check stuff here so if the answer is false the else stuff below won't be executed */
      if ((already_set == 0) && digits2int(Line + 10, 100) <= 1)
      {
        already_set = 1;

        track->minute = digits2int(Line + 13, 0);
        track->second = digits2int(Line + 16, 0);
        track->frame  = digits2int(Line + 19, 0);
      }
    }
    else if (strncmp(&Line[4], "PREGAP ", 7)==0) { ; /* ignore */ }
    else if (strncmp(&Line[4], "FLAGS ", 6)==0)  { ; /* ignore */ }
    else mp_msg (MSGT_OPEN,MSGL_INFO,
                 MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine, Line);
  }
  return 0;
}



/* FIXME: the string operations ( strcpy,strcat ) below depend
 * on the arrays to have the same size, thus we need to make
 * sure the sizes are in sync.
 */
static int cue_find_bin (char *firstline) {
  int i,j;
  char s[256];
  char t[256];

  /* get the filename out of that */
  /*                      12345 6  */
  mp_msg (MSGT_OPEN,MSGL_INFO, "[bincue] cue_find_bin(%s)\n", firstline);
  if (strncmp(firstline, "FILE \"",6)==0)
  {
    i = 0;
    j = 0;
    while ( firstline[6 + i] != '"')
    {
      bin_filename[j] = firstline[6 + i];

      /* if I found a path info, than delete all bevor it */
      switch (bin_filename[j])
      {
        case '\\':
          j = 0;
          break;

        case '/':
          j = 0;
          break;

        default:
          j++;
      }
      i++;
    }
    bin_filename[j+1] = '\0';

  }

  /* now try to open that file, without path */
  fd_bin = open (bin_filename, O_RDONLY);
  if (fd_bin == -1)
  {
    mp_msg(MSGT_OPEN,MSGL_STATUS, MSGTR_MPDEMUX_CUEREAD_BinFilenameTested,
           bin_filename);

    /* now try to find it with the path of the cue file */
    snprintf(s,sizeof( s ),"%s/%s",bincue_path,bin_filename);
    fd_bin = open (s, O_RDONLY);
    if (fd_bin == -1)
    {
      mp_msg(MSGT_OPEN,MSGL_STATUS,
             MSGTR_MPDEMUX_CUEREAD_BinFilenameTested, s);
      /* now I would say the whole filename is shit, build our own */
      strncpy(s, cue_filename, strlen(cue_filename) - 3 );
      s[strlen(cue_filename) - 3] = '\0';
      strcat(s, "bin");
      fd_bin = open (s, O_RDONLY);
      if (fd_bin == -1)
      {
        mp_msg(MSGT_OPEN,MSGL_STATUS,
               MSGTR_MPDEMUX_CUEREAD_BinFilenameTested, s);

        /* ok try it with path */
        snprintf(t, sizeof( t ), "%s/%s", bincue_path, s);
        fd_bin = open (t, O_RDONLY);
        if (fd_bin == -1)
        {
          mp_msg(MSGT_OPEN,MSGL_STATUS,
                 MSGTR_MPDEMUX_CUEREAD_BinFilenameTested,t);
          /* now I would say the whole filename is shit, build our own */
          strncpy(s, cue_filename, strlen(cue_filename) - 3 );
          s[strlen(cue_filename) - 3] = '\0';
          strcat(s, "img");
          fd_bin = open (s, O_RDONLY);
          if (fd_bin == -1)
          {
            mp_msg(MSGT_OPEN,MSGL_STATUS,
                   MSGTR_MPDEMUX_CUEREAD_BinFilenameTested, s);
            /* ok try it with path */
            snprintf(t, sizeof( t ), "%s/%s", bincue_path, s);
            fd_bin = open (t, O_RDONLY);
            if (fd_bin == -1)
            {
              mp_msg(MSGT_OPEN,MSGL_STATUS,
                     MSGTR_MPDEMUX_CUEREAD_BinFilenameTested, s);

              /* I'll give up */
              mp_msg(MSGT_OPEN,MSGL_ERR,
                     MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile);
              return -1;
            }
          }
        } else strcpy(bin_filename, t);

      } else strcpy(bin_filename, s);

    } else strcpy(bin_filename, s);

  }

  mp_msg(MSGT_OPEN,MSGL_INFO,
         MSGTR_MPDEMUX_CUEREAD_UsingBinFile, bin_filename);
  return 0;
}

static inline int cue_msf_2_sector(int minute, int second, int frame) {
 return frame + (second + minute * 60 ) * 75;
}

static inline int cue_get_msf(void) {
  return cue_msf_2_sector (cue_current_pos.minute,
                           cue_current_pos.second,
                           cue_current_pos.frame);
}

static inline void cue_set_msf(unsigned int sect){
  cue_current_pos.frame=sect%75;
  sect=sect/75;
  cue_current_pos.second=sect%60;
  sect=sect/60;
  cue_current_pos.minute=sect;
}

static inline int cue_mode_2_sector_size(int mode)
{
  switch (mode)
  {
    case AUDIO:      return AUDIO;
    case MODE1_2352: return SIZERAW;
    case MODE1_2048: return SIZEISO_MODE1;
    case MODE2_2352: return SIZEISO_MODE2_RAW;
    case MODE2_2336: return SIZEISO_MODE2_FORM2;

    default:
      mp_msg(MSGT_OPEN,MSGL_FATAL,
             MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile);
      abort();
  }

}


static int cue_read_cue (char *in_cue_filename)
{
  struct stat filestat;
  char sLine[256];
  unsigned int sect;
  char *s,*t;
  int i;

  /* we have no tracks at the beginning */
  nTracks = 0;

  fd_bin = 0;

  /* split the filename into a path and filename part */
  s = strdup(in_cue_filename);
  t = strrchr(s, '/');
  if (t == (char *)NULL)
     t = ".";
  else {
     *t = '\0';
     t = s;
     if (*t == '\0')
       strcpy(t, "/");
  }
  
  av_strlcpy(bincue_path,t,sizeof( bincue_path ));
  mp_msg(MSGT_OPEN,MSGL_V,"dirname: %s, cuepath: %s\n", t, bincue_path);

  /* no path at all? */
  if (strcmp(bincue_path, ".") == 0) {
    mp_msg(MSGT_OPEN,MSGL_V,"bincue_path: %s\n", bincue_path);
    av_strlcpy(cue_filename,in_cue_filename,sizeof( cue_filename ));
  } else {
    av_strlcpy(cue_filename,in_cue_filename + strlen(bincue_path) + 1,
            sizeof( cue_filename ));
  }



  /* open the cue file */
  fd_cue = fopen (in_cue_filename, "r");
  if (fd_cue == NULL)
  {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile, in_cue_filename);
    return -1;
  }

  /* read the first line and hand it to find_bin, which will
     test more than one possible name of the file */

  if(! fgets( sLine, 256, fd_cue ) )
  {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile, in_cue_filename);
    fclose (fd_cue);
    return -1;
  }

  if (cue_find_bin(sLine)) {
    fclose (fd_cue);
    return -1;
  }


  /* now build the track list */
  /* red the next line and call our track finder */
  if(! fgets( sLine, 256, fd_cue ) )
  {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile, in_cue_filename);
    fclose (fd_cue);
    return -1;
  }

  while(!feof(fd_cue))
  {
    if (cue_getTrackinfo(sLine, &tracks[nTracks++]) != 0)
    {
      mp_msg(MSGT_OPEN,MSGL_ERR,
             MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile, in_cue_filename);
      fclose (fd_cue);
      return -1;
    }
  }

  /* make a fake track with stands for the Lead out */
  if (fstat (fd_bin, &filestat) == -1) {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize);
    fclose (fd_cue);
    return -1;
  }

  sect = filestat.st_size / 2352;

  tracks[nTracks].frame = sect%75;
  sect=sect/75;
  tracks[nTracks].second = sect%60;
  sect=sect/60;
  tracks[nTracks].minute = sect;


  /* let's calculate the start sectors and offsets */
  for(i = 0; i <= nTracks; i++)
  {
    tracks[i].start_sector = cue_msf_2_sector(tracks[i].minute,
                                              tracks[nTracks].second,
                                              tracks[nTracks].frame);

    /* if we're the first track we don't need to offset of the one befor */
    if (i == 0)
    {
      /* was always 0 on my svcds, but who knows */
      tracks[0].start_offset = tracks[0].start_sector *
        cue_mode_2_sector_size(tracks[0].mode);
    } else
    {
      tracks[i].start_offset = tracks[i-1].start_offset +
        (tracks[i].start_sector - tracks[i-1].start_sector) *
        cue_mode_2_sector_size(tracks[i-1].mode);
    }
  }

  fclose (fd_cue);

  return fd_bin;
}




static int cue_read_toc_entry(void) {

  int track = cue_current_pos.track - 1;

  /* check if its a valid track, if not return -1 */
  if (track >= nTracks)
    return -1;


  switch (tracks[track].mode)
  {
    case AUDIO:
      cue_current_pos.mode = AUDIO;
      break;
    case MODE1_2352:
      cue_current_pos.mode = MODE1;
      break;
    case MODE1_2048:
      cue_current_pos.mode = MODE1;
      break;
    default: /* MODE2_2352 and MODE2_2336 */
      cue_current_pos.mode = MODE2;
  }
  cue_current_pos.minute = tracks[track].minute;
  cue_current_pos.second = tracks[track].second;
  cue_current_pos.frame = tracks[track].frame;

  return 0;
}

static int cue_vcd_seek_to_track (int track){
  cue_current_pos.track  = track;

  if (cue_read_toc_entry ())
    return -1;

  return VCD_SECTOR_DATA * cue_get_msf();
}

static int cue_vcd_get_track_end (int track){
  cue_current_pos.frame = tracks[track].frame;
  cue_current_pos.second = tracks[track].second;
  cue_current_pos.minute = tracks[track].minute;

  return VCD_SECTOR_DATA * cue_get_msf();
}

static void cue_vcd_read_toc(void){
  int i;
  for (i = 0; i < nTracks; ++i) {

    mp_msg(MSGT_OPEN,MSGL_INFO,
           MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat,
           i+1,
           tracks[i].mode,
           tracks[i].minute,
           tracks[i].second,
           tracks[i].frame
           );
  }
}

static int cue_vcd_read(stream_t *stream, char *mem, int size) {
  unsigned long position;
  int track = cue_current_pos.track - 1;

  position = tracks[track].start_offset +
             (cue_msf_2_sector(cue_current_pos.minute,
                               cue_current_pos.second,
                               cue_current_pos.frame) -
              tracks[track].start_sector)
             * cue_mode_2_sector_size(tracks[track].mode);

  
  if(position >= tracks[track+1].start_offset)
    return 0;

  if(lseek(fd_bin, position+VCD_SECTOR_OFFS, SEEK_SET) == -1) {
    mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF);
    return 0;
  }

  if(read(fd_bin, mem, VCD_SECTOR_DATA) != VCD_SECTOR_DATA) {
    mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload, VCD_SECTOR_DATA);
    return 0;
  }

  cue_current_pos.frame++;
  if (cue_current_pos.frame==75){
    cue_current_pos.frame=0;
    cue_current_pos.second++;
    if (cue_current_pos.second==60){
      cue_current_pos.second=0;
      cue_current_pos.minute++;
    }
  }

  return VCD_SECTOR_DATA;
}

static int seek(stream_t *s,off_t newpos) {
  s->pos=newpos;
  cue_set_msf(s->pos/VCD_SECTOR_DATA);
  return 1;
}


static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  int ret,ret2,f,track = 0;
  char *filename = NULL, *colon = NULL;

  if(mode != STREAM_READ || !p->filename) {
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }
  filename = strdup(p->filename);
  if(!filename) {
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }
  colon = strstr(filename, ":");
  if(colon) {
    if(strlen(colon)>1)
      track = atoi(colon+1);
    *colon = 0;
  }
  if(!track)
    track = 1;
  
  f = cue_read_cue(filename);
  if(f < 0) {
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }
  cue_vcd_read_toc();
  ret2=cue_vcd_get_track_end(track);
  ret=cue_vcd_seek_to_track(track);
  if(ret<0){ 
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (seek)\n");
    return STREAM_UNSUPPORTED;
  }
  mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail, filename, track, ret, ret2);

  stream->fd = f;
  stream->type = STREAMTYPE_VCDBINCUE;
  stream->sector_size = VCD_SECTOR_DATA;
  stream->flags = STREAM_READ | STREAM_SEEK_FW;
  stream->start_pos = ret;
  stream->end_pos = ret2;
  stream->fill_buffer = cue_vcd_read;
  stream->seek = seek;

  free(filename);
  m_struct_free(&stream_opts,opts);
  return STREAM_OK;
}

const stream_info_t stream_info_cue = {
  "CUE track",
  "cue",
  "Albeu",
  "based on the code from ???",
  open_s,
  { "cue", NULL },
  &stream_opts,
  1 // Urls are an option string
};

