/*****************************************************************************
 * ioctl.h: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

int ioctl_ReadCopyright     ( int, int, int * );
int ioctl_ReadDiscKey       ( int, int *, uint8_t * );
int ioctl_ReadTitleKey      ( int, int *, int, uint8_t * );
int ioctl_ReportAgid        ( int, int * );
int ioctl_ReportChallenge   ( int, int *, uint8_t * );
int ioctl_ReportKey1        ( int, int *, uint8_t * );
int ioctl_ReportASF         ( int, int *, int * );
int ioctl_InvalidateAgid    ( int, int * );
int ioctl_SendChallenge     ( int, int *, uint8_t * );
int ioctl_SendKey2          ( int, int *, uint8_t * );
int ioctl_ReportRPC         ( int, int *, int *, int * );
int ioctl_SendRPC           ( int, int );

#define DVD_KEY_SIZE 5
#define DVD_CHALLENGE_SIZE 10
#define DVD_DISCKEY_SIZE 2048

/*****************************************************************************
 * Common macro, BeOS specific
 *****************************************************************************/
#if defined( SYS_BEOS )
#define INIT_RDC( TYPE, SIZE ) \
    raw_device_command rdc; \
    uint8_t p_buffer[ (SIZE)+1 ]; \
    memset( &rdc, 0, sizeof( raw_device_command ) ); \
    rdc.data = (char *)p_buffer; \
    rdc.data_length = (SIZE); \
    BeInitRDC( &rdc, (TYPE) );
#endif

/*****************************************************************************
 * Common macro, HP-UX specific
 *****************************************************************************/
#if defined( HPUX_SCTL_IO )
#define INIT_SCTL_IO( TYPE, SIZE ) \
    struct sctl_io sctl_io; \
    uint8_t p_buffer[ (SIZE)+1 ]; \
    memset( &sctl_io, 0, sizeof( sctl_io ) ); \
    sctl_io.data = (void *)p_buffer; \
    sctl_io.data_length = (SIZE); \
    HPUXInitSCTL( &sctl_io, (TYPE) );
#endif

/*****************************************************************************
 * Common macro, Solaris specific
 *****************************************************************************/
#if defined( SOLARIS_USCSI )
#define USCSI_TIMEOUT( SC, TO ) ( (SC)->uscsi_timeout = (TO) )
#define USCSI_RESID( SC )       ( (SC)->uscsi_resid )
#define INIT_USCSI( TYPE, SIZE ) \
    struct uscsi_cmd sc; \
    union scsi_cdb rs_cdb; \
    uint8_t p_buffer[ (SIZE)+1 ]; \
    memset( &sc, 0, sizeof( struct uscsi_cmd ) ); \
    sc.uscsi_cdb = (caddr_t)&rs_cdb; \
    sc.uscsi_bufaddr = (caddr_t)p_buffer; \
    sc.uscsi_buflen = (SIZE); \
    SolarisInitUSCSI( &sc, (TYPE) );
#endif

/*****************************************************************************
 * Common macro, Darwin specific
 *****************************************************************************/
#if defined( DARWIN_DVD_IOCTL )
#define INIT_DVDIOCTL( DKDVD_TYPE, BUFFER_TYPE, FORMAT ) \
    DKDVD_TYPE dvd; \
    BUFFER_TYPE dvdbs; \
    memset( &dvd, 0, sizeof(dvd) ); \
    memset( &dvdbs, 0, sizeof(dvdbs) ); \
    dvd.format = FORMAT; \
    dvd.buffer = &dvdbs; \
    dvd.bufferLength = sizeof(dvdbs);
#endif

/*****************************************************************************
 * Common macro, win32 specific
 *****************************************************************************/
#if defined( WIN32 )
#define INIT_SPTD( TYPE, SIZE ) \
    DWORD tmp; \
    SCSI_PASS_THROUGH_DIRECT sptd; \
    uint8_t p_buffer[ (SIZE) ]; \
    memset( &sptd, 0, sizeof( SCSI_PASS_THROUGH_DIRECT ) ); \
    sptd.Length = sizeof( SCSI_PASS_THROUGH_DIRECT ); \
    sptd.DataBuffer = p_buffer; \
    sptd.DataTransferLength = (SIZE); \
    WinInitSPTD( &sptd, (TYPE) );
#define SEND_SPTD( DEV, SPTD, TMP ) \
    (DeviceIoControl( (HANDLE)(DEV), IOCTL_SCSI_PASS_THROUGH_DIRECT, \
                      (SPTD), sizeof( SCSI_PASS_THROUGH_DIRECT ), \
                      (SPTD), sizeof( SCSI_PASS_THROUGH_DIRECT ), \
                      (TMP), NULL ) ? 0 : -1)
#define INIT_SSC( TYPE, SIZE ) \
    struct SRB_ExecSCSICmd ssc; \
    uint8_t p_buffer[ (SIZE)+1 ]; \
    memset( &ssc, 0, sizeof( struct SRB_ExecSCSICmd ) ); \
    ssc.SRB_BufPointer = (char *)p_buffer; \
    ssc.SRB_BufLen = (SIZE); \
    WinInitSSC( &ssc, (TYPE) );
#endif

/*****************************************************************************
 * Common macro, QNX specific
 *****************************************************************************/
#if defined( __QNXNTO__ )
#define INIT_CPT( TYPE, SIZE ) \
    CAM_PASS_THRU * p_cpt; \
    uint8_t * p_buffer; \
    int structSize = sizeof( CAM_PASS_THRU ) + (SIZE); \
    p_cpt = (CAM_PASS_THRU *) malloc ( structSize ); \
    p_buffer = (uint8_t *) p_cpt + sizeof( CAM_PASS_THRU ); \
    memset( p_cpt, 0, structSize ); \
      p_cpt->cam_data_ptr = sizeof( CAM_PASS_THRU ); \
      p_cpt->cam_dxfer_len = (SIZE); \
    QNXInitCPT( p_cpt, (TYPE) );
#endif

/*****************************************************************************
 * Common macro, OS2 specific
 *****************************************************************************/
#if defined( SYS_OS2 )
#define INIT_SSC( TYPE, SIZE ) \
    struct OS2_ExecSCSICmd sdc; \
    uint8_t p_buffer[ (SIZE)+1 ]; \
    unsigned long ulParamLen; \
    unsigned long ulDataLen; \
    memset( &sdc, 0, sizeof( OS2_ExecSCSICmd ) ); \
    memset( &p_buffer, 0, SIZE ); \
    sdc.data_length = (SIZE); \
    ulParamLen = sizeof(sdc); \
    OS2InitSDC( &sdc, (TYPE) )
#endif

/*****************************************************************************
 * Additional types, OpenBSD specific
 *****************************************************************************/
#if defined( HAVE_OPENBSD_DVD_STRUCT )
typedef union dvd_struct dvd_struct;
typedef union dvd_authinfo dvd_authinfo;
#endif

/*****************************************************************************
 * Various DVD I/O tables
 *****************************************************************************/
#if defined( SYS_BEOS ) || defined( WIN32 ) || defined ( SOLARIS_USCSI ) || defined ( HPUX_SCTL_IO ) || defined ( __QNXNTO__ ) || defined ( SYS_OS2 )
    /* The generic packet command opcodes for CD/DVD Logical Units,
     * From Table 57 of the SFF8090 Ver. 3 (Mt. Fuji) draft standard. */
#   define GPCMD_READ_DVD_STRUCTURE 0xad
#   define GPCMD_REPORT_KEY         0xa4
#   define GPCMD_SEND_KEY           0xa3
    /* DVD struct types */
#   define DVD_STRUCT_PHYSICAL      0x00
#   define DVD_STRUCT_COPYRIGHT     0x01
#   define DVD_STRUCT_DISCKEY       0x02
#   define DVD_STRUCT_BCA           0x03
#   define DVD_STRUCT_MANUFACT      0x04
    /* Key formats */
#   define DVD_REPORT_AGID          0x00
#   define DVD_REPORT_CHALLENGE     0x01
#   define DVD_SEND_CHALLENGE       0x01
#   define DVD_REPORT_KEY1          0x02
#   define DVD_SEND_KEY2            0x03
#   define DVD_REPORT_TITLE_KEY     0x04
#   define DVD_REPORT_ASF           0x05
#   define DVD_SEND_RPC             0x06
#   define DVD_REPORT_RPC           0x08
#   define DVD_INVALIDATE_AGID      0x3f
#endif

/*****************************************************************************
 * win32 ioctl specific
 *****************************************************************************/
#if defined( WIN32 )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define IOCTL_DVD_START_SESSION         CTL_CODE(FILE_DEVICE_DVD, 0x0400, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_READ_KEY              CTL_CODE(FILE_DEVICE_DVD, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SEND_KEY              CTL_CODE(FILE_DEVICE_DVD, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_END_SESSION           CTL_CODE(FILE_DEVICE_DVD, 0x0403, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_GET_REGION            CTL_CODE(FILE_DEVICE_DVD, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SEND_KEY2             CTL_CODE(FILE_DEVICE_DVD, 0x0406, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DVD_READ_STRUCTURE        CTL_CODE(FILE_DEVICE_DVD, 0x0450, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(FILE_DEVICE_CONTROLLER, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define DVD_CHALLENGE_KEY_LENGTH        (12 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_BUS_KEY_LENGTH              (8 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_TITLE_KEY_LENGTH            (8 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_DISK_KEY_LENGTH             (2048 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_RPC_KEY_LENGTH              (sizeof(DVD_RPC_KEY) + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_ASF_LENGTH                  (sizeof(DVD_ASF) + sizeof(DVD_COPY_PROTECT_KEY))

#define DVD_COPYRIGHT_MASK              0x00000040
#define DVD_NOT_COPYRIGHTED             0x00000000
#define DVD_COPYRIGHTED                 0x00000040

#define DVD_SECTOR_PROTECT_MASK         0x00000020
#define DVD_SECTOR_NOT_PROTECTED        0x00000000
#define DVD_SECTOR_PROTECTED            0x00000020

#define SCSI_IOCTL_DATA_OUT             0
#define SCSI_IOCTL_DATA_IN              1

typedef ULONG DVD_SESSION_ID, *PDVD_SESSION_ID;

typedef enum DVD_STRUCTURE_FORMAT {
    DvdPhysicalDescriptor,
    DvdCopyrightDescriptor,
    DvdDiskKeyDescriptor,
    DvdBCADescriptor,
    DvdManufacturerDescriptor,
    DvdMaxDescriptor
} DVD_STRUCTURE_FORMAT, *PDVD_STRUCTURE_FORMAT;

typedef struct DVD_READ_STRUCTURE {
    LARGE_INTEGER BlockByteOffset;
    DVD_STRUCTURE_FORMAT Format;
    DVD_SESSION_ID SessionId;
    UCHAR LayerNumber;
} DVD_READ_STRUCTURE, *PDVD_READ_STRUCTURE;

typedef struct _DVD_COPYRIGHT_DESCRIPTOR {
    UCHAR CopyrightProtectionType;
    UCHAR RegionManagementInformation;
    USHORT Reserved;
} DVD_COPYRIGHT_DESCRIPTOR, *PDVD_COPYRIGHT_DESCRIPTOR;

typedef enum
{
    DvdChallengeKey = 0x01,
    DvdBusKey1,
    DvdBusKey2,
    DvdTitleKey,
    DvdAsf,
    DvdSetRpcKey = 0x6,
    DvdGetRpcKey = 0x8,
    DvdDiskKey = 0x80,
    DvdInvalidateAGID = 0x3f
} DVD_KEY_TYPE;

typedef struct _DVD_COPY_PROTECT_KEY
{
    ULONG KeyLength;
    DVD_SESSION_ID SessionId;
    DVD_KEY_TYPE KeyType;
    ULONG KeyFlags;
    union
    {
        struct
        {
            ULONG FileHandle;
            ULONG Reserved;   // used for NT alignment
        };
        LARGE_INTEGER TitleOffset;
    } Parameters;
    UCHAR KeyData[0];
} DVD_COPY_PROTECT_KEY, *PDVD_COPY_PROTECT_KEY;

typedef struct _DVD_ASF
{
    UCHAR Reserved0[3];
    UCHAR SuccessFlag:1;
    UCHAR Reserved1:7;
} DVD_ASF, * PDVD_ASF;

typedef struct _DVD_RPC_KEY
{
    UCHAR UserResetsAvailable:3;
    UCHAR ManufacturerResetsAvailable:3;
    UCHAR TypeCode:2;
    UCHAR RegionMask;
    UCHAR RpcScheme;
    UCHAR Reserved2[1];
} DVD_RPC_KEY, * PDVD_RPC_KEY;

typedef struct _SCSI_PASS_THROUGH_DIRECT
{
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    PVOID DataBuffer;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

/*****************************************************************************
 * win32 aspi specific
 *****************************************************************************/

#define WIN2K               ( GetVersion() < 0x80000000 )
#define ASPI_HAID           0
#define ASPI_TARGET         0
#define DTYPE_CDROM         0x05

#define SENSE_LEN           0x0E
#define SC_GET_DEV_TYPE     0x01
#define SC_EXEC_SCSI_CMD    0x02
#define SC_GET_DISK_INFO    0x06
#define SS_COMP             0x01
#define SS_PENDING          0x00
#define SS_NO_ADAPTERS      0xE8
#define SRB_DIR_IN          0x08
#define SRB_DIR_OUT         0x10
#define SRB_EVENT_NOTIFY    0x40

struct w32_aspidev
{
    long  hASPI;
    short i_sid;
    int   i_blocks;
    long  (*lpSendCommand)( void* );
};

#pragma pack(1)

struct SRB_GetDiskInfo
{
    unsigned char   SRB_Cmd;
    unsigned char   SRB_Status;
    unsigned char   SRB_HaId;
    unsigned char   SRB_Flags;
    unsigned long   SRB_Hdr_Rsvd;
    unsigned char   SRB_Target;
    unsigned char   SRB_Lun;
    unsigned char   SRB_DriveFlags;
    unsigned char   SRB_Int13HDriveInfo;
    unsigned char   SRB_Heads;
    unsigned char   SRB_Sectors;
    unsigned char   SRB_Rsvd1[22];
};

struct SRB_GDEVBlock
{
    unsigned char SRB_Cmd;
    unsigned char SRB_Status;
    unsigned char SRB_HaId;
    unsigned char SRB_Flags;
    unsigned long SRB_Hdr_Rsvd;
    unsigned char SRB_Target;
    unsigned char SRB_Lun;
    unsigned char SRB_DeviceType;
    unsigned char SRB_Rsvd1;
};

struct SRB_ExecSCSICmd
{
    unsigned char   SRB_Cmd;
    unsigned char   SRB_Status;
    unsigned char   SRB_HaId;
    unsigned char   SRB_Flags;
    unsigned long   SRB_Hdr_Rsvd;
    unsigned char   SRB_Target;
    unsigned char   SRB_Lun;
    unsigned short  SRB_Rsvd1;
    unsigned long   SRB_BufLen;
    unsigned char   *SRB_BufPointer;
    unsigned char   SRB_SenseLen;
    unsigned char   SRB_CDBLen;
    unsigned char   SRB_HaStat;
    unsigned char   SRB_TargStat;
    unsigned long   *SRB_PostProc;
    unsigned char   SRB_Rsvd2[20];
    unsigned char   CDBByte[16];
    unsigned char   SenseArea[SENSE_LEN+2];
};

#pragma pack()

#endif

/*****************************************************************************
 * OS2 ioctl specific
 *****************************************************************************/
#if defined( SYS_OS2 )

#define CDROMDISK_EXECMD      0x7A

#define EX_DIRECTION_IN       0x01
#define EX_PLAYING_CHK        0x02

#pragma pack(1)

struct OS2_ExecSCSICmd
{
    unsigned long   id_code;      // 'CD01'
    unsigned short  data_length;  // length of the Data Packet
    unsigned short  cmd_length;   // length of the Command Buffer
    unsigned short  flags;        // flags
    unsigned char   command[16];  // Command Buffer for SCSI command

} OS2_ExecSCSICmd;

#pragma pack()

#endif
