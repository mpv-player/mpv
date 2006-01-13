/*
 * subpic_encode.c - encodes a pixmap with RLE
 *
 * Copyright (C) 2000   Alejandro J. Cura <alecu@protocultura.net>
 *
 * (modified a bit to work with the dxr3 driver...4/2/2002 cg)
 * 
 * Based on the hard work of:
 *
 *   Samuel Hocevar <sam@via.ecp.fr> and Michel Lespinasse <walken@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "unistd.h"
#include "spuenc.h"

static void
encode_do_control(int x,int y, encodedata* ed, pixbuf* pb) {
	int controlstart= ed->count;
	int x1;
	int i;
	unsigned int top, left, bottom, right;

	top= 450 - pb->y/2;
   	left=(720 / 2) - (pb->x / 2);
	top= 32;//this forces the first bit to be visible on a TV
	left= 32;//you could actually pass in x/y and do some nice
	         //calculations for making it look right...
	bottom= top + pb->y - 1;
	right= left + pb->x - 1;

/* the format of this is well described by a page:
 * http://members.aol.com/mpucoder/DVD/spu.html
 * 
 * note I changed the layout of commands to turn off the subpic as the 
 * first command, and then turn on the new subpic...this is so we can 
 * leave the subpic on for an arbitrary ammount of time as controlled by 
 * mplayer (ie when we turn on the subpic we don't know how long it should
 * stay on when using mplayer).
 * with this layout we turn off the last subpic as we are turning on the 
 * new one.
 * The original hd it turn on the subpic, and delay the turn off command using
 * the durration/delay feature.
 * */
	/* start at x0+2*/
	i= controlstart;
	/* display duration... */
//	ed->data[i++]= 0x00;
//	ed->data[i++]= 0x00; //durration before turn off command occurs
			     //in 90000/1024 units
	
	/* x1 */
//	x1=i+4;
//	ed->data[i++]= x1 >> 8;//location of next command block
//	ed->data[i++]= x1 & 0xff;
	/* finish it */
//	ed->data[i++]= 0x02;//turn off command
//	ed->data[i++]= 0xff;//end of command block
	x1= i; //marker for last command block address 
	
	/* display duration... */
	ed->data[i++]= 0x00;
	ed->data[i++]= 0x00; //durration before turn on command occurs
			     //in 90000/1024 units
	/* x1 */
	ed->data[i++]= x1 >> 8;  //since this is the last command block, this
	ed->data[i++]= x1 & 0xff;//points back to itself


	/* 0x01: start displaying */
	ed->data[i++]= 0x01;

	/* 0x03: palette info */
	ed->data[i++]= 0x03;
	ed->data[i++]= 0x08;
	ed->data[i++]= 0x7f;
/*
 * The palette is a coded index (one of 16) 0 is black, 0xf is white 
 * (unless you screw with the default palette)
 * for what I am doing I only use white. 
 * 7 is lt grey, and 8 is dk grey...
 * */
	/* 0x04: transparency info (reversed) */
	ed->data[i++]= 0x04;
	ed->data[i++]= 0xFF;//change the opacity values of the color entries
	ed->data[i++]= 0xF0;//say if you wanted white text on a black backround
			    //note you will have to work harder, by finding the
	//bounding box of the text, and use a non transparent black palette
	// entry to fill the backround with, (say color 1 instead of 0)

	/* 0x05: coordinates */
	ed->data[i++]= 0x05;
	ed->data[i++]= left >> 4;
	ed->data[i++]= ((left&0xf)<<4)+(right>>8);
	ed->data[i++]= (right&0xff);
	ed->data[i++]= top >> 4;
	ed->data[i++]= ((top&0xf)<<4)+(bottom>>8);
	ed->data[i++]= (bottom&0xff);

	/* 0x06: both fields' offsets */
	ed->data[i++]= 0x06;
	ed->data[i++]= 0x00;
	ed->data[i++]= 0x04;
	ed->data[i++]= ed->oddstart >> 8;
	ed->data[i++]= ed->oddstart & 0xff;

	/* 0xFF: end sequence */
	ed->data[i++]= 0xFF;
	if(! i&1 ) {
		ed->data[i++]= 0xff;
	}

	/* x0 */
	ed->data[2]= (controlstart) >> 8;
	ed->data[3]= (controlstart) & 0xff;
	
	/* packet size */
	ed->data[0]= i >> 8;
	ed->data[1]= i & 0xff;
	
	ed->count= i;
}

static void
encode_put_nibble( encodedata* ed, unsigned char nibble ) {
	if( ed->nibblewaiting ) {
		ed->data[ed->count++]|= nibble;
		ed->nibblewaiting= 0;
	} else {
		ed->data[ed->count]= nibble<<4;
		ed->nibblewaiting= 1;
	}
}

static void
encode_pixels( encodedata* ed, int color, int number ) {
	if(number > 3) {
		if(number > 15) {
			encode_put_nibble( ed, 0 );
			if(number > 63) {
				encode_put_nibble( ed, (number & 0xC0)>>6 );
			}
		}
		encode_put_nibble( ed, (number & 0x3C)>>2 );
	}
	encode_put_nibble( ed, ((number & 0xF)<<2) | color);
}

static void
encode_eol( encodedata* ed ) {
	if( ed->nibblewaiting ) {
		ed->count++;
		ed->nibblewaiting= 0;
	}
	ed->data[ed->count++]= 0x00;
	ed->data[ed->count++]= 0x00;
}

static void
encode_do_row( encodedata* ed, pixbuf* pb, int row ) {
	int i= 0;
	unsigned char* pix= pb->pixels + row * pb->x;
	int color= *pix;
	int n= 0; /* the number of pixels of this color */
	
	while( i++ < pb->x ) {
		/* FIXME: watch this space for EOL */
		if( *pix != color || n == 255 ) {
			encode_pixels( ed, color, n );
			color= *pix;
			n= 1;
		} else {
			n++;
		}
		pix++;
	}

	/* this small optimization: (n>63) can save up to two bytes per line
	 * I wonder if this is compatible with all the hardware... */
	if( color == 0 && n > 63 ) {
		encode_eol( ed );
	} else {
		encode_pixels( ed, color, n );
	}

	if( ed->nibblewaiting ) {
		ed->count++;
		ed->nibblewaiting= 0;
	}
}


void
pixbuf_encode_rle(int x, int y, int w, int h, char *inbuf,  int stride,encodedata *ed){
       	pixbuf pb;
	int i, row;
	pb.x = w;
	pb.y = h;

	pb.pixels = inbuf;
	ed->count= 4;
	ed->nibblewaiting= 0;

	row= 0;
	for( i= 0; i < pb.y; i++ ) {
		encode_do_row(ed, &pb, row);
		row+= 2;
		if( row > pb.y ) {
			row= 1;
			ed->oddstart= ed->count;
		}
	}
	encode_do_control(x,y, ed, &pb);
}


void
pixbuf_load_xpm( pixbuf* pb, char* xpm[] ) {
	int colors, chrs, l, n; 
	char c[4], table[256];
	unsigned char *b, *i;

	sscanf( xpm[0], "%d %d %d %d", &pb->x, &pb->y, &colors, &chrs);
	if( colors > 4 ) {
		fprintf( stderr, "the pixmap MUST be 4 colors or less\n");
		exit (-1);
	}
	if( chrs != 1 ) {
		fprintf( stderr, "the XPM format MUST be 1 char per pixel\n");
		exit (-1);
	}
	if( pb->x > 0xFFF || pb->y > 0xFFF ) {
		fprintf( stderr, "the size is excesive\n");
		exit (-1);
	}
	
	for( l=0; l<colors; l++ ) {
		n= sscanf( xpm[l+1], "%c c #%x", &c[l], &pb->rgb[l]);
		if( n < 2 ) {
			/* this one is transparent */
			pb->rgb[l]=0xff000000;
		}
		table[(int)c[l]]=l;
	}

	pb->pixels= malloc( pb->x * pb->y );
	b= pb->pixels;
	
	for( l= colors+1; l <= pb->y + colors; l++ ) {
		i= xpm[l];
		while( (int)*i) {
			*b++ = table[*i++];
		}
	}
}

void
pixbuf_delete( pixbuf* pb ) {
	free( pb->pixels );
}

