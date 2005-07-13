/* nuppelvideo.h  rh */

typedef struct  __attribute__((packed)) rtfileheader
{
  char finfo[12];     // "NuppelVideo" + \0
  char version[5];    // "0.05" + \0
  char pad1[3];
  int  width;
  int  height;
  int  desiredwidth;  // 0 .. as it is
  int  desiredheight; // 0 .. as it is
  char pimode;        // P .. progressive
		      // I .. interlaced  (2 half pics) [NI]
  char pad2[3];
  double aspect;      // 1.0 .. square pixel (1.5 .. e.g. width=480: width*1.5=720
                      // for capturing for svcd material
  double fps;
  int videoblocks;   // count of video-blocks -1 .. unknown   0 .. no video
  int audioblocks;   // count of audio-blocks -1 .. unknown   0 .. no audio
  int textsblocks;   // count of text-blocks  -1 .. unknown   0 .. no text
  int keyframedist;
} rtfileheader;

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
#define FILEHEADERSIZE  sizeof(rtfileheader)

typedef struct vidbuffertype
{
    int sample;
    int timecode;
    int freeToEncode;
    int freeToBuffer;
    unsigned char *buffer_offset;
} vidbuffertyp;

typedef struct audbuffertype
{
    int sample;
    int timecode;
    int freeToEncode;
    int freeToBuffer;
    unsigned char *buffer_offset;
} audbuffertyp;

#define le2me_rtfileheader(h) {					\
    (h)->width = le2me_32((h)->width);				\
    (h)->height = le2me_32((h)->height);			\
    (h)->desiredwidth = le2me_32((h)->desiredwidth);		\
    (h)->desiredheight = le2me_32((h)->desiredheight);		\
    (h)->aspect = le2me_dbl((h)->aspect);			\
    (h)->fps = le2me_dbl((h)->fps);				\
    (h)->videoblocks = le2me_32((h)->videoblocks);		\
    (h)->audioblocks = le2me_32((h)->audioblocks);		\
    (h)->textsblocks = le2me_32((h)->textsblocks);		\
    (h)->keyframedist = le2me_32((h)->keyframedist);		\
  }
#define le2me_rtframeheader(h) {				\
    (h)->timecode = le2me_32((h)->timecode);			\
    (h)->packetlength = le2me_32((h)->packetlength);		\
  }

