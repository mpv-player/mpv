/*
   nuppelvideo.h taken from NuppelVideo, by
   (c) Roman Hochleitner roman@mars.tuwien.ac.at

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef MPLAYER_NUPPELVIDEO_H
#define MPLAYER_NUPPELVIDEO_H

typedef struct  __attribute__((packed)) rtframeheader
{
   char frametype;			// A .. Audio, V .. Video, S .. Sync, T .. Text
   					// R .. Seekpoint: String RTjjjjjjjj (use full packet)
					// D .. Addition Data for Compressors
   					//      ct: R .. RTjpeg Tables

   char comptype;			// V: 0 .. Uncompressed [NI]
					//    1 .. RTJpeg
					//    2 .. RTJpeg with lzo afterwards
					//    N .. black frame
					//    L .. simply copy last frame (if lost frames)
    					// A: 0 .. Uncompressed (44100/sec 16bit 2ch)
    					//    1 .. lzo compression [NI]
    					//    2 .. layer2 (packet) [NI]
    					//    3 .. layer3 (packet) [NI]
    					//    F .. flac (lossless) [NI]
    					//    S .. shorten (lossless) [NI]
					//    N .. null frame loudless
					//    L .. simply copy last frame (may sound bad) NI
					// S: B .. Audio and Video sync point [NI]
                                        //    A .. Audio Sync Information
					//         timecode == effective dsp-frequency*100
					//         when reaching this audio sync point
					//         because many cheap soundcards are unexact 
					//         and have a range from 44000 to 44250
					//         instead of the expected exact 44100 S./sec
					//    V .. Next Video Sync 
					//         timecode == next video framenumber
					//    S .. Audio,Video,Text Correlation [NI]
   char keyframe;			//    0 .. keyframe
					//    1 .. nr of frame in gop => no keyframe

   char filters;			//    Every bit stands for one type of filter
					//    1 .. Gauss 5 Pixel (8*m+2*l+2*r+2*a+2*b)/16 [NYI]
					//    2 .. Gauss 5 Pixel (8*m+1*l+1*r+1*a+1*b)/12 [NYI]
					//    4 .. Cartoon Filter   [NI]
					//    8 .. Reserverd Filter [NI]
					//   16 .. Reserverd Filter [NI]
					//   32 .. Reserverd Filter [NI]
					//   64 .. Reserverd Filter [NI]
					//  128 .. Reserverd Filter [NI]

   int  timecode;			// Timecodeinformation sec*1000 + msecs
 
   int  packetlength;                   // V,A,T: length of following data in stream
   					// S:     length of packet correl. information [NI]
   					// R:     do not use here! (fixed 'RTjjjjjjjjjjjjjj')
} rtframeheader;

#define FRAMEHEADERSIZE sizeof(rtframeheader)

#define le2me_rtframeheader(h) {				\
    (h)->timecode = le2me_32((h)->timecode);			\
    (h)->packetlength = le2me_32((h)->packetlength);		\
  }

#endif /* MPLAYER_NUPPELVIDEO_H */
