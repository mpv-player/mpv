//=================== VideoCD BinCue ==========================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>


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

/* presumes Line is preloaded with the "current" line of the file */
int cue_getTrackinfo(char *Line, tTrack *track)
{
  char inum[3];
  char min;
  char sec;
  char fps;
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
  else return(1);

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
      strncpy(inum, &Line[10], 2); inum[2] = '\0';
      if ((already_set == 0) &&
          ((strcmp(inum, "00")==0) || (strcmp(inum, "01")==0)))
      {
        already_set = 1;

        min = ((Line[13]-'0')<<4) | (Line[14]-'0');
        sec = ((Line[16]-'0')<<4) | (Line[17]-'0');
        fps = ((Line[19]-'0')<<4) | (Line[20]-'0');

        track->minute = (((min>>4)*10) + (min&0xf));
        track->second = (((sec>>4)*10) + (sec&0xf));
        track->frame  = (((fps>>4)*10) + (fps&0xf));
      }
    }
    else if (strncmp(&Line[4], "PREGAP ", 7)==0) { ; /* ignore */ }
    else if (strncmp(&Line[4], "FLAGS ", 6)==0)  { ; /* ignore */ }
    else mp_msg (MSGT_OPEN,MSGL_INFO,
                 "[bincue] Unexpected cuefile line: %s\n", Line);
  }
  return(0);
}



int cue_find_bin (char *firstline) {
  int i,j;
  char s[256];
  char t[256];

  /* get the filename out of that */
  /*                      12345 6  */
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
    mp_msg(MSGT_OPEN,MSGL_STATUS, "[bincue] bin filename tested: %s\n",
           bin_filename);

    /* now try to find it with the path of the cue file */
    sprintf(s,"%s/%s",bincue_path, bin_filename);
    fd_bin = open (s, O_RDONLY);
    if (fd_bin == -1)
    {
      mp_msg(MSGT_OPEN,MSGL_STATUS,
             "[bincue] bin filename tested: %s\n", s);
      /* now I would say the whole filename is shit, build our own */
      strncpy(s, cue_filename, strlen(cue_filename) - 3 );
      s[strlen(cue_filename) - 3] = '\0';
      strcat(s, "bin");
      fd_bin = open (s, O_RDONLY);
      if (fd_bin == -1)
      {
        mp_msg(MSGT_OPEN,MSGL_STATUS,
               "[bincue] bin filename tested: %s\n", s);

        /* ok try it with path */
        sprintf(t,"%s/%s",bincue_path, s);
        fd_bin = open (t, O_RDONLY);
        if (fd_bin == -1)
        {
          mp_msg(MSGT_OPEN,MSGL_STATUS,
                 "[bincue] bin filename tested: %s\n",t);
          /* now I would say the whole filename is shit, build our own */
          strncpy(s, cue_filename, strlen(cue_filename) - 3 );
          s[strlen(cue_filename) - 3] = '\0';
          strcat(s, "img");
          fd_bin = open (s, O_RDONLY);
          if (fd_bin == -1)
          {
            mp_msg(MSGT_OPEN,MSGL_STATUS,
                   "[bincue] bin filename tested: %s \n", s);
            /* ok try it with path */
            sprintf(t,"%s/%s",bincue_path, s);
            fd_bin = open (t, O_RDONLY);
            if (fd_bin == -1)
            {
              mp_msg(MSGT_OPEN,MSGL_STATUS,
                     "[bincue] bin filename tested: %s\n", s);

              /* I'll give up */
              mp_msg(MSGT_OPEN,MSGL_ERR,
                     "[bincue] couldn't find the bin file - giving up\n");
              return -1;
            }
          }
        } else strcpy(bin_filename, t);

      } else strcpy(bin_filename, s);

    } else strcpy(bin_filename, s);

  }

  mp_msg(MSGT_OPEN,MSGL_INFO,
         "[bincue] using bin file %s\n", bin_filename);
  return 0;
}

static inline int cue_msf_2_sector(int minute, int second, int frame) {
 return frame + (second + minute * 60 ) * 75;
}

static inline int cue_get_msf() {
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
             "[bincue] unknown mode for binfile. should not happen. aborting\n");
      abort();
  }

}


int cue_read_cue (char *in_cue_filename)
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
  t = dirname(s);
  printf ("dirname: %s\n", t);
  strcpy(bincue_path,t);


  /* no path at all? */
  if (strcmp(bincue_path, ".") == 0) {
    printf ("bincue_path: %s\n", bincue_path);
    strcpy(cue_filename,in_cue_filename);
  } else {
    strcpy(cue_filename,in_cue_filename + strlen(bincue_path) + 1);
  }



  /* open the cue file */
  fd_cue = fopen (in_cue_filename, "r");
  if (fd_cue == NULL)
  {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           "[bincue] cannot open %s\n", in_cue_filename);
    return -1;
  }

  /* read the first line and hand it to find_bin, which will
     test more than one possible name of the file */

  if(! fgets( sLine, 256, fd_cue ) )
  {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           "[bincue] error reading from  %s\n", in_cue_filename);
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
           "[bincue] error reading from  %s\n", in_cue_filename);
    fclose (fd_cue);
    return -1;
  }

  while(!feof(fd_cue))
  {
    if (cue_getTrackinfo(sLine, &tracks[nTracks++]) != 0)
    {
      mp_msg(MSGT_OPEN,MSGL_ERR,
             "[bincue] error reading from  %s\n", in_cue_filename);
      fclose (fd_cue);
      return -1;
    }
  }

  /* make a fake track with stands for the Lead out */
  if (fstat (fd_bin, &filestat) == -1) {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           "[bincue] error getting size of bin file\n");
    fclose (fd_cue);
    return -1;
  }

  sect = filestat.st_size / 2352;

  tracks[nTracks].frame = sect%75;
  sect=sect/75;
  tracks[nTracks].second = sect%60;
  sect=sect/60;
  tracks[nTracks].minute = sect;


  /* lets calculate the start sectors and offsets */
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




int cue_read_toc_entry() {

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

int cue_read_raw(char *buf) {
  int position;
  int track = cue_current_pos.track - 1;

  /* get the mode of the bin file part and calc the positon */
  position = tracks[track].start_offset +
             (cue_msf_2_sector(cue_current_pos.minute,
                               cue_current_pos.second,
                               cue_current_pos.frame) -
              tracks[track].start_sector)
             * cue_mode_2_sector_size(tracks[track].mode);

  /* check if the track is at its end*/
  if (position >= tracks[track+1].start_offset)
    return -1;

  if (lseek (fd_bin, position, SEEK_SET) == -1) {
    mp_msg(MSGT_OPEN,MSGL_ERR,
           "[bincue] unexpected end of bin file\n");
    return -1;
  }

  if (!read (fd_bin, buf, VCD_SECTOR_SIZE))
    return -1;
  else
    return VCD_SECTOR_DATA;
}



int cue_vcd_seek_to_track (int track){
  cue_current_pos.track  = track;

  if (cue_read_toc_entry ())
    return -1;

  return VCD_SECTOR_DATA * cue_get_msf();
}

int cue_vcd_get_track_end (int track){
  cue_current_pos.frame = tracks[track].frame;
  cue_current_pos.second = tracks[track].second;
  cue_current_pos.minute = tracks[track].minute;

  return VCD_SECTOR_DATA * cue_get_msf();
}

void cue_vcd_read_toc(){
  int i;
  for (i = 0; i < nTracks; ++i) {

    mp_msg(MSGT_OPEN,MSGL_INFO,
           "track %02d:  format=%d  %02d:%02d:%02d\n",
           i,
           tracks[i].mode,
           tracks[i].minute,
           tracks[i].second,
           tracks[i].frame
           );
  }
}


static char vcd_buf[VCD_SECTOR_SIZE];

static int cue_vcd_read(char *mem){

  if (cue_read_raw(vcd_buf)==-1) return 0; // EOF?

  memcpy(mem,&vcd_buf[VCD_SECTOR_OFFS],VCD_SECTOR_DATA);

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
