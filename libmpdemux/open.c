
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef __FreeBSD__
#include <sys/cdrio.h>
#endif

#include "../m_config.h"
#include "stream.h"
#include "demuxer.h"
#include "mf.h"

#ifdef STREAMING
#include "url.h"
#include "network.h"
extern int streaming_start( stream_t *stream, int *demuxer_type, URL_t *url);
#ifdef STREAMING_LIVE_DOT_COM
#include "demux_rtp.h"
#endif
static URL_t* url;
#endif

int dvbin_param_on=0;

/// We keep these 2 for the gui atm, but they will be removed.
int dvd_title=0;
int vcd_track=0;

int dvd_chapter=1;
int dvd_last_chapter=0;
int dvd_angle=1;
char* dvd_device=NULL;
char* cdrom_device=NULL;

#ifdef USE_DVDNAV
#include "dvdnav_stream.h"
#endif

#ifdef USE_DVDREAD

#define	DVDREAD_VERSION(maj,min,micro)	((maj)*10000 + (min)*100 + (micro))

/*
 * Try to autodetect the libdvd-0.9.0 library
 * (0.9.0 removed the <dvdread/dvd_udf.h> header, and moved the two defines
 * DVD_VIDEO_LB_LEN and MAX_UDF_FILE_NAME_LEN from it to
 * <dvdread/dvd_reader.h>)
 */
#if defined(DVD_VIDEO_LB_LEN) && defined(MAX_UDF_FILE_NAME_LEN)
#define	LIBDVDREAD_VERSION	DVDREAD_VERSION(0,9,0)
#else
#define	LIBDVDREAD_VERSION	DVDREAD_VERSION(0,8,0)
#endif

char * dvd_audio_stream_types[8] =
        { "ac3","unknown","mpeg1","mpeg2ext","lpcm","unknown","dts" };

char * dvd_audio_stream_channels[6] =
	{ "unknown", "stereo", "unknown", "unknown", "unknown", "5.1" };
#endif

extern int vcd_get_track_end(int fd,int track);

#include "cue_read.h"

#ifdef USE_TV
#include "tv.h"
extern char* tv_param_channel;
#endif

#ifdef HAS_DVBIN_SUPPORT
#include "dvbin.h"
#endif



#ifdef HAVE_CDDA
stream_t* open_cdda(char* dev,char* track);
#ifdef STREAMING
stream_t* cddb_open(char* dev,char* track);
#endif
#endif

// Define function about auth the libsmbclient library
// FIXME: I really do not not is this function is properly working

#ifdef LIBSMBCLIENT

#include "libsmbclient.h"

static char smb_password[15];
static char smb_username[15];

static void smb_auth_fn(const char *server, const char *share,
             char *workgroup, int wgmaxlen, char *username, int unmaxlen,
	     char *password, int pwmaxlen)
{
  char temp[128];
  
  strcpy(temp, "LAN");				  
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
					
  if (temp[0]) strncpy(workgroup, temp, wgmaxlen - 1);
					   
  strcpy(temp, smb_username); 
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
						    
  if (temp[0]) strncpy(username, temp, unmaxlen - 1);
						      
  strcpy(temp, smb_password); 
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
								
   if (temp[0]) strncpy(password, temp, pwmaxlen - 1);
}
								  

#endif

// Open a new stream  (stdin/file/vcd/url)

stream_t* open_stream(char* filename,char** options, int* file_format){
stream_t* stream=NULL;
int f=-1;
off_t len;
#ifdef __FreeBSD__
int bsize = VCD_SECTOR_SIZE;
#endif
*file_format = DEMUXER_TYPE_UNKNOWN;
if(!filename) {
   mp_msg(MSGT_OPEN,MSGL_ERR,"NULL filename, report this bug\n");
   return NULL;
}

#ifdef HAVE_CDDA
if(filename && strncmp("cdda://",filename,7) == 0) {
  *file_format = DEMUXER_TYPE_RAWAUDIO;
  return open_cdda(cdrom_device ? cdrom_device : DEFAULT_CDROM_DEVICE,filename+7);
}
#ifdef STREAMING
if(filename && strncmp("cddb://",filename,7) == 0) {
  *file_format = DEMUXER_TYPE_RAWAUDIO;
  return cddb_open(cdrom_device ? cdrom_device : DEFAULT_CDROM_DEVICE,filename+7);
}
#endif
#endif

//============ Open VideoCD track ==============
#ifdef HAVE_VCD
if(strncmp("vcd://",filename,6) == 0){
  int ret,ret2;
  if(!cdrom_device) cdrom_device=strdup(DEFAULT_CDROM_DEVICE);
  f=open(cdrom_device,O_RDONLY);
  if(f<0){ mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CdDevNotfound,cdrom_device);return NULL; }
  vcd_track = filename[6] == '\0' ? 1 : strtol(filename+6,NULL,0);
  if(vcd_track < 1){ 
    mp_msg(MSGT_OPEN,MSGL_ERR,"Invalid vcd track %s\n",filename+6);
    return NULL;
  }
  vcd_read_toc(f);
  ret2=vcd_get_track_end(f,vcd_track);
  if(ret2<0){ mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (get)\n");return NULL;}
  ret=vcd_seek_to_track(f,vcd_track);
  if(ret<0){ mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (seek)\n");return NULL;}
  mp_msg(MSGT_OPEN,MSGL_V,"VCD start byte position: 0x%X  end: 0x%X\n",ret,ret2);
#ifdef __FreeBSD__
  if (ioctl (f, CDRIOCSETBLOCKSIZE, &bsize) == -1) {
        perror ( "Error in CDRIOCSETBLOCKSIZE");
  }
#endif
  stream=new_stream(f,STREAMTYPE_VCD);
  stream->start_pos=ret;
  stream->end_pos=ret2;
  return stream;
}
#endif


// for opening of vcds in bincue files
if(strncmp("cue://",filename,6) == 0){
  int ret,ret2;
  char* p = filename + 6;
  vcd_track = 1;
  p = strchr(p,':');
  if(p && p[1] != '\0') {
    vcd_track = strtol(p+1,NULL,0);
    if(vcd_track < 1){ 
      mp_msg(MSGT_OPEN,MSGL_ERR,"Invalid cue track %s\n",p+1);
      return NULL;
    }
    p[0] = '\0';
  }
  f = cue_read_cue (filename + 6);
  if(p && p[1] != '\0') p[0] = ':';
  if (f == -1) return NULL;
  cue_vcd_read_toc();
  ret2=cue_vcd_get_track_end(vcd_track);
  ret=cue_vcd_seek_to_track(vcd_track);
  if(ret<0){ mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (seek)\n");return NULL;}
  mp_msg(MSGT_OPEN,MSGL_V,"VCD start byte position: 0x%X  end: 0x%X\n",ret,ret2);

  stream=new_stream(f,STREAMTYPE_VCDBINCUE);
  stream->start_pos=ret;
  stream->end_pos=ret2;
  printf ("start:%d end:%d\n", ret, ret2);
  return stream;
}


//============ Open DVD title ==============
#ifdef USE_DVDNAV
if(strncmp("dvdnav://",filename,9) == 0){
    dvdnav_priv_t *dvdnav_priv;
    int event,len,tmplen=0;
    char* name = (filename[9] == '\0') ? NULL : filename + 9;
    
    stream=new_stream(-1,STREAMTYPE_DVDNAV);
    if (!stream) {
        mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_Exit_error);
        return NULL;
    }

    if(!name) name=DEFAULT_DVD_DEVICE;
    if (!(dvdnav_priv=new_dvdnav_stream(name))) {
	mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,name);
        return NULL;
    }

    stream->priv=(void*)dvdnav_priv;
    return stream;
}
#endif
#ifdef USE_DVDREAD
if(strncmp("dvd://",filename,6) == 0){
//  int ret,ret2;
    dvd_priv_t *d;
    int ttn,pgc_id,pgn;
    dvd_reader_t *dvd;
    dvd_file_t *title;
    ifo_handle_t *vmg_file;
    tt_srpt_t *tt_srpt;
    ifo_handle_t *vts_file;
    dvd_title = filename[6] == '\0' ? 1 : strtol(filename + 6,NULL,0);
    /**
     * Open the disc.
     */
    if(!dvd_device) dvd_device=strdup(DEFAULT_DVD_DEVICE);
#ifdef SYS_DARWIN
    /* Dynamic DVD drive selection on Darwin */
    if (!strcmp(dvd_device, "/dev/rdiskN")) {
	int i;
	char *temp_device = malloc((strlen(dvd_device)+1)*sizeof(char));
	
	for (i = 1; i < 10; i++) {
	    sprintf(temp_device, "/dev/rdisk%d", i);
	    dvd = DVDOpen(temp_device);
	    if (!dvd) {
	        mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,temp_device);
	    } else {
	        free(temp_device);
	        break;
	    }
	}
	
	if (!dvd)
	    return NULL;
    } else
#endif /* SYS_DARWIN */
    {
        dvd = DVDOpen(dvd_device);
	if( !dvd ) {
	    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,dvd_device);
	    return NULL;
	}
    }

    mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_DVDwait);

    /**
     * Load the video manager to find out the information about the titles on
     * this disc.
     */
    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
        mp_msg(MSGT_OPEN,MSGL_ERR, "Can't open VMG info!\n");
        DVDClose( dvd );
        return NULL;
    }
    tt_srpt = vmg_file->tt_srpt;
    /**
     * Make sure our title number is valid.
     */
    mp_msg(MSGT_OPEN,MSGL_INFO, MSGTR_DVDnumTitles,
             tt_srpt->nr_of_srpts );
    if( dvd_title < 1 || dvd_title > tt_srpt->nr_of_srpts ) {
	mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDinvalidTitle, dvd_title);
        ifoClose( vmg_file );
        DVDClose( dvd );
        return NULL;
    }
    --dvd_title; // remap 1.. -> 0..
    /**
     * Make sure the chapter number is valid for this title.
     */
    mp_msg(MSGT_OPEN,MSGL_INFO, MSGTR_DVDnumChapters,
             tt_srpt->title[dvd_title].nr_of_ptts );
    if( dvd_chapter<1 || dvd_chapter>tt_srpt->title[dvd_title].nr_of_ptts ) {
	mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDinvalidChapter, dvd_chapter);
        ifoClose( vmg_file );
        DVDClose( dvd );
        return NULL;
    }
    if( dvd_last_chapter>0 ) {
	if ( dvd_last_chapter<dvd_chapter || dvd_last_chapter>tt_srpt->title[dvd_title].nr_of_ptts ) {
	    mp_msg(MSGT_OPEN,MSGL_ERR, "Invalid DVD last chapter number: %d\n", dvd_last_chapter);
	    ifoClose( vmg_file );
	    DVDClose( dvd );
	    return NULL;
	}
    }
    --dvd_chapter; // remap 1.. -> 0..
    /* XXX No need to remap dvd_last_chapter */
    /**
     * Make sure the angle number is valid for this title.
     */
    mp_msg(MSGT_OPEN,MSGL_INFO, MSGTR_DVDnumAngles,
             tt_srpt->title[dvd_title].nr_of_angles );
    if( dvd_angle<1 || dvd_angle>tt_srpt->title[dvd_title].nr_of_angles ) {
	mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDinvalidAngle, dvd_angle);
        ifoClose( vmg_file );
        DVDClose( dvd );
        return NULL;
    }
    --dvd_angle; // remap 1.. -> 0..
    /**
     * Load the VTS information for the title set our title is in.
     */
    vts_file = ifoOpen( dvd, tt_srpt->title[dvd_title].title_set_nr );
    if( !vts_file ) {
	mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDnoIFO,
                 tt_srpt->title[dvd_title].title_set_nr );
        ifoClose( vmg_file );
        DVDClose( dvd );
        return NULL;
    }
    /**
     * We've got enough info, time to open the title set data.
     */
    title = DVDOpenFile( dvd, tt_srpt->title[dvd_title].title_set_nr,
                         DVD_READ_TITLE_VOBS );
    if( !title ) {
	mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDnoVOBs,
                 tt_srpt->title[dvd_title].title_set_nr );
        ifoClose( vts_file );
        ifoClose( vmg_file );
        DVDClose( dvd );
        return NULL;
    }

    mp_msg(MSGT_OPEN,MSGL_INFO, MSGTR_DVDopenOk);
    // store data
    d=malloc(sizeof(dvd_priv_t)); memset(d,0,sizeof(dvd_priv_t));
    d->dvd=dvd;
    d->title=title;
    d->vmg_file=vmg_file;
    d->tt_srpt=tt_srpt;
    d->vts_file=vts_file;

    /**
     * Check number of audio channels and types
     */
    {
     int ac3aid = 128;
     int mpegaid = 0;
     int pcmaid = 160;
     
     d->nr_of_channels=0;
     
     if ( vts_file->vts_pgcit ) 
      {
       int i;
       for ( i=0;i<8;i++ )
        if ( vts_file->vts_pgcit->pgci_srp[0].pgc->audio_control[i] & 0x8000 )
	 {
	  audio_attr_t * audio = &vts_file->vtsi_mat->vts_audio_attr[i];
	  int language = 0;
	  char tmp[] = "unknown";
	  
	  if ( audio->lang_type == 1 ) 
	   {
	    language=audio->lang_code;
	    tmp[0]=language>>8;
	    tmp[1]=language&0xff;
	    tmp[2]=0;
	   }
	  
          d->audio_streams[d->nr_of_channels].language=language;
          d->audio_streams[d->nr_of_channels].id=0;
	  switch ( audio->audio_format )
	   {
	    case 0: // ac3
	    case 6: // dts
	            d->audio_streams[d->nr_of_channels].id=ac3aid;
		    ac3aid++;
		    break;
	    case 2: // mpeg layer 1/2/3
	    case 3: // mpeg2 ext
	            d->audio_streams[d->nr_of_channels].id=mpegaid;
		    mpegaid++;
		    break;
	    case 4: // lpcm
	            d->audio_streams[d->nr_of_channels].id=pcmaid;
		    pcmaid++;
		    break;
	   }

	  d->audio_streams[d->nr_of_channels].type=audio->audio_format;
	  // Pontscho: to my mind, tha channels:
	  //  1 - stereo
	  //  5 - 5.1
	  d->audio_streams[d->nr_of_channels].channels=audio->channels;
          mp_msg(MSGT_OPEN,MSGL_V,"[open] audio stream: %d audio format: %s (%s) language: %s aid: %d\n",
	    d->nr_of_channels,
            dvd_audio_stream_types[ audio->audio_format ],
	    dvd_audio_stream_channels[ audio->channels ],
	    tmp,
	    d->audio_streams[d->nr_of_channels].id
	    );
	    
	  d->nr_of_channels++;
	 }
      }
     mp_msg(MSGT_OPEN,MSGL_V,"[open] number of audio channels on disk: %d.\n",d->nr_of_channels );
    }

    /**
     * Check number of subtitles and language
     */
    {
     int i;

     d->nr_of_subtitles=0;
     for ( i=0;i<32;i++ )
      if ( vts_file->vts_pgcit->pgci_srp[0].pgc->subp_control[i] & 0x80000000 )
       {
        subp_attr_t * subtitle = &vts_file->vtsi_mat->vts_subp_attr[i];
	int language = 0;
	char tmp[] = "unknown";
	
	if ( subtitle->type == 1 )
	 {
	  language=subtitle->lang_code;
	  tmp[0]=language>>8;
	  tmp[1]=language&0xff;
	  tmp[2]=0;
	 }
	 
	d->subtitles[ d->nr_of_subtitles ].language=language;
	d->subtitles[ d->nr_of_subtitles ].id=d->nr_of_subtitles;
	
        mp_msg(MSGT_OPEN,MSGL_V,"[open] subtitle ( sid ): %d language: %s\n",
	  d->nr_of_subtitles,
	  tmp
	  );
        d->nr_of_subtitles++;
       }
     mp_msg(MSGT_OPEN,MSGL_V,"[open] number of subtitles on disk: %d\n",d->nr_of_subtitles );
    }

    /**
     * Determine which program chain we want to watch.  This is based on the
     * chapter number.
     */
    ttn = tt_srpt->title[ dvd_title ].vts_ttn; // local
    pgc_id = vts_file->vts_ptt_srpt->title[ttn-1].ptt[dvd_chapter].pgcn; // local
    pgn    = vts_file->vts_ptt_srpt->title[ttn-1].ptt[dvd_chapter].pgn;  // local
    d->cur_pgc = vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    d->cur_cell = d->cur_pgc->program_map[pgn-1] - 1; // start playback here
    d->packs_left=-1;      // for Navi stuff
    d->angle_seek=0;
    /* XXX dvd_last_chapter is in the range 1..nr_of_ptts */
    if ( dvd_last_chapter > 0 && dvd_last_chapter < tt_srpt->title[dvd_title].nr_of_ptts ) {
	pgn=vts_file->vts_ptt_srpt->title[ttn-1].ptt[dvd_last_chapter].pgn;
	d->last_cell=d->cur_pgc->program_map[pgn-1] - 1;
    }
    else
	d->last_cell=d->cur_pgc->nr_of_cells;
    
    if( d->cur_pgc->cell_playback[d->cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle;
    d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
    mp_msg(MSGT_DVD,MSGL_V, "DVD start cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);

    // ... (unimplemented)
//    return NULL;
  stream=new_stream(-1,STREAMTYPE_DVD);
  stream->start_pos=(off_t)d->cur_pack*2048;
  stream->end_pos=(off_t)(d->cur_pgc->cell_playback[d->last_cell-1].last_sector)*2048;
  mp_msg(MSGT_DVD,MSGL_V,"DVD start=%d end=%d  \n",d->cur_pack,d->cur_pgc->cell_playback[d->last_cell-1].last_sector);
  stream->priv=(void*)d;
  return stream;
}
#endif

#ifdef HAS_DVBIN_SUPPORT
if(strncmp("dvbin://",filename,8) == 0)
{
	stream = new_stream(-1, STREAMTYPE_DVB);
	if (!stream)
	   return(NULL);
       	if (!dvb_streaming_start(stream))
	   return NULL;

       	return stream;
}
#endif




//============ Check for TV-input or multi-file input ====
  if( (strncmp("mf://",filename,5) == 0)
#ifdef USE_TV
   || (strncmp("tv://",filename,5) == 0)
#endif
  ){
    /* create stream */
    stream = new_stream(-1, STREAMTYPE_DUMMY);
    if (!stream) return(NULL);
    if(strncmp("mf://",filename,5) == 0) {
      *file_format =  DEMUXER_TYPE_MF;
#ifdef USE_TV
     } else {
      *file_format =  DEMUXER_TYPE_TV;
      if(filename[5] != '\0')
	tv_param_channel = strdup(filename + 5);
#endif
    }
    stream->url= filename[5] != '\0' ? strdup(filename + 5) : NULL;
    return(stream);
  }
  
#ifdef STREAMING
  url = url_new(filename);
  if(url) {
	if (strcmp(url->protocol, "smb")==0){
#ifdef LIBSMBCLIENT
	    
	    // we need init of libsmbclient
            int err;
	    
	    // FIXME: HACK: make the username/password global varaibles
	    // so the auth_fn function should grab it ...
	    // i cannot thing other way...
	    err = smbc_init(smb_auth_fn, 10);  	/* Initialize things */
	                        	        // libsmbclient using				
	    if (err < 0) {
        	mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBInitError,err);
	    	return NULL;
	    }
	    f=smbc_open(filename, O_RDONLY, 0666);    
	    
	    // cannot open the file, outputs that
	    // MSGTR_FileNotFound -> MSGTR_SMBFileNotFound
    	    if(f<0){ 
		mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBFileNotFound,filename);
		return NULL; 
	    }
	    len=smbc_lseek(f,0,SEEK_END); 
	    smbc_lseek(f,0,SEEK_SET);
	    // FIXME: I really wonder is such situation -> but who cares ;)
//	    if (len == -1)	
//    		   return new_stream(f,STREAMTYPE_STREAM); // open as stream
	    url_free(url);
            stream=new_stream(f,STREAMTYPE_SMB);
    	    stream->end_pos=len;
	    return stream;
#else
	    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBNotCompiled,filename);
	    return NULL;
#endif
	}
        stream=new_stream(f,STREAMTYPE_STREAM);
	if( streaming_start( stream, file_format, url )<0){
          mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_UnableOpenURL, filename);
	  //url_free(url);
	  //return NULL;
	} else {
        mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_ConnToServer, url->hostname );
	url_free(url);
	return stream;
  }
  }

//============ Open STDIN or plain FILE ============
#ifdef STREAMING_LIVE_DOT_COM
  //  a SDP file: I hope the sdp protocol isn't really in use
  if(strncmp("sdp://",filename,6) == 0) {
       filename += 6;
#if defined(__CYGWIN__) || defined(__MINGW32__)
       f=open(filename,O_RDONLY|O_BINARY);
#else
       f=open(filename,O_RDONLY);
#endif
       if(f<0){ mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_FileNotFound,filename);return NULL; }

       len=lseek(f,0,SEEK_END); lseek(f,0,SEEK_SET);
       if (len == -1)
           return NULL;

#ifdef _LARGEFILE_SOURCE
	 mp_msg(MSGT_OPEN,MSGL_V,"File size is %lld bytes\n", (long long)len);
#else
	 mp_msg(MSGT_OPEN,MSGL_V,"File size is %u bytes\n", (unsigned int)len);
#endif
	 return stream_open_sdp(f, len, file_format);
  }
#endif
#endif

  return open_stream_full(filename,STREAM_READ,options,file_format);
}

int dvd_parse_chapter_range(struct config *conf, const char *range){
  const char *s;
  char *t;
/*  conf; prevent warning from GCC */
  s = range;
  dvd_chapter = 1;
  dvd_last_chapter = 0;
  if (*range && isdigit(*range)) {
    dvd_chapter = strtol(range, &s, 10);
    if (range == s) {
      mp_msg(MSGT_OPEN, MSGL_ERR, "Invalid chapter range specification %s\n", range);
      return -1;
    }
  }
  if (*s == 0)
    return 0;
  else if (*s != '-') {
    mp_msg(MSGT_OPEN, MSGL_ERR, "Invalid chapter range specification %s\n", range);
    return -1;
  }
  ++s;
  if (*s == 0)
      return 0;
  if (! isdigit(*s)) {
    mp_msg(MSGT_OPEN, MSGL_ERR, "Invalid chapter range specification %s\n", range);
    return -1;
  }
  dvd_last_chapter = strtol(s, &t, 10);
  if (s == t || *t)  {
    mp_msg(MSGT_OPEN, MSGL_ERR, "Invalid chapter range specification %s\n", range);
    return -1;
  }
  return 0;
}

#ifdef USE_DVDREAD
int dvd_chapter_from_cell(dvd_priv_t* dvd,int title,int cell)
{
  pgc_t * cur_pgc;
  ptt_info_t* ptt;
  int chapter = cell;
  int pgc_id,pgn;
  if(title < 0 || cell < 0){
    return 0;
  }
  /* for most DVD's chapter == cell */
  /* but there are more complecated cases... */
  if(chapter >= dvd->vmg_file->tt_srpt->title[title].nr_of_ptts){
    chapter = dvd->vmg_file->tt_srpt->title[title].nr_of_ptts-1;
  }
  title = dvd->tt_srpt->title[title].vts_ttn-1;
  ptt = dvd->vts_file->vts_ptt_srpt->title[title].ptt;
  while(chapter >= 0){
    pgc_id = ptt[chapter].pgcn;
    pgn = ptt[chapter].pgn;
    cur_pgc = dvd->vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    if(cell >= cur_pgc->program_map[pgn-1]-1){
      return chapter;
    }
    --chapter;
  }
  /* didn't find a chapter ??? */
  return chapter;
}

int dvd_aid_from_lang(stream_t *stream, unsigned char* lang){
dvd_priv_t *d=stream->priv;
int code,i;
  while(lang && strlen(lang)>=2){
    code=lang[1]|(lang[0]<<8);
    for(i=0;i<d->nr_of_channels;i++){
	if(d->audio_streams[i].language==code){
	    mp_msg(MSGT_OPEN,MSGL_INFO,"Selected DVD audio channel: %d language: %c%c\n",
		d->audio_streams[i].id, lang[0],lang[1]);
	    return d->audio_streams[i].id;
	}
//	printf("%X != %X  (%c%c)\n",code,d->audio_streams[i].language,lang[0],lang[1]);
    }
    lang+=2; while (lang[0]==',' || lang[0]==' ') ++lang;
  }
  mp_msg(MSGT_OPEN,MSGL_WARN,"No matching DVD audio language found!\n");
  return -1;
}

int dvd_sid_from_lang(stream_t *stream, unsigned char* lang){
dvd_priv_t *d=stream->priv;
int code,i;
  while(lang && strlen(lang)>=2){
    code=lang[1]|(lang[0]<<8);
    for(i=0;i<d->nr_of_subtitles;i++){
	if(d->subtitles[i].language==code){
	    mp_msg(MSGT_OPEN,MSGL_INFO,"Selected DVD subtitle channel: %d language: %c%c\n",
		d->subtitles[i].id, lang[0],lang[1]);
	    return d->subtitles[i].id;
	}
    }
    lang+=2; while (lang[0]==',' || lang[0]==' ') ++lang;
  }
  mp_msg(MSGT_OPEN,MSGL_WARN,"No matching DVD subtitle language found!\n");
  return -1;
}

static int dvd_next_cell(dvd_priv_t *d){
    int next_cell=d->cur_cell;

    mp_msg(MSGT_DVD,MSGL_V, "dvd_next_cell: next1=0x%X  \n",next_cell);
    
    if( d->cur_pgc->cell_playback[ next_cell ].block_type
                                        == BLOCK_TYPE_ANGLE_BLOCK ) {
	    while(next_cell<d->last_cell){
                if( d->cur_pgc->cell_playback[next_cell].block_mode
                                          == BLOCK_MODE_LAST_CELL ) break;
		++next_cell;
            }
    }
    mp_msg(MSGT_DVD,MSGL_V, "dvd_next_cell: next2=0x%X  \n",next_cell);
    
    ++next_cell;
    if(next_cell>=d->last_cell) return -1; // EOF
    if( d->cur_pgc->cell_playback[next_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ){
	next_cell+=dvd_angle;
	if(next_cell>=d->last_cell) return -1; // EOF
    }
    mp_msg(MSGT_DVD,MSGL_V, "dvd_next_cell: next3=0x%X  \n",next_cell);
    return next_cell;
}

int dvd_read_sector(dvd_priv_t *d,unsigned char* data){
    int len;
    
    if(d->packs_left==0){
            /**
             * If we're not at the end of this cell, we can determine the next
             * VOBU to display using the VOBU_SRI information section of the
             * DSI.  Using this value correctly follows the current angle,
             * avoiding the doubled scenes in The Matrix, and makes our life
             * really happy.
             *
             * Otherwise, we set our next address past the end of this cell to
             * force the code above to go to the next cell in the program.
             */
            if( d->dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL ) {
                d->cur_pack= d->dsi_pack.dsi_gi.nv_pck_lbn +
		( d->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
		mp_msg(MSGT_DVD,MSGL_DBG2, "Navi  new pos=0x%X  \n",d->cur_pack);
            } else {
		// end of cell! find next cell!
		mp_msg(MSGT_DVD,MSGL_V, "--- END OF CELL !!! ---\n");
		d->cur_pack=d->cell_last_pack+1;
            }
    }

read_next:

    if(d->cur_pack>d->cell_last_pack){
	// end of cell!
	int next=dvd_next_cell(d);
	if(next>=0){
	    d->cur_cell=next;

//    if( d->cur_pgc->cell_playback[d->cur_cell].block_type 
//	== BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle;
    d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
    mp_msg(MSGT_DVD,MSGL_V, "DVD next cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);
	    
	} else return -1; // EOF
    }

    len = DVDReadBlocks( d->title, d->cur_pack, 1, data );
    if(!len) return -1; //error
    
    if(data[38]==0 && data[39]==0 && data[40]==1 && data[41]==0xBF &&
       data[1024]==0 && data[1025]==0 && data[1026]==1 && data[1027]==0xBF){
	// found a Navi packet!!!
#if LIBDVDREAD_VERSION >= DVDREAD_VERSION(0,9,0)
        navRead_DSI( &d->dsi_pack, &(data[ DSI_START_BYTE ]) );
#else
        navRead_DSI( &d->dsi_pack, &(data[ DSI_START_BYTE ]), sizeof(dsi_t) );
#endif
	if(d->cur_pack != d->dsi_pack.dsi_gi.nv_pck_lbn ){
	    mp_msg(MSGT_DVD,MSGL_V, "Invalid NAVI packet! lba=0x%X  navi=0x%X  \n",
		d->cur_pack,d->dsi_pack.dsi_gi.nv_pck_lbn);
	} else {
	    // process!
    	    d->packs_left = d->dsi_pack.dsi_gi.vobu_ea;
	    mp_msg(MSGT_DVD,MSGL_DBG2, "Found NAVI packet! lba=0x%X  len=%d  \n",d->cur_pack,d->packs_left);
	    //navPrint_DSI(&d->dsi_pack);
	    mp_msg(MSGT_DVD,MSGL_DBG3,"\r### CELL %d: Navi: %d/%d  IFO: %d/%d   \n",d->cur_cell,
		d->dsi_pack.dsi_gi.vobu_c_idn,d->dsi_pack.dsi_gi.vobu_vob_idn,
		d->cur_pgc->cell_position[d->cur_cell].cell_nr,
		d->cur_pgc->cell_position[d->cur_cell].vob_id_nr);

	    if(d->angle_seek){
		int i,skip=0;
#if defined(__GNUC__) && defined(__sparc__)
		// workaround for a bug in the sparc version of gcc 2.95.X ... 3.2,
		// it generates incorrect code for unaligned access to a packed
		// structure member, resulting in an mplayer crash with a SIGBUS
		// signal.
		//
		// See also gcc problem report PR c/7847:
		// http://gcc.gnu.org/cgi-bin/gnatsweb.pl?database=gcc&cmd=view+audit-trail&pr=7847
		for(i=0;i<9;i++){	// check if all values zero:
		    typeof(d->dsi_pack.sml_agli.data[i].address) tmp_addr;
		    memcpy(&tmp_addr,&d->dsi_pack.sml_agli.data[i].address,sizeof(tmp_addr));
		    if((skip=tmp_addr)!=0) break;
		}
#else
		for(i=0;i<9;i++)	// check if all values zero:
		    if((skip=d->dsi_pack.sml_agli.data[i].address)!=0) break;
#endif
		if(skip){
		    // sml_agli table has valid data (at least one non-zero):
		    d->cur_pack=d->dsi_pack.dsi_gi.nv_pck_lbn+
			d->dsi_pack.sml_agli.data[dvd_angle].address;
		    d->angle_seek=0;
		    mp_msg(MSGT_DVD,MSGL_V, "Angle-seek synced using sml_agli map!  new_lba=0x%X  \n",d->cur_pack);
		} else {
		    // check if we're in the right cell, jump otherwise:
		    if( (d->dsi_pack.dsi_gi.vobu_c_idn==d->cur_pgc->cell_position[d->cur_cell].cell_nr) &&
		        (d->dsi_pack.dsi_gi.vobu_vob_idn==d->cur_pgc->cell_position[d->cur_cell].vob_id_nr) ){
			d->angle_seek=0;
			mp_msg(MSGT_DVD,MSGL_V, "Angle-seek synced by cell/vob IDN search!  \n");
		    } else {
			// wrong angle, skip this vobu:
			d->cur_pack=d->dsi_pack.dsi_gi.nv_pck_lbn+
			    d->dsi_pack.dsi_gi.vobu_ea;
			d->angle_seek=2; // DEBUG
		    }
		}
	    }
	}
	++d->cur_pack;
	goto read_next;
    }
    
    ++d->cur_pack;
    if(d->packs_left>=0) --d->packs_left;
    
    if(d->angle_seek){
	if(d->angle_seek==2) mp_msg(MSGT_DVD,MSGL_V, "!!! warning! reading packet while angle_seek !!!\n");
	goto read_next; // searching for Navi packet
    }

    return d->cur_pack-1;
}

void dvd_seek(dvd_priv_t *d,int pos){
    d->packs_left=-1;
    d->cur_pack=pos;
    
// check if we stay in current cell (speedup things, and avoid angle skip)
if(d->cur_pack>d->cell_last_pack ||
   d->cur_pack<d->cur_pgc->cell_playback[ d->cur_cell ].first_sector){

    // ok, cell change, find the right cell!
    d->cur_cell=0;
    if( d->cur_pgc->cell_playback[d->cur_cell].block_type 
	== BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle;

  while(1){
    int next;
    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
    if(d->cur_pack<d->cur_pgc->cell_playback[ d->cur_cell ].first_sector){
	d->cur_pack=d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
	break;
    }
    if(d->cur_pack<=d->cell_last_pack) break; // ok, we find it! :)
    next=dvd_next_cell(d);
    if(next<0){
//	d->cur_pack=d->cell_last_pack+1;
	break; // we're after the last cell
    }
    d->cur_cell=next;
  }

}

mp_msg(MSGT_DVD,MSGL_V, "DVD Seek! lba=0x%X  cell=%d  packs: 0x%X-0x%X  \n",
    d->cur_pack,d->cur_cell,d->cur_pgc->cell_playback[ d->cur_cell ].first_sector,d->cell_last_pack);

// if we're in interleaved multi-angle cell, find the right angle chain!
// (read Navi block, and use the seamless angle jump table)
d->angle_seek=1;

}

void dvd_close(dvd_priv_t *d) {
  ifoClose(d->vts_file);
  ifoClose(d->vmg_file);
  DVDCloseFile(d->title);
  DVDClose(d->dvd);
  dvd_chapter = 1;
  dvd_last_chapter = 0;
}

#endif
