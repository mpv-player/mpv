#ifndef __CDD_H__
#define __CDD_H__

#include "config.h"
#ifndef HAVE_LIBCDIO
#include <cdda_interface.h>
#include <cdda_paranoia.h>
#else
#include <cdio/cdda.h>
#include <cdio/paranoia.h>
#endif

typedef struct {
	char cddb_hello[1024];	
	unsigned long disc_id;
	unsigned int tracks;
	char *cache_dir;
	char *freedb_server;
	int freedb_proto_level;
	int anonymous;
	char category[100];
	char *xmcd_file;
	size_t xmcd_file_size;
	void *user_data;
} cddb_data_t;

typedef struct {
	unsigned int min, sec, frame;
} cd_toc_t;

typedef struct cd_track {
	char *name;
	unsigned int track_nb;
	unsigned int min;
	unsigned int sec;
	unsigned int msec;
	unsigned long frame_begin;
	unsigned long frame_length;
	struct cd_track *prev;
	struct cd_track *next;
} cd_track_t;

typedef struct {
	char *artist;
	char *album;
	char *genre;
	unsigned int nb_tracks;
	unsigned int min;
	unsigned int sec;
	unsigned msec;
	cd_track_t *first;
	cd_track_t *last;
	cd_track_t *current;
} cd_info_t;

typedef struct {
#ifndef HAVE_LIBCDIO
	cdrom_drive* cd;
	cdrom_paranoia* cdp;
#else
	cdrom_drive_t* cd;
	cdrom_paranoia_t* cdp;
#endif
	int sector;
	int start_sector;
	int end_sector;
	cd_info_t *cd_info;
} cdda_priv;

cd_info_t* 	cd_info_new();
void		cd_info_free(cd_info_t *cd_info);
cd_track_t*	cd_info_add_track(cd_info_t *cd_info, char *track_name, unsigned int track_nb, unsigned int min, unsigned int sec, unsigned int msec, unsigned long frame_begin, unsigned long frame_length);
cd_track_t*	cd_info_get_track(cd_info_t *cd_info, unsigned int track_nb);

void 		cd_info_debug(cd_info_t *cd_info);

#endif // __CDD_H__
