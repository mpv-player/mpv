#include "config.h"

/*
 * Hacked version of the linux cdrom.c kernel module - everything except the
 * DVD handling ripped out and the rest rewritten to use raw SCSI commands
 * on BSD/OS 4.2 (but should work with earlier versions as well).
*/

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include </sys/dev/scsi/scsi.h>
#include </sys/dev/scsi/scsi_ioctl.h>

#include "bsdi_dvd.h"

/*
 * Now get rid of the override/intercept macro so we can call the real ioctl()
 * routine!
*/
#undef	ioctl

#define CMD_READ_10             0x28
#define CMD_READ_TOC_PMA_ATIP   0x43
#define CMD_READ_CD             0xbe
#define	CMD_START_STOP_UNIT	0x1b

#define	CMD_SEND_KEY		0xa3
#define	CMD_REPORT_KEY		0xa4
#define	CMD_READ_DVD_STRUCTURE	0xad

#define copy_key(dest,src)	memcpy((dest), (src), sizeof(dvd_key))
#define copy_chal(dest,src)	memcpy((dest), (src), sizeof(dvd_challenge))

/* Define the Cdrom Generic Command structure */
typedef	struct	cgc
		{
		u_char	cdb[12];
		u_char	*buf;
		int	buflen;
		int	rw;
		int	timeout;
		scsi_user_sense_t *sus;
		} cgc_t;

static int scsi_cmd(int, cgc_t *);
static int cdrom_ioctl(int, u_long, void *);
static int cdrom_tray_move(int, int);
static void cdrom_count_tracks(int, tracktype *);
static int dvd_ioctl(int, u_long, void *);
static	int	debug = 0;

void dvd_cdrom_debug(int flag)
	{
	debug = flag;
	}

/*
 * This is the published entry point.   Actually applications should simply
 * include <dvd.h> and not refer to this at all.
*/
int dvd_cdrom_ioctl(int fd, unsigned long cmd, void *arg)
	{
	switch	(cmd)
		{
		case	DVD_AUTH:
		case	DVD_READ_STRUCT:
			return(dvd_ioctl(fd, cmd, arg));
		case	CDROMREADTOCHDR:
		case	CDROMREADTOCENTRY:
		case	CDROMEJECT:
		case	CDROMREADRAW:
		case	CDROMREADMODE1:
		case	CDROMREADMODE2:
		case	CDROMCLOSETRAY:
		case	CDROM_DRIVE_STATUS:
		case	CDROM_DISC_STATUS:
			return(cdrom_ioctl(fd, cmd, arg));
		default:
			return(ioctl(fd, cmd, arg));
		}
	}

static void setup_report_key(cgc_t *cgc, u_int agid, u_int type)
	{

	cgc->cdb[0] = CMD_REPORT_KEY;
	cgc->cdb[10] = type | (agid << 6);
	switch	(type)
		{
		case	0:
		case	5:
		case	8:
			cgc->buflen = 8;
			break;
		case	1:
			cgc->buflen = 16;
			break;
		case	2:
		case	4:
			cgc->buflen = 12;
			break;
		}
	cgc->cdb[9] = cgc->buflen;
	cgc->rw = SUC_READ;;
	}

static void setup_send_key(cgc_t *cgc, u_int agid, u_int type)
	{

	cgc->cdb[0] = CMD_SEND_KEY;
	cgc->cdb[10] = type | (agid << 6);
	switch	(type)
		{
		case	1:
			cgc->buflen = 16;
			break;
		case	3:
			cgc->buflen = 12;
			break;
		case	6:
			cgc->buflen = 8;
			break;
		}
	cgc->cdb[9] = cgc->buflen;
	cgc->rw = SUC_WRITE;
	}

static void cgc_init(cgc_t *cgc, void *buf, int len, int type)
	{

	memset(cgc, 0, sizeof (*cgc));
	if	(buf)
		memset(buf, 0, len);
	cgc->buf = (u_char *)buf;
	cgc->buflen = len;
	cgc->rw = type;
	cgc->timeout = 5;	/* 5 second timeout */
	}

static int dvd_do_auth(int fd, dvd_authinfo *ai)
	{
	int	ret;
	u_char	buf[20];
	cgc_t	cgc;
	rpc_state_t rpc_state;

	memset(buf, 0, sizeof(buf));
	cgc_init(&cgc, buf, 0, SUC_READ);

	switch	(ai->type)
		{
		case	DVD_LU_SEND_AGID:	/* LU data send */
			setup_report_key(&cgc, ai->lsa.agid, 0);
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			ai->lsa.agid = buf[7] >> 6;
			break;
		case	DVD_LU_SEND_KEY1:
			setup_report_key(&cgc, ai->lsk.agid, 2);
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			copy_key(ai->lsk.key, &buf[4]);
			break;
		case	DVD_LU_SEND_CHALLENGE:
			setup_report_key(&cgc, ai->lsc.agid, 1);
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			copy_chal(ai->lsc.chal, &buf[4]);
			break;
		case	DVD_LU_SEND_TITLE_KEY:	/* Post-auth key */
			setup_report_key(&cgc, ai->lstk.agid, 4);
			cgc.cdb[5] = ai->lstk.lba;
			cgc.cdb[4] = ai->lstk.lba >> 8;
			cgc.cdb[3] = ai->lstk.lba >> 16;
			cgc.cdb[2] = ai->lstk.lba >> 24;
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			ai->lstk.cpm = (buf[4] >> 7) & 1;
			ai->lstk.cp_sec = (buf[4] >> 6) & 1;
			ai->lstk.cgms = (buf[4] >> 4) & 3;
			copy_key(ai->lstk.title_key, &buf[5]);
			break;
		case	DVD_LU_SEND_ASF:
			setup_report_key(&cgc, ai->lsasf.agid, 5);
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			ai->lsasf.asf = buf[7] & 1;
			break;
		case	DVD_HOST_SEND_CHALLENGE: /* LU data receive (LU changes state) */
			setup_send_key(&cgc, ai->hsc.agid, 1);
			buf[1] = 0xe;
			copy_chal(&buf[4], ai->hsc.chal);
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			ai->type = DVD_LU_SEND_KEY1;
			break;
		case	DVD_HOST_SEND_KEY2:
			setup_send_key(&cgc, ai->hsk.agid, 3);
			buf[1] = 0xa;
			copy_key(&buf[4], ai->hsk.key);
			if	(ret = scsi_cmd(fd, &cgc))
				{
				ai->type = DVD_AUTH_FAILURE;
				return ret;
				}
			ai->type = DVD_AUTH_ESTABLISHED;
			break;
		case	DVD_INVALIDATE_AGID:
			setup_report_key(&cgc, ai->lsa.agid, 0x3f);
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			break;
		case	DVD_LU_SEND_RPC_STATE:	/* Get region settings */
			setup_report_key(&cgc, 0, 8);
			memset(&rpc_state, 0, sizeof(rpc_state_t));
			cgc.buf = (char *) &rpc_state;
			if	(ret = scsi_cmd(fd, &cgc))
				{
				ai->lrpcs.type = 0;
				ai->lrpcs.rpc_scheme = 0;
				}
			else
				{
				ai->lrpcs.type = rpc_state.type_code;
				ai->lrpcs.vra = rpc_state.vra;
				ai->lrpcs.ucca = rpc_state.ucca;
				ai->lrpcs.region_mask = rpc_state.region_mask;
				ai->lrpcs.rpc_scheme = rpc_state.rpc_scheme;
				}
			break;
		case	DVD_HOST_SEND_RPC_STATE:  /* Set region settings */
			setup_send_key(&cgc, 0, 6);
			buf[1] = 6;
			buf[4] = ai->hrpcs.pdrc;
			if	(ret = scsi_cmd(fd, &cgc))
				return ret;
			break;
		default:
			return EINVAL;
		}
	return 0;
	}

static int dvd_read_physical(int fd, dvd_struct *s)
	{
	int ret, i;
	u_char buf[4 + 4 * 20], *base;
	struct dvd_layer *layer;
	cgc_t cgc;

	cgc_init(&cgc, buf, sizeof(buf), SUC_READ);
	cgc.cdb[0] = CMD_READ_DVD_STRUCTURE;
	cgc.cdb[6] = s->physical.layer_num;
	cgc.cdb[7] = s->type;
	cgc.cdb[9] = cgc.buflen & 0xff;

	if	(ret = scsi_cmd(fd, &cgc))
		return ret;

	base = &buf[4];
	layer = &s->physical.layer[0];

	/* place the data... really ugly, but at least we won't have to
	   worry about endianess in userspace or here. */
	for	(i = 0; i < 4; ++i, base += 20, ++layer)
		{
		memset(layer, 0, sizeof(*layer));
		layer->book_version = base[0] & 0xf;
		layer->book_type = base[0] >> 4;
		layer->min_rate = base[1] & 0xf;
		layer->disc_size = base[1] >> 4;
		layer->layer_type = base[2] & 0xf;
		layer->track_path = (base[2] >> 4) & 1;
		layer->nlayers = (base[2] >> 5) & 3;
		layer->track_density = base[3] & 0xf;
		layer->linear_density = base[3] >> 4;
		layer->start_sector = base[5] << 16 | base[6] << 8 | base[7];
		layer->end_sector = base[9] << 16 | base[10] << 8 | base[11];
		layer->end_sector_l0 = base[13] << 16 | base[14] << 8 | base[15];
		layer->bca = base[16] >> 7;
		}
	return 0;
	}

static int dvd_read_copyright(int fd, dvd_struct *s)
	{
	int ret;
	u_char buf[8];
	cgc_t cgc;

	cgc_init(&cgc, buf, sizeof(buf), SUC_READ);
	cgc.cdb[0] = CMD_READ_DVD_STRUCTURE;
	cgc.cdb[6] = s->copyright.layer_num;
	cgc.cdb[7] = s->type;
	cgc.cdb[8] = cgc.buflen >> 8;
	cgc.cdb[9] = cgc.buflen & 0xff;

	if	(ret = scsi_cmd(fd, &cgc))
		return ret;
	s->copyright.cpst = buf[4];
	s->copyright.rmi = buf[5];
	return 0;
	}

static int dvd_read_disckey(int fd, dvd_struct *s)
	{
	int ret, size;
	u_char *buf;
	cgc_t cgc;

	size = sizeof(s->disckey.value) + 4;

	if	((buf = (u_char *) malloc(size)) == NULL)
		return ENOMEM;

	cgc_init(&cgc, buf, size, SUC_READ);
	cgc.cdb[0] = CMD_READ_DVD_STRUCTURE;
	cgc.cdb[7] = s->type;
	cgc.cdb[8] = size >> 8;
	cgc.cdb[9] = size & 0xff;
	cgc.cdb[10] = s->disckey.agid << 6;

	if	(!(ret = scsi_cmd(fd, &cgc)))
		memcpy(s->disckey.value, &buf[4], sizeof(s->disckey.value));
	free(buf);
	return ret;
	}

static int dvd_read_bca(int fd, dvd_struct *s)
	{
	int ret;
	u_char buf[4 + 188];
	cgc_t cgc;

	cgc_init(&cgc, buf, sizeof(buf), SUC_READ);
	cgc.cdb[0] = CMD_READ_DVD_STRUCTURE;
	cgc.cdb[7] = s->type;
	cgc.cdb[9] = cgc.buflen = 0xff;

	if	(ret = scsi_cmd(fd, &cgc))
		return ret;
	s->bca.len = buf[0] << 8 | buf[1];
	if	(s->bca.len < 12 || s->bca.len > 188)
		return EIO;
	memcpy(s->bca.value, &buf[4], s->bca.len);
	return 0;
	}

static int dvd_read_manufact(int fd, dvd_struct *s)
	{
	int ret = 0, size;
	u_char *buf;
	cgc_t cgc;

	size = sizeof(s->manufact.value) + 4;

	if	((buf = (u_char *) malloc(size)) == NULL)
		return ENOMEM;

	cgc_init(&cgc, buf, size, SUC_READ);
	cgc.cdb[0] = CMD_READ_DVD_STRUCTURE;
	cgc.cdb[7] = s->type;
	cgc.cdb[8] = size >> 8;
	cgc.cdb[9] = size & 0xff;

	if	(ret = scsi_cmd(fd, &cgc))
		{
		free(buf);
		return ret;
		}
	s->manufact.len = buf[0] << 8 | buf[1];
	if	(s->manufact.len < 0 || s->manufact.len > 2048)
		ret = -EIO;
	else
		memcpy(s->manufact.value, &buf[4], s->manufact.len);
	free(buf);
	return ret;
	}

static int dvd_read_struct(int fd, dvd_struct *s)
	{
	switch	(s->type)
		{
		case	DVD_STRUCT_PHYSICAL:
			return dvd_read_physical(fd, s);
		case	DVD_STRUCT_COPYRIGHT:
			return dvd_read_copyright(fd, s);
		case	DVD_STRUCT_DISCKEY:
			return dvd_read_disckey(fd, s);
		case	DVD_STRUCT_BCA:
			return dvd_read_bca(fd, s);
		case	DVD_STRUCT_MANUFACT:
			return dvd_read_manufact(fd, s);
		default:
			return EINVAL;
		}
	}

static	u_char scsi_cdblen[8] = {6, 10, 10, 12, 12, 12, 10, 10};

static int scsi_cmd(int fd, cgc_t *cgc)
	{
	int	i, scsistatus, cdblen;
	unsigned char	*cp;
	struct	scsi_user_cdb suc;

    /* safety checks */
	if	(cgc->rw != SUC_READ && cgc->rw != SUC_WRITE)
		return(EINVAL);

	suc.suc_flags = cgc->rw;
	cdblen = scsi_cdblen[(cgc->cdb[0] >> 5) & 7];
	suc.suc_cdblen = cdblen;
	bcopy(cgc->cdb, suc.suc_cdb, cdblen);
	suc.suc_data = cgc->buf;
	suc.suc_datalen = cgc->buflen;
	suc.suc_timeout = cgc->timeout;
	if	(ioctl(fd, SCSIRAWCDB, &suc) == -1)
		return(errno);
	scsistatus = suc.suc_sus.sus_status;

/*
 * If the device returns a scsi sense error and debugging is enabled print
 * some hopefully useful information on stderr.
*/
	if	(scsistatus && debug)
		{
		cp = suc.suc_sus.sus_sense;
		fprintf(stderr,"scsistatus = %x cdb =",
			scsistatus);
		for	(i = 0; i < cdblen; i++)
			fprintf(stderr, " %x", cgc->cdb[i]);
		fprintf(stderr, "\nsense =");
		for	(i = 0; i < 16; i++)
			fprintf(stderr, " %x", cp[i]);
		fprintf(stderr, "\n");
		}
	if	(cgc->sus)
		bcopy(&suc.suc_sus, cgc->sus, sizeof (struct scsi_user_sense));
	if	(scsistatus)
		return(EIO);	/* generic i/o error for unsuccessful status */
	return(0);
	}

/*
 * The entry point for the DVDioctls for BSD/OS.
*/
static int dvd_ioctl(int fd, u_long cmd, void *arg)
	{
	int	ret;

	switch	(cmd)
		{
		case	DVD_READ_STRUCT:
			ret = dvd_read_struct(fd, (dvd_struct *)arg);
			if	(ret)
				errno = ret;
			return(ret ? -1 : 0);
		case	DVD_AUTH:
			ret = dvd_do_auth(fd, (dvd_authinfo *)arg);
			if	(ret)
				errno = ret;
			return(ret ? -1 : 0);
		default:
			errno = EINVAL;
			return(-1);
		}
	}

/*
 * The entry point for the CDROMioctls for BSD/OS
*/
static int cdrom_read_block(int, cgc_t *, int, int, int, int);
static int cdrom_read_cd(int, cgc_t *, int, int, int );
	int cdrom_blocksize(int, int );

static inline
int msf_to_lba(char m, char s, char f)
{
	return (((m * CD_SECS) + s) * CD_FRAMES + f) - CD_MSF_OFFSET;
}

cdrom_ioctl(int fd, u_long cmd, void *arg)
	{
	int	ret;
	cgc_t	cgc;

	switch	(cmd)
		{
		case	CDROMREADRAW:
		case	CDROMREADMODE1:
		case	CDROMREADMODE2:
			{
			struct cdrom_msf *msf;
			int blocksize = 0, format = 0, lba;
		
			switch	(cmd)
				{
				case	CDROMREADRAW:
					blocksize = CD_FRAMESIZE_RAW;
					break;
				case	CDROMREADMODE1:
					blocksize = CD_FRAMESIZE;
					format = 2;
					break;
				case	CDROMREADMODE2:
					blocksize = CD_FRAMESIZE_RAW0;
					break;
				}
			msf = (struct cdrom_msf *)arg;
			lba = msf_to_lba(msf->cdmsf_min0,msf->cdmsf_sec0,
				msf->cdmsf_frame0);
			ret = EINVAL;
			if	(lba < 0)
				break;

			cgc_init(&cgc, arg, blocksize, SUC_READ);
			ret = cdrom_read_block(fd, &cgc, lba, 1, format,							blocksize);
			if	(ret)
				{
/*
 * SCSI-II devices are not required to support CMD_READ_CD (which specifies
 * the blocksize to read) so try switching the block size with a mode select,
 * doing the normal read sector command and then changing the sector size back
 * to 2048.
 *
 * If the program dies before changing the blocksize back sdopen()
 * in the kernel will fail opens with a message that looks something like:
 *
 * "sr1: blksize 2336 not multiple of 512: cannot use"
 *
 * At that point the drive has to be power cycled (or reset in some other way).
*/
				if	(ret = cdrom_blocksize(fd, blocksize))
					break;
				ret = cdrom_read_cd(fd, &cgc, lba, blocksize, 1);
				ret |= cdrom_blocksize(fd, 2048);
				}
			break;
			}
		case	CDROMREADTOCHDR:
			{
			struct cdrom_tochdr *tochdr = (struct cdrom_tochdr *) arg;
			u_char buffer[12];
			
			cgc_init(&cgc, buffer, sizeof (buffer), SUC_READ);
			cgc.cdb[0] = CMD_READ_TOC_PMA_ATIP;
			cgc.cdb[1] = 0x2;	/* MSF */
			cgc.cdb[8] = 12;	/* LSB of length */

			ret = scsi_cmd(fd, &cgc);
			if	(!ret)
				{
				tochdr->cdth_trk0 = buffer[2];
				tochdr->cdth_trk1 = buffer[3];
				}
			break;
			}
		case	CDROMREADTOCENTRY:
			{
			struct cdrom_tocentry *tocentry = (struct cdrom_tocentry *) arg;
			u_char	buffer[12];

			cgc_init(&cgc, buffer, sizeof (buffer), SUC_READ);
			cgc.cdb[0] = CMD_READ_TOC_PMA_ATIP;
			cgc.cdb[1] = (tocentry->cdte_format == CDROM_MSF) ? 0x02 : 0;
			cgc.cdb[6] = tocentry->cdte_track;
			cgc.cdb[8] = 12;		/* LSB of length */

			ret = scsi_cmd(fd, &cgc);
			if	(ret)
				break;

			tocentry->cdte_ctrl = buffer[5] & 0xf;
			tocentry->cdte_adr = buffer[5] >> 4;
			tocentry->cdte_datamode = (tocentry->cdte_ctrl & 0x04) ? 1 : 0;
			if	(tocentry->cdte_format == CDROM_MSF)
				{
				tocentry->cdte_addr.msf.minute = buffer[9];
				tocentry->cdte_addr.msf.second = buffer[10];
				tocentry->cdte_addr.msf.frame = buffer[11];
				}
			else
				tocentry->cdte_addr.lba = (((((buffer[8] << 8)
						+ buffer[9]) << 8)
						+ buffer[10]) << 8)
						+ buffer[11];
			break;
			}
		case	CDROMEJECT:		/* NO-OP for now */
			ret = cdrom_tray_move(fd, 1);
			break;
		case	CDROMCLOSETRAY:
			ret = cdrom_tray_move(fd, 0);
			break;
/*
 * This sucks but emulates the expected behaviour.  Instead of the return
 * value being the actual status a success/fail indicator should have been
 * returned and the 3rd arg to the ioctl should have been an 'int *' to update
 * with the actual status.   Both the drive and disc status ioctl calls are
 * similarily braindamaged.
*/
		case	CDROM_DRIVE_STATUS:
			return(CDS_NO_INFO);	/* XXX */
		case	CDROM_DISC_STATUS:
			{
			tracktype tracks;
			int	cnt;

			cdrom_count_tracks(fd, &tracks);
			if	(tracks.error)
				return(tracks.error);
			if	(tracks.audio > 0)
				{
				cnt = tracks.data + tracks.cdi + tracks.xa;
				if	(cnt == 0)
					return(CDS_AUDIO);
				else
					return(CDS_MIXED);
				}
			if	(tracks.cdi)
				return(CDS_XA_2_2);
			if	(tracks.xa)
				return(CDS_XA_2_1);
			if	(tracks.data)
				return(CDS_DATA_1);
			return(CDS_NO_INFO);
			}
		}
	errno = ret;
	return(ret ? -1 : 0);
	}

static int cdrom_read_cd(int fd, cgc_t *cgc, int lba, int blocksize, int nblocks)
	{

	memset(&cgc->cdb, 0, sizeof(cgc->cdb));
	cgc->cdb[0] = CMD_READ_10;
	cgc->cdb[2] = (lba >> 24) & 0xff;
	cgc->cdb[3] = (lba >> 16) & 0xff;
	cgc->cdb[4] = (lba >>  8) & 0xff;
	cgc->cdb[5] = lba & 0xff;
	cgc->cdb[6] = (nblocks >> 16) & 0xff;
	cgc->cdb[7] = (nblocks >>  8) & 0xff;
	cgc->cdb[8] = nblocks & 0xff;
	cgc->buflen = blocksize * nblocks;
	return(scsi_cmd(fd, cgc));
	}

static int cdrom_read_block(int fd, cgc_t *cgc,
			    int lba, int nblocks, int format, int blksize)
	{

	memset(&cgc->cdb, 0, sizeof(cgc->cdb));
	cgc->cdb[0] = CMD_READ_CD;
	/* expected sector size - cdda,mode1,etc. */
	cgc->cdb[1] = format << 2;
	/* starting address */
	cgc->cdb[2] = (lba >> 24) & 0xff;
	cgc->cdb[3] = (lba >> 16) & 0xff;
	cgc->cdb[4] = (lba >>  8) & 0xff;
	cgc->cdb[5] = lba & 0xff;
	/* number of blocks */
	cgc->cdb[6] = (nblocks >> 16) & 0xff;
	cgc->cdb[7] = (nblocks >>  8) & 0xff;
	cgc->cdb[8] = nblocks & 0xff;
	cgc->buflen = blksize * nblocks;
	
	/* set the header info returned */
	switch	(blksize)
		{
		case	CD_FRAMESIZE_RAW0:
			cgc->cdb[9] = 0x58;
			break;
		case	CD_FRAMESIZE_RAW1:
			cgc->cdb[9] = 0x78;
			break;
		case	CD_FRAMESIZE_RAW:
			cgc->cdb[9] = 0xf8;
			break;
		default:
			cgc->cdb[9] = 0x10;
		}
	return(scsi_cmd(fd, cgc));
	}

static void cdrom_count_tracks(int fd, tracktype *tracks)
	{
	struct	cdrom_tochdr header;
	struct	cdrom_tocentry entry;
	int	ret, i;

	memset(tracks, 0, sizeof (*tracks));
	ret = cdrom_ioctl(fd, CDROMREADTOCHDR, &header);
/*
 * This whole business is a crock anyhow so we don't bother distinguishing
 * between no media, drive not ready, etc and on any error just say we have
 * no info.
*/
	if	(ret)
		{
		tracks->error = CDS_NO_INFO;
		return;
		}

	entry.cdte_format = CDROM_MSF;
	for	(i = header.cdth_trk0; i <= header.cdth_trk1; i++)
		{
		entry.cdte_track = i;
		if	(cdrom_ioctl(fd, CDROMREADTOCENTRY, &entry))
			{
			tracks->error = CDS_NO_INFO;
			return;
			}
		if	(entry.cdte_ctrl & CDROM_DATA_TRACK)
			{
			if	(entry.cdte_format == 0x10)
				tracks->cdi++;
			else if	(entry.cdte_format == 0x20)
				tracks->xa++;
			else
				tracks->data++;
			}
		else
			tracks->audio++;
		}
	return;
	}

static int cdrom_tray_move(int fd, int flag)
	{
	cgc_t	cgc;

	cgc_init(&cgc, NULL, 0, SUC_READ);
	cgc.cdb[0] = CMD_START_STOP_UNIT;
	cgc.cdb[1] = 1;			/* immediate */
	cgc.cdb[4] = flag ? 0x2 : 0x3;	/* eject : close */
	return(scsi_cmd(fd, &cgc));
	}

/*
 * Required when we need to use READ_10 to issue other than 2048 block
 * reads
 */
int cdrom_blocksize(int fd, int size)
	{
	cgc_t	cgc;
	struct modesel_head mh;

	memset(&mh, 0, sizeof(mh));
	mh.block_desc_length = 0x08;
	mh.block_length_med = (size >> 8) & 0xff;
	mh.block_length_lo = size & 0xff;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cdb[0] = 0x15;
	cgc.cdb[1] = 1 << 4;
	cgc.cdb[4] = 12;
	cgc.buflen = sizeof(mh);
	cgc.buf = (u_char *) &mh;
	cgc.rw = SUC_WRITE;
	mh.block_desc_length = 0x08;
	mh.block_length_med = (size >> 8) & 0xff;
	mh.block_length_lo = size & 0xff;
	return(scsi_cmd(fd, &cgc));
	}
