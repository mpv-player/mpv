
// based on libmpeg2/header.c by Aaron Holtzman <aholtzma@ess.engr.uvic.ca>

// #include <inttypes.h>
#include <stdio.h>

#include "config.h"
#include "mpeg_hdr.h"

static int frameratecode2framerate[16] = {
  0,
  // Official mpeg1/2 framerates:
  24000*10000/1001, 24*10000,25*10000, 30000*10000/1001, 30*10000,50*10000,60000*10000/1001, 60*10000,
  // libmpeg3's "Unofficial economy rates":
  299700,5*10000,10*10000,12*10000,15*10000,0,0
};


int mp_header_process_sequence_header (mp_mpeg_header_t * picture, unsigned char * buffer)
{
    int width, height;

    if ((buffer[6] & 0x20) != 0x20){
	printf("missing marker bit!\n");
	return 1;	/* missing marker_bit */
    }

    height = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];

    picture->display_picture_width = (height >> 12);
    picture->display_picture_height = (height & 0xfff);

    width = ((height >> 12) + 15) & ~15;
    height = ((height & 0xfff) + 15) & ~15;

    if ((width > 768) || (height > 576)){
	printf("size restrictions for MP@ML or MPEG1 exceeded! (%dx%d)\n",width,height);
//	return 1;	/* size restrictions for MP@ML or MPEG1 */
    }
    
    picture->aspect_ratio_information = buffer[3] >> 4;
    picture->frame_rate_code = buffer[3] & 15;
    picture->fps=frameratecode2framerate[picture->frame_rate_code];
    picture->bitrate = (buffer[4]<<10)|(buffer[5]<<2)|(buffer[6]>>6);
    picture->mpeg1 = 1;
    picture->picture_structure = 3; //FRAME_PICTURE;
    picture->display_time=100;
    return 0;
}

static int header_process_sequence_extension (mp_mpeg_header_t * picture,
					      unsigned char * buffer)
{
    /* check chroma format, size extensions, marker bit */
    if (((buffer[1] & 0x07) != 0x02) || (buffer[2] & 0xe0) ||
	((buffer[3] & 0x01) != 0x01))
	return 1;

    picture->progressive_sequence = (buffer[1] >> 3) & 1;
    picture->mpeg1 = 0;
    return 0;
}

static int header_process_picture_coding_extension (mp_mpeg_header_t * picture, unsigned char * buffer)
{
    picture->picture_structure = buffer[2] & 3;
    picture->top_field_first = buffer[3] >> 7;
    picture->repeat_first_field = (buffer[3] >> 1) & 1;
    picture->progressive_frame = buffer[4] >> 7;

    // repeat_first implementation by A'rpi/ESP-team, based on libmpeg3:
    picture->display_time=100;
    if(picture->repeat_first_field){
        if(picture->progressive_sequence){
            if(picture->top_field_first)
                picture->display_time+=200;
            else
                picture->display_time+=100;
        } else
        if(picture->progressive_frame){
                picture->display_time+=50;
        }
    }
    //temopral hack. We calc time on every field, so if we have 2 fields
    // interlaced we'll end with double time for 1 frame
    if( picture->picture_structure!=3 ) picture->display_time/=2;
    return 0;
}

int mp_header_process_extension (mp_mpeg_header_t * picture, unsigned char * buffer)
{
    switch (buffer[0] & 0xf0) {
    case 0x10:	/* sequence extension */
	return header_process_sequence_extension (picture, buffer);
    case 0x80:	/* picture coding extension */
	return header_process_picture_coding_extension (picture, buffer);
    }
    return 0;
}

