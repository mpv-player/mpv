#ifndef __CDD_H__
#define __CDD_H__

#include <cdda_interface.h>
#include <cdda_paranoia.h>

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
	cdrom_drive* cd;
	cdrom_paranoia* cdp;
	int sector;
	int start_sector;
	int end_sector;
	cd_info_t *cd_info;
} cdda_priv;

cd_info_t* 	cd_info_new();
void		cd_info_free(cd_info_t *cd_info);
cd_track_t*	cd_info_add_track(cd_info_t *cd_info, char *track_name, unsigned int track_nb, unsigned int min, unsigned int sec, unsigned int msec, unsigned long frame_begin, unsigned long frame_length);
cd_track_t*	cd_info_get_track(cd_info_t *cd_info, unsigned int track_nb);


#endif // __CDD_H__
