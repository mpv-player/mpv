#ifndef	_DVD_H_
#define	_DVD_H_

#include <sys/cdefs.h>
#include <machine/endian.h>
#include <sys/ioctl.h>

__BEGIN_DECLS
int	dvd_cdrom_ioctl(int, unsigned long, void *);
int	cdrom_blocksize(int, int);
void	dvd_cdrom_debug(int);
__END_DECLS

#define	ioctl(a,b,c)	dvd_cdrom_ioctl((a),(b),(c))

typedef	unsigned char __u8;
typedef	unsigned short __u16;
typedef	unsigned int __u32;

#define DVD_READ_STRUCT		0x5390  /* Read structure */
#define DVD_WRITE_STRUCT	0x5391  /* Write structure */
#define DVD_AUTH		0x5392  /* Authentication */

#define DVD_STRUCT_PHYSICAL	0x00
#define DVD_STRUCT_COPYRIGHT	0x01
#define DVD_STRUCT_DISCKEY	0x02
#define DVD_STRUCT_BCA		0x03
#define DVD_STRUCT_MANUFACT	0x04

struct dvd_layer {
	__u8 book_version	: 4;
	__u8 book_type		: 4;
	__u8 min_rate		: 4;
	__u8 disc_size		: 4;
	__u8 layer_type		: 4;
	__u8 track_path		: 1;
	__u8 nlayers		: 2;
	__u8 track_density	: 4;
	__u8 linear_density	: 4;
	__u8 bca		: 1;
	__u32 start_sector;
	__u32 end_sector;
	__u32 end_sector_l0;
};

struct dvd_physical {
	__u8 type;
	__u8 layer_num;
	struct dvd_layer layer[4];
};

struct dvd_copyright {
	__u8 type;

	__u8 layer_num;
	__u8 cpst;
	__u8 rmi;
};

struct dvd_disckey {
	__u8 type;

	unsigned agid		: 2;
	__u8 value[2048];
};

struct dvd_bca {
	__u8 type;

	int len;
	__u8 value[188];
};

struct dvd_manufact {
	__u8 type;

	__u8 layer_num;
	int len;
	__u8 value[2048];
};

typedef union {
	__u8 type;

	struct dvd_physical	physical;
	struct dvd_copyright	copyright;
	struct dvd_disckey	disckey;
	struct dvd_bca		bca;
	struct dvd_manufact	manufact;
} dvd_struct;

/*
 * DVD authentication ioctl
 */

/* Authentication states */
#define DVD_LU_SEND_AGID	0
#define DVD_HOST_SEND_CHALLENGE	1
#define DVD_LU_SEND_KEY1	2
#define DVD_LU_SEND_CHALLENGE	3
#define DVD_HOST_SEND_KEY2	4

/* Termination states */
#define DVD_AUTH_ESTABLISHED	5
#define DVD_AUTH_FAILURE	6

/* Other functions */
#define DVD_LU_SEND_TITLE_KEY	7
#define DVD_LU_SEND_ASF		8
#define DVD_INVALIDATE_AGID	9
#define DVD_LU_SEND_RPC_STATE	10
#define DVD_HOST_SEND_RPC_STATE	11

/* State data */
typedef __u8 dvd_key[5];		/* 40-bit value, MSB is first elem. */
typedef __u8 dvd_challenge[10];	/* 80-bit value, MSB is first elem. */

struct dvd_lu_send_agid {
	__u8 type;
	unsigned agid		: 2;
};

struct dvd_host_send_challenge {
	__u8 type;
	unsigned agid		: 2;

	dvd_challenge chal;
};

struct dvd_send_key {
	__u8 type;
	unsigned agid		: 2;

	dvd_key key;
};

struct dvd_lu_send_challenge {
	__u8 type;
	unsigned agid		: 2;

	dvd_challenge chal;
};

#define DVD_CPM_NO_COPYRIGHT	0
#define DVD_CPM_COPYRIGHTED	1

#define DVD_CP_SEC_NONE		0
#define DVD_CP_SEC_EXIST	1

#define DVD_CGMS_UNRESTRICTED	0
#define DVD_CGMS_SINGLE		2
#define DVD_CGMS_RESTRICTED	3

struct dvd_lu_send_title_key {
	__u8 type;
	unsigned agid		: 2;

	dvd_key title_key;
	int lba;
	unsigned cpm		: 1;
	unsigned cp_sec		: 1;
	unsigned cgms		: 2;
};

struct dvd_lu_send_asf {
	__u8 type;
	unsigned agid		: 2;

	unsigned asf		: 1;
};

struct dvd_host_send_rpcstate {
	__u8 type;
	__u8 pdrc;
};

struct dvd_lu_send_rpcstate {
	__u8 type		: 2;
	__u8 vra		: 3;
	__u8 ucca		: 3;
	__u8 region_mask;
	__u8 rpc_scheme;
};

typedef union {
	__u8 type;

	struct dvd_lu_send_agid		lsa;
	struct dvd_host_send_challenge	hsc;
	struct dvd_send_key		lsk;
	struct dvd_lu_send_challenge	lsc;
	struct dvd_send_key		hsk;
	struct dvd_lu_send_title_key	lstk;
	struct dvd_lu_send_asf		lsasf;
	struct dvd_host_send_rpcstate	hrpcs;
	struct dvd_lu_send_rpcstate	lrpcs;
} dvd_authinfo;


typedef struct {
	__u16 report_key_length;
	__u8 reserved1;
	__u8 reserved2;
#if BYTE_ORDER == BIG_ENDIAN
	__u8 type_code			: 2;
	__u8 vra			: 3;
	__u8 ucca			: 3;
#elif BYTE_ORDER == LITTLE_ENDIAN
	__u8 ucca			: 3;
	__u8 vra			: 3;
	__u8 type_code			: 2;
#endif
	__u8 region_mask;
	__u8 rpc_scheme;
	__u8 reserved3;
} rpc_state_t;

/*
 * Stuff for the CDROM ioctls
*/

#define CDROMREADTOCHDR		0x5305 /* Read TOC header (cdrom_tochdr) */
#define CDROMREADTOCENTRY	0x5306 /* Read TOC entry (cdrom_tocentry) */
#define CDROMEJECT		0x5309 /* Ejects the cdrom media */
#define CDROMCLOSETRAY          0x5319 /* Reverse of CDROMEJECT */
#define CDROM_DRIVE_STATUS      0x5326 /* Get tray position, etc. */
#define CDROM_DISC_STATUS	0x5327 /* Get disc type, etc. */
#define CDROMREADMODE2		0x530c /* Read CDROM mode 2 data (2336 Bytes) */
#define CDROMREADMODE1		0x530d /* Read CDROM mode 1 data (2048 Bytes) */
#define CDROMREADRAW            0x5314 /* read data in raw mode (2352 bytes) */

#define CD_MINS              74 /* max. minutes per CD, not really a limit */
#define CD_SECS              60 /* seconds per minute */
#define CD_FRAMES            75 /* frames per second */
#define CD_MSF_OFFSET       150 /* MSF numbering offset of first frame */

#define CD_HEAD_SIZE          4 /* header (address) bytes per raw data frame */
#define CD_SYNC_SIZE         12 /* 12 sync bytes per raw data frame */
#define CD_FRAMESIZE       2048 /* bytes per frame, "cooked" mode */
#define CD_FRAMESIZE_RAW   2352 /* bytes per frame, "raw" mode */
#define CD_FRAMESIZE_RAW0 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE-CD_HEAD_SIZE) /*2336*/
#define CD_FRAMESIZE_RAW1 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE) 	/*2340*/

/* CD-ROM address types (cdrom_tocentry.cdte_format) */
#define CDROM_LBA 0x01 		/* logical block: first frame is #0 */
#define CDROM_MSF 0x02 		/* minute-second-frame: binary. not bcd here!*/

/* bit to tell whether track is data or audio (cdrom_tocentry.cdte_ctrl) */
#define CDROM_DATA_TRACK        0x04

/* The leadout track is always 0xAA, regardless of # of tracks on disc */
#define CDROM_LEADOUT           0xAA

/* drive status returned by CDROM_DRIVE_STATUS ioctl */
#define CDS_NO_INFO             0       /* if not implemented */
#define CDS_NO_DISC             1
#define CDS_TRAY_OPEN           2
#define CDS_DRIVE_NOT_READY     3
#define CDS_DISC_OK             4

/*
 * Return values for CDROM_DISC_STATUS ioctl.
 * Can also return CDS_NO_INFO and CDS_NO_DISC from above
*/
#define	CDS_AUDIO		100
#define	CDS_DATA_1		101
#define	CDS_DATA_2		102
#define	CDS_XA_2_1		103
#define	CDS_XA_2_2		104
#define	CDS_MIXED		105

/* For compile compatibility only - we don't support changers */
#define CDSL_NONE               ((int) (~0U>>1)-1)
#define CDSL_CURRENT            ((int) (~0U>>1))

struct cdrom_msf
{
	__u8	cdmsf_min0;	/* start minute */
	__u8	cdmsf_sec0;	/* start second */
	__u8	cdmsf_frame0;	/* start frame */
	__u8	cdmsf_min1;	/* end minute */
	__u8	cdmsf_sec1;	/* end second */
	__u8	cdmsf_frame1;	/* end frame */
};

struct	cdrom_tochdr
	{
	__u8	cdth_trk0;	/* start track */
	__u8	cdth_trk1;	/* end track */
	};

struct cdrom_msf0
{
	__u8	minute;
	__u8	second;
	__u8	frame;
};

union cdrom_addr
{
	struct cdrom_msf0	msf;
	int			lba;
};

struct cdrom_tocentry
{
	__u8	cdte_track;
	__u8	cdte_adr	:4;
	__u8	cdte_ctrl	:4;
	__u8	cdte_format;
	union cdrom_addr cdte_addr;
	__u8	cdte_datamode;
};

struct modesel_head
{
	__u8	reserved1;
	__u8	medium;
	__u8	reserved2;
	__u8	block_desc_length;
	__u8	density;
	__u8	number_of_blocks_hi;
	__u8	number_of_blocks_med;
	__u8	number_of_blocks_lo;
	__u8	reserved3;
	__u8	block_length_hi;
	__u8	block_length_med;
	__u8	block_length_lo;
};

typedef	struct
{
	int	data;
	int	audio;
	int	cdi;
	int	xa;
	int	error;
} tracktype;

#endif /* _DVD_H_ */
