/* The sample format system used lin libaf is based on bitmasks. The
   format definition only refers to the storage format not the
   resolution. */

// Endianess
#define AF_FORMAT_BE		(0<<0) // Big Endian
#define AF_FORMAT_LE		(1<<0) // Little Endian
#define AF_FORMAT_END_MASK	(1<<0)

#if WORDS_BIGENDIAN	       	// Native endian of cpu
#define	AF_FORMAT_NE		AF_FORMAT_BE
#else
#define	AF_FORMAT_NE		AF_FORMAT_LE
#endif

// Signed/unsigned
#define AF_FORMAT_SI		(0<<1) // SIgned
#define AF_FORMAT_US		(1<<1) // Un Signed
#define AF_FORMAT_SIGN_MASK	(1<<1)

// Fixed of floating point
#define AF_FORMAT_I		(0<<2) // Int
#define AF_FORMAT_F		(1<<2) // Foating point
#define AF_FORMAT_POINT_MASK	(1<<2)

// Special flags refering to non pcm data
#define AF_FORMAT_MU_LAW	(1<<3) // 
#define AF_FORMAT_A_LAW		(2<<3) // 
#define AF_FORMAT_MPEG2		(3<<3) // MPEG(2) audio
#define AF_FORMAT_AC3		(4<<3) // Dolby Digital AC3
#define AF_FORMAT_IMA_ADPCM	AF_FORMAT_LE|AF_FORMAT_SI // Same as 16 bit signed int 
#define AF_FORMAT_SPECIAL_MASK	(7<<3)

extern char* fmt2str(int format, char* str, size_t size);

