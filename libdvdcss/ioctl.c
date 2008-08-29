/*****************************************************************************
 * ioctl.c: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id$
 *
 * Authors: Markus Kuespert <ltlBeBoy@beosmail.com>
 *          Sam Hocevar <sam@zoy.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *          David Siebörger <drs-videolan@rucus.ru.ac.za>
 *          Alex Strelnikov <lelik@os2.ru>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "config.h"

#include <stdio.h>

#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>

#if defined( WIN32 )
#   include <windows.h>
#   include <winioctl.h>
#elif defined ( SYS_OS2 )
#   define INCL_DOSFILEMGR
#   define INCL_DOSDEVICES
#   define INCL_DOSDEVIOCTL
#   define INCL_DOSERRORS
#   include <os2.h>
#   include <sys/ioctl.h>
#else
#   include <netinet/in.h>
#   include <sys/ioctl.h>
#endif

#ifdef DVD_STRUCT_IN_SYS_CDIO_H
#   include <sys/cdio.h>
#endif
#ifdef DVD_STRUCT_IN_SYS_DVDIO_H
#   include <sys/dvdio.h>
#endif
#ifdef DVD_STRUCT_IN_LINUX_CDROM_H
#   include <linux/cdrom.h>
#endif
#ifdef DVD_STRUCT_IN_DVD_H
#   include <dvd.h>
#endif
#ifdef DVD_STRUCT_IN_BSDI_DVDIOCTL_DVD_H
#   include "bsdi_dvd.h"
#endif
#ifdef SYS_BEOS
#   include <malloc.h>
#   include <scsi.h>
#endif
#ifdef HPUX_SCTL_IO
#   include <sys/scsi.h>
#endif
#ifdef SOLARIS_USCSI
#   include <dlfcn.h>
#   include <unistd.h>
#   include <stropts.h>
#   include <sys/scsi/scsi_types.h>
#   include <sys/scsi/impl/uscsi.h>
#endif
#ifdef DARWIN_DVD_IOCTL
#   include <IOKit/storage/IODVDMediaBSDClient.h>
#endif
#ifdef __QNXNTO__
#   include <sys/mman.h>
#   include <sys/dcmd_cam.h>
#endif

#include "common.h"

#include "ioctl.h"

/*****************************************************************************
 * Local prototypes, BeOS specific
 *****************************************************************************/
#if defined( SYS_BEOS )
static void BeInitRDC ( raw_device_command *, int );
#endif

/*****************************************************************************
 * Local prototypes, HP-UX specific
 *****************************************************************************/
#if defined( HPUX_SCTL_IO )
static void HPUXInitSCTL ( struct sctl_io *sctl_io, int i_type );
#endif

/*****************************************************************************
 * Local prototypes, Solaris specific
 *****************************************************************************/
#if defined( SOLARIS_USCSI )
static void SolarisInitUSCSI( struct uscsi_cmd *p_sc, int i_type );
static int SolarisSendUSCSI( int fd, struct uscsi_cmd *p_sc );
#endif

/*****************************************************************************
 * Local prototypes, win32 (aspi) specific
 *****************************************************************************/
#if defined( WIN32 )
static void WinInitSPTD ( SCSI_PASS_THROUGH_DIRECT *, int );
static void WinInitSSC  ( struct SRB_ExecSCSICmd *, int );
static int  WinSendSSC  ( int, struct SRB_ExecSCSICmd * );
#endif

/*****************************************************************************
 * Local prototypes, QNX specific
 *****************************************************************************/
#if defined( __QNXNTO__ )
static void QNXInitCPT ( CAM_PASS_THRU *, int );
#endif

/*****************************************************************************
 * Local prototypes, OS2 specific
 *****************************************************************************/
#if defined( SYS_OS2 )
static void OS2InitSDC( struct OS2_ExecSCSICmd *, int );
#endif

/*****************************************************************************
 * ioctl_ReadCopyright: check whether the disc is encrypted or not
 *****************************************************************************/
int ioctl_ReadCopyright( int i_fd, int i_layer, int *pi_copyright )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_struct dvd;

    memset( &dvd, 0, sizeof( dvd ) );
    dvd.type = DVD_STRUCT_COPYRIGHT;
    dvd.copyright.layer_num = i_layer;

    i_ret = ioctl( i_fd, DVD_READ_STRUCT, &dvd );

    *pi_copyright = dvd.copyright.cpst;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_struct dvd;

    memset( &dvd, 0, sizeof( dvd ) );
    dvd.format = DVD_STRUCT_COPYRIGHT;
    dvd.layer_num = i_layer;

    i_ret = ioctl( i_fd, DVDIOCREADSTRUCTURE, &dvd );

    *pi_copyright = dvd.cpst;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_READ_DVD_STRUCTURE, 8 );

    rdc.command[ 6 ] = i_layer;
    rdc.command[ 7 ] = DVD_STRUCT_COPYRIGHT;

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    *pi_copyright = p_buffer[ 4 ];

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_READ_DVD_STRUCTURE, 8 );

    sctl_io.cdb[ 6 ] = i_layer;
    sctl_io.cdb[ 7 ] = DVD_STRUCT_COPYRIGHT;

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    *pi_copyright = p_buffer[ 4 ];

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_READ_DVD_STRUCTURE, 8 );

    rs_cdb.cdb_opaque[ 6 ] = i_layer;
    rs_cdb.cdb_opaque[ 7 ] = DVD_STRUCT_COPYRIGHT;

    i_ret = SolarisSendUSCSI(i_fd, &sc);

    if( i_ret < 0 || sc.uscsi_status ) {
        i_ret = -1;
    }

    *pi_copyright = p_buffer[ 4 ];
    /* s->copyright.rmi = p_buffer[ 5 ]; */

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_read_structure_t, DVDCopyrightInfo,
                   kDVDStructureFormatCopyrightInfo );

    dvd.layer = i_layer;

    i_ret = ioctl( i_fd, DKIOCDVDREADSTRUCTURE, &dvd );

    *pi_copyright = dvdbs.copyrightProtectionSystemType;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        INIT_SPTD( GPCMD_READ_DVD_STRUCTURE, 8 );

        /*  When using IOCTL_DVD_READ_STRUCTURE and
            DVD_COPYRIGHT_DESCRIPTOR, CopyrightProtectionType
            seems to be always 6 ???
            To work around this MS bug we try to send a raw scsi command
            instead (if we've got enough privileges to do so). */

        sptd.Cdb[ 6 ] = i_layer;
        sptd.Cdb[ 7 ] = DVD_STRUCT_COPYRIGHT;

        i_ret = SEND_SPTD( i_fd, &sptd, &tmp );

        if( i_ret == 0 )
        {
            *pi_copyright = p_buffer[ 4 ];
        }
    }
    else
    {
        INIT_SSC( GPCMD_READ_DVD_STRUCTURE, 8 );

        ssc.CDBByte[ 6 ] = i_layer;
        ssc.CDBByte[ 7 ] = DVD_STRUCT_COPYRIGHT;

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_copyright = p_buffer[ 4 ];
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_READ_DVD_STRUCTURE, 8 );

    p_cpt->cam_cdb[ 6 ] = i_layer;
    p_cpt->cam_cdb[ 7 ] = DVD_STRUCT_COPYRIGHT;

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    *pi_copyright = p_buffer[4];

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_READ_DVD_STRUCTURE, 8 );

    sdc.command[ 6 ] = i_layer;
    sdc.command[ 7 ] = DVD_STRUCT_COPYRIGHT;

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    *pi_copyright = p_buffer[ 4 ];

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadDiscKey: get the disc key
 *****************************************************************************/
int ioctl_ReadDiscKey( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_struct dvd;

    memset( &dvd, 0, sizeof( dvd ) );
    dvd.type = DVD_STRUCT_DISCKEY;
    dvd.disckey.agid = *pi_agid;
    memset( dvd.disckey.value, 0, DVD_DISCKEY_SIZE );

    i_ret = ioctl( i_fd, DVD_READ_STRUCT, &dvd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, dvd.disckey.value, DVD_DISCKEY_SIZE );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_struct dvd;

    memset( &dvd, 0, sizeof( dvd ) );
    dvd.format = DVD_STRUCT_DISCKEY;
    dvd.agid = *pi_agid;
    memset( dvd.data, 0, DVD_DISCKEY_SIZE );

    i_ret = ioctl( i_fd, DVDIOCREADSTRUCTURE, &dvd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, dvd.data, DVD_DISCKEY_SIZE );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_READ_DVD_STRUCTURE, DVD_DISCKEY_SIZE + 4 );

    rdc.command[ 7 ]  = DVD_STRUCT_DISCKEY;
    rdc.command[ 10 ] = *pi_agid << 6;

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, p_buffer + 4, DVD_DISCKEY_SIZE );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_READ_DVD_STRUCTURE, DVD_DISCKEY_SIZE + 4 );

    sctl_io.cdb[ 7 ]  = DVD_STRUCT_DISCKEY;
    sctl_io.cdb[ 10 ] = *pi_agid << 6;

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, p_buffer + 4, DVD_DISCKEY_SIZE );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_READ_DVD_STRUCTURE, DVD_DISCKEY_SIZE + 4 );

    rs_cdb.cdb_opaque[ 7 ] = DVD_STRUCT_DISCKEY;
    rs_cdb.cdb_opaque[ 10 ] = *pi_agid << 6;

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
        return i_ret;
    }

    memcpy( p_key, p_buffer + 4, DVD_DISCKEY_SIZE );

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_read_structure_t, DVDDiscKeyInfo,
                   kDVDStructureFormatDiscKeyInfo );

    dvd.grantID = *pi_agid;

    i_ret = ioctl( i_fd, DKIOCDVDREADSTRUCTURE, &dvd );

    memcpy( p_key, dvdbs.discKeyStructures, DVD_DISCKEY_SIZE );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_DISK_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_DISK_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdDiskKey;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        memcpy( p_key, key->KeyData, DVD_DISCKEY_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_READ_DVD_STRUCTURE, DVD_DISCKEY_SIZE + 4 );

        ssc.CDBByte[ 7 ]  = DVD_STRUCT_DISCKEY;
        ssc.CDBByte[ 10 ] = *pi_agid << 6;

        i_ret = WinSendSSC( i_fd, &ssc );

        if( i_ret < 0 )
        {
            return i_ret;
        }

        memcpy( p_key, p_buffer + 4, DVD_DISCKEY_SIZE );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_READ_DVD_STRUCTURE, DVD_DISCKEY_SIZE + 4 );

    p_cpt->cam_cdb[ 7 ] = DVD_STRUCT_DISCKEY;
    p_cpt->cam_cdb[ 10 ] = *pi_agid << 6;

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    memcpy( p_key, p_buffer + 4, DVD_DISCKEY_SIZE );

#elif defined ( SYS_OS2 )
    INIT_SSC( GPCMD_READ_DVD_STRUCTURE, DVD_DISCKEY_SIZE + 4 );

    sdc.command[ 7 ]  = DVD_STRUCT_DISCKEY;
    sdc.command[ 10 ] = *pi_agid << 6;

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, p_buffer + 4, DVD_DISCKEY_SIZE );

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadTitleKey: get the title key
 *****************************************************************************/
int ioctl_ReadTitleKey( int i_fd, int *pi_agid, int i_pos, uint8_t *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_TITLE_KEY;
    auth_info.lstk.agid = *pi_agid;
    auth_info.lstk.lba = i_pos;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    memcpy( p_key, auth_info.lstk.title_key, DVD_KEY_SIZE );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_REPORT_TITLE_KEY;
    auth_info.agid = *pi_agid;
    auth_info.lba = i_pos;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    memcpy( p_key, auth_info.keychal, DVD_KEY_SIZE );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 12 );

    rdc.command[ 2 ] = ( i_pos >> 24 ) & 0xff;
    rdc.command[ 3 ] = ( i_pos >> 16 ) & 0xff;
    rdc.command[ 4 ] = ( i_pos >>  8 ) & 0xff;
    rdc.command[ 5 ] = ( i_pos       ) & 0xff;
    rdc.command[ 10 ] = DVD_REPORT_TITLE_KEY | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    memcpy( p_key, p_buffer + 5, DVD_KEY_SIZE );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_REPORT_KEY, 12 );

    sctl_io.cdb[ 2 ] = ( i_pos >> 24 ) & 0xff;
    sctl_io.cdb[ 3 ] = ( i_pos >> 16 ) & 0xff;
    sctl_io.cdb[ 4 ] = ( i_pos >>  8 ) & 0xff;
    sctl_io.cdb[ 5 ] = ( i_pos       ) & 0xff;
    sctl_io.cdb[ 10 ] = DVD_REPORT_TITLE_KEY | (*pi_agid << 6);

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    memcpy( p_key, p_buffer + 5, DVD_KEY_SIZE );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 12 );

    rs_cdb.cdb_opaque[ 2 ] = ( i_pos >> 24 ) & 0xff;
    rs_cdb.cdb_opaque[ 3 ] = ( i_pos >> 16 ) & 0xff;
    rs_cdb.cdb_opaque[ 4 ] = ( i_pos >>  8 ) & 0xff;
    rs_cdb.cdb_opaque[ 5 ] = ( i_pos       ) & 0xff;
    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_TITLE_KEY | (*pi_agid << 6);

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    /* Do we want to return the cp_sec flag perhaps? */
    /* a->lstk.cpm    = (buf[ 4 ] >> 7) & 1; */
    /* a->lstk.cp_sec = (buf[ 4 ] >> 6) & 1; */
    /* a->lstk.cgms   = (buf[ 4 ] >> 4) & 3; */

    memcpy( p_key, p_buffer + 5, DVD_KEY_SIZE );

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_report_key_t, DVDTitleKeyInfo,
                   kDVDKeyFormatTitleKey );

    dvd.address = i_pos;
    dvd.grantID = *pi_agid;
    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM;

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    memcpy( p_key, dvdbs.titleKeyValue, DVD_KEY_SIZE );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_TITLE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_TITLE_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdTitleKey;
        key->KeyFlags   = 0;
        key->Parameters.TitleOffset.QuadPart = (LONGLONG) i_pos *
                                                   2048 /*DVDCSS_BLOCK_SIZE*/;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        memcpy( p_key, key->KeyData, DVD_KEY_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 12 );

        ssc.CDBByte[ 2 ] = ( i_pos >> 24 ) & 0xff;
        ssc.CDBByte[ 3 ] = ( i_pos >> 16 ) & 0xff;
        ssc.CDBByte[ 4 ] = ( i_pos >>  8 ) & 0xff;
        ssc.CDBByte[ 5 ] = ( i_pos       ) & 0xff;
        ssc.CDBByte[ 10 ] = DVD_REPORT_TITLE_KEY | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_key, p_buffer + 5, DVD_KEY_SIZE );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_REPORT_KEY, 12 );

    p_cpt->cam_cdb[ 2 ] = ( i_pos >> 24 ) & 0xff;
    p_cpt->cam_cdb[ 3 ] = ( i_pos >> 16 ) & 0xff;
    p_cpt->cam_cdb[ 4 ] = ( i_pos >>  8 ) & 0xff;
    p_cpt->cam_cdb[ 5 ] = ( i_pos       ) & 0xff;
    p_cpt->cam_cdb[ 10 ] = DVD_REPORT_TITLE_KEY | (*pi_agid << 6);

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    memcpy( p_key, p_buffer + 5, DVD_KEY_SIZE );

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_REPORT_KEY, 12 );

    sdc.command[ 2 ] = ( i_pos >> 24 ) & 0xff;
    sdc.command[ 3 ] = ( i_pos >> 16 ) & 0xff;
    sdc.command[ 4 ] = ( i_pos >>  8 ) & 0xff;
    sdc.command[ 5 ] = ( i_pos       ) & 0xff;
    sdc.command[ 10 ] = DVD_REPORT_TITLE_KEY | (*pi_agid << 6);

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    memcpy( p_key, p_buffer + 5, DVD_KEY_SIZE );

#else
#   error "DVD ioctls are unavailable on this system"

#endif

    return i_ret;
}


/*****************************************************************************
 * ioctl_ReportAgid: get AGID from the drive
 *****************************************************************************/
int ioctl_ReportAgid( int i_fd, int *pi_agid )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_AGID;
    auth_info.lsa.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *pi_agid = auth_info.lsa.agid;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_REPORT_AGID;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    *pi_agid = auth_info.agid;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 8 );

    rdc.command[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    *pi_agid = p_buffer[ 7 ] >> 6;

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_REPORT_KEY, 8 );

    sctl_io.cdb[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    *pi_agid = p_buffer[ 7 ] >> 6;

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 8 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    *pi_agid = p_buffer[ 7 ] >> 6;

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_report_key_t, DVDAuthenticationGrantIDInfo,
                   kDVDKeyFormatAGID_CSS );

    dvd.grantID = *pi_agid;
    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM;

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    *pi_agid = dvdbs.grantID;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        ULONG id;
        DWORD tmp;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_START_SESSION,
                        &tmp, 4, &id, sizeof( id ), &tmp, NULL ) ? 0 : -1;

        *pi_agid = id;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_agid = p_buffer[ 7 ] >> 6;
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_REPORT_KEY, 8 );

    p_cpt->cam_cdb[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    *pi_agid = p_buffer[ 7 ] >> 6;

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_REPORT_KEY, 8 );

    sdc.command[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    *pi_agid = p_buffer[ 7 ] >> 6;

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportChallenge: get challenge from the drive
 *****************************************************************************/
int ioctl_ReportChallenge( int i_fd, int *pi_agid, uint8_t *p_challenge )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_CHALLENGE;
    auth_info.lsc.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    memcpy( p_challenge, auth_info.lsc.chal, DVD_CHALLENGE_SIZE );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_REPORT_CHALLENGE;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    memcpy( p_challenge, auth_info.keychal, DVD_CHALLENGE_SIZE );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 16 );

    rdc.command[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    memcpy( p_challenge, p_buffer + 4, DVD_CHALLENGE_SIZE );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_REPORT_KEY, 16 );

    sctl_io.cdb[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    memcpy( p_challenge, p_buffer + 4, DVD_CHALLENGE_SIZE );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 16 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    memcpy( p_challenge, p_buffer + 4, DVD_CHALLENGE_SIZE );

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_report_key_t, DVDChallengeKeyInfo,
                   kDVDKeyFormatChallengeKey );

    dvd.grantID = *pi_agid;

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    memcpy( p_challenge, dvdbs.challengeKeyValue, DVD_CHALLENGE_SIZE );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_CHALLENGE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_CHALLENGE_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdChallengeKey;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        memcpy( p_challenge, key->KeyData, DVD_CHALLENGE_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 16 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_challenge, p_buffer + 4, DVD_CHALLENGE_SIZE );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_REPORT_KEY, 16 );

    p_cpt->cam_cdb[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    memcpy( p_challenge, p_buffer + 4, DVD_CHALLENGE_SIZE );

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_REPORT_KEY, 16 );

    sdc.command[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    memcpy( p_challenge, p_buffer + 4, DVD_CHALLENGE_SIZE );

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportASF: get ASF from the drive
 *****************************************************************************/
int ioctl_ReportASF( int i_fd, int *pi_remove_me, int *pi_asf )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_ASF;
    auth_info.lsasf.asf = *pi_asf;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *pi_asf = auth_info.lsasf.asf;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_REPORT_ASF;
    auth_info.asf = *pi_asf;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    *pi_asf = auth_info.asf;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 8 );

    rdc.command[ 10 ] = DVD_REPORT_ASF;

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    *pi_asf = p_buffer[ 7 ] & 1;

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_REPORT_KEY, 8 );

    sctl_io.cdb[ 10 ] = DVD_REPORT_ASF;

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    *pi_asf = p_buffer[ 7 ] & 1;

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 8 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_ASF;

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    *pi_asf = p_buffer[ 7 ] & 1;

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_report_key_t, DVDAuthenticationSuccessFlagInfo,
                   kDVDKeyFormatASF );

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    *pi_asf = dvdbs.successFlag;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_ASF_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_ASF_LENGTH;
        key->KeyType    = DvdAsf;
        key->KeyFlags   = 0;

        ((PDVD_ASF)key->KeyData)->SuccessFlag = *pi_asf;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        *pi_asf = ((PDVD_ASF)key->KeyData)->SuccessFlag;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_ASF;

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_asf = p_buffer[ 7 ] & 1;
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_REPORT_KEY, 8 );

    p_cpt->cam_cdb[ 10 ] = DVD_REPORT_ASF;

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    *pi_asf = p_buffer[ 7 ] & 1;

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_REPORT_KEY, 8 );

    sdc.command[ 10 ] = DVD_REPORT_ASF;

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    *pi_asf = p_buffer[ 7 ] & 1;

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportKey1: get the first key from the drive
 *****************************************************************************/
int ioctl_ReportKey1( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_KEY1;
    auth_info.lsk.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    memcpy( p_key, auth_info.lsk.key, DVD_KEY_SIZE );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_REPORT_KEY1;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    memcpy( p_key, auth_info.keychal, DVD_KEY_SIZE );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 12 );

    rdc.command[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    memcpy( p_key, p_buffer + 4, DVD_KEY_SIZE );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_REPORT_KEY, 12 );

    sctl_io.cdb[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    memcpy( p_key, p_buffer + 4, DVD_KEY_SIZE );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 12 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    memcpy( p_key, p_buffer + 4, DVD_KEY_SIZE );

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_report_key_t, DVDKey1Info,
                   kDVDKeyFormatKey1 );

    dvd.grantID = *pi_agid;

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    memcpy( p_key, dvdbs.key1Value, DVD_KEY_SIZE );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_BUS_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_BUS_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdBusKey1;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        memcpy( p_key, key->KeyData, DVD_KEY_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 12 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_key, p_buffer + 4, DVD_KEY_SIZE );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_REPORT_KEY, 12 );

    p_cpt->cam_cdb[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    memcpy( p_key, p_buffer + 4, DVD_KEY_SIZE );

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_REPORT_KEY, 12 );

    sdc.command[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    memcpy( p_key, p_buffer + 4, DVD_KEY_SIZE );

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_InvalidateAgid: invalidate the current AGID
 *****************************************************************************/
int ioctl_InvalidateAgid( int i_fd, int *pi_agid )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_INVALIDATE_AGID;
    auth_info.lsa.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_INVALIDATE_AGID;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 0 );

    rdc.command[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_REPORT_KEY, 0 );

    sctl_io.cdb[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 0 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_send_key_t, DVDAuthenticationGrantIDInfo,
                   kDVDKeyFormatAGID_Invalidate );

    dvd.grantID = *pi_agid;

    i_ret = ioctl( i_fd, DKIOCDVDSENDKEY, &dvd );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_END_SESSION,
                    pi_agid, sizeof( *pi_agid ), NULL, 0, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
#if defined( __MINGW32__ )
        INIT_SSC( GPCMD_REPORT_KEY, 0 );
#else
        INIT_SSC( GPCMD_REPORT_KEY, 1 );

        ssc.SRB_BufLen    = 0;
        ssc.CDBByte[ 8 ]  = 0;
        ssc.CDBByte[ 9 ]  = 0;
#endif

        ssc.CDBByte[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_REPORT_KEY, 0 );

    p_cpt->cam_cdb[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_REPORT_KEY, 1 );

    sdc.data_length = 0;
    sdc.command[ 8 ] = 0;
    sdc.command[ 9 ] = 0;

    sdc.command[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        NULL, 0, &ulDataLen);
#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendChallenge: send challenge to the drive
 *****************************************************************************/
int ioctl_SendChallenge( int i_fd, int *pi_agid, uint8_t *p_challenge )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_HOST_SEND_CHALLENGE;
    auth_info.hsc.agid = *pi_agid;

    memcpy( auth_info.hsc.chal, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_SEND_CHALLENGE;
    auth_info.agid = *pi_agid;

    memcpy( auth_info.keychal, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = ioctl( i_fd, DVDIOCSENDKEY, &auth_info );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_SEND_KEY, 16 );

    rdc.command[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_SEND_KEY, 16 );

    sctl_io.cdb[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_SEND_KEY, 16 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, DVD_CHALLENGE_SIZE );

    if( SolarisSendUSCSI( i_fd, &sc ) < 0 || sc.uscsi_status )
    {
        return -1;
    }

    i_ret = 0;

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_send_key_t, DVDChallengeKeyInfo,
                   kDVDKeyFormatChallengeKey );

    dvd.grantID = *pi_agid;
    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM;

    dvdbs.dataLength[ 1 ] = 0xe;
    memcpy( dvdbs.challengeKeyValue, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = ioctl( i_fd, DKIOCDVDSENDKEY, &dvd );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_CHALLENGE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_CHALLENGE_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdChallengeKey;
        key->KeyFlags   = 0;

        memcpy( key->KeyData, p_challenge, DVD_CHALLENGE_SIZE );

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_SEND_KEY, key,
                 key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 16 );

        ssc.CDBByte[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

        p_buffer[ 1 ] = 0xe;
        memcpy( p_buffer + 4, p_challenge, DVD_CHALLENGE_SIZE );

        i_ret = WinSendSSC( i_fd, &ssc );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_SEND_KEY, 16 );

    p_cpt->cam_cdb[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_SEND_KEY, 16 );

    sdc.command[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = DosDevIOCtl( i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                         &sdc, sizeof(sdc), &ulParamLen,
                         p_buffer, sizeof(p_buffer), &ulDataLen );

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendKey2: send the second key to the drive
 *****************************************************************************/
int ioctl_SendKey2( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_HOST_SEND_KEY2;
    auth_info.hsk.agid = *pi_agid;

    memcpy( auth_info.hsk.key, p_key, DVD_KEY_SIZE );

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_SEND_KEY2;
    auth_info.agid = *pi_agid;

    memcpy( auth_info.keychal, p_key, DVD_KEY_SIZE );

    i_ret = ioctl( i_fd, DVDIOCSENDKEY, &auth_info );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_SEND_KEY, 12 );

    rdc.command[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, DVD_KEY_SIZE );

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_SEND_KEY, 12 );

    sctl_io.cdb[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, DVD_KEY_SIZE );

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_SEND_KEY, 12 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, DVD_KEY_SIZE );

    if( SolarisSendUSCSI( i_fd, &sc ) < 0 || sc.uscsi_status )
    {
        return -1;
    }

    i_ret = 0;

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_send_key_t, DVDKey2Info,
                   kDVDKeyFormatKey2 );

    dvd.grantID = *pi_agid;
    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM;

    dvdbs.dataLength[ 1 ] = 0xa;
    memcpy( dvdbs.key2Value, p_key, DVD_KEY_SIZE );

    i_ret = ioctl( i_fd, DKIOCDVDSENDKEY, &dvd );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_BUS_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_BUS_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdBusKey2;
        key->KeyFlags   = 0;

        memcpy( key->KeyData, p_key, DVD_KEY_SIZE );

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_SEND_KEY, key,
                 key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 12 );

        ssc.CDBByte[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

        p_buffer[ 1 ] = 0xa;
        memcpy( p_buffer + 4, p_key, DVD_KEY_SIZE );

        i_ret = WinSendSSC( i_fd, &ssc );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_SEND_KEY, 12 );

    p_cpt->cam_cdb[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, DVD_KEY_SIZE );

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_SEND_KEY, 12 );

    sdc.command[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, DVD_KEY_SIZE );

    i_ret = DosDevIOCtl( i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                         &sdc, sizeof(sdc), &ulParamLen,
                         p_buffer, sizeof(p_buffer), &ulDataLen );

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportRPC: get RPC status for the drive
 *****************************************************************************/
int ioctl_ReportRPC( int i_fd, int *p_type, int *p_mask, int *p_scheme )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT ) && defined( DVD_LU_SEND_RPC_STATE )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_RPC_STATE;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *p_type = auth_info.lrpcs.type;
    *p_mask = auth_info.lrpcs.region_mask;
    *p_scheme = auth_info.lrpcs.rpc_scheme;

#elif defined( HAVE_LINUX_DVD_STRUCT )
    /* FIXME: OpenBSD doesn't know this */
    i_ret = -1;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_REPORT_RPC;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    *p_type = auth_info.reg_type;
    *p_mask = auth_info.region; // ??
    *p_scheme = auth_info.rpc_scheme;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 8 );

    rdc.command[ 10 ] = DVD_REPORT_RPC;

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    *p_type = p_buffer[ 4 ] >> 6;
    *p_mask = p_buffer[ 5 ];
    *p_scheme = p_buffer[ 6 ];

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_REPORT_KEY, 8 );

    sctl_io.cdb[ 10 ] = DVD_REPORT_RPC;

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

    *p_type = p_buffer[ 4 ] >> 6;
    *p_mask = p_buffer[ 5 ];
    *p_scheme = p_buffer[ 6 ];

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 8 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_RPC;

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    *p_type = p_buffer[ 4 ] >> 6;
    *p_mask = p_buffer[ 5 ];
    *p_scheme = p_buffer[ 6 ];

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_report_key_t, DVDRegionPlaybackControlInfo,
                   kDVDKeyFormatRegionState );

    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM;

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    *p_type = dvdbs.typeCode;
    *p_mask = dvdbs.driveRegion;
    *p_scheme = dvdbs.rpcScheme;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_RPC_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_RPC_KEY_LENGTH;
        key->KeyType    = DvdGetRpcKey;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        *p_type = ((PDVD_RPC_KEY)key->KeyData)->TypeCode;
        *p_mask = ((PDVD_RPC_KEY)key->KeyData)->RegionMask;
        *p_scheme = ((PDVD_RPC_KEY)key->KeyData)->RpcScheme;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_RPC;

        i_ret = WinSendSSC( i_fd, &ssc );

        *p_type = p_buffer[ 4 ] >> 6;
        *p_mask = p_buffer[ 5 ];
        *p_scheme = p_buffer[ 6 ];
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_REPORT_KEY, 8 );

    p_cpt->cam_cdb[ 10 ] = DVD_REPORT_RPC;

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

    *p_type = p_buffer[ 4 ] >> 6;
    *p_mask = p_buffer[ 5 ];
    *p_scheme = p_buffer[ 6 ];

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_REPORT_KEY, 8 );

    sdc.command[ 10 ] = DVD_REPORT_RPC;

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    *p_type = p_buffer[ 4 ] >> 6;
    *p_mask = p_buffer[ 5 ];
    *p_scheme = p_buffer[ 6 ];

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendRPC: set RPC status for the drive
 *****************************************************************************/
int ioctl_SendRPC( int i_fd, int i_pdrc )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT ) && defined( DVD_HOST_SEND_RPC_STATE )
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_HOST_SEND_RPC_STATE;
    auth_info.hrpcs.pdrc = i_pdrc;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

#elif defined( HAVE_LINUX_DVD_STRUCT )
    /* FIXME: OpenBSD doesn't know this */
    i_ret = -1;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.format = DVD_SEND_RPC;
    auth_info.region = i_pdrc;

    i_ret = ioctl( i_fd, DVDIOCSENDKEY, &auth_info );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_SEND_KEY, 8 );

    rdc.command[ 10 ] = DVD_SEND_RPC;

    p_buffer[ 1 ] = 6;
    p_buffer[ 4 ] = i_pdrc;

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

#elif defined( HPUX_SCTL_IO )
    INIT_SCTL_IO( GPCMD_SEND_KEY, 8 );

    sctl_io.cdb[ 10 ] = DVD_SEND_RPC;

    p_buffer[ 1 ] = 6;
    p_buffer[ 4 ] = i_pdrc;

    i_ret = ioctl( i_fd, SIOC_IO, &sctl_io );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_SEND_KEY, 8 );

    rs_cdb.cdb_opaque[ 10 ] = DVD_SEND_RPC;

    p_buffer[ 1 ] = 6;
    p_buffer[ 4 ] = i_pdrc;

    i_ret = SolarisSendUSCSI( i_fd, &sc );

    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

#elif defined( DARWIN_DVD_IOCTL )
    INIT_DVDIOCTL( dk_dvd_send_key_t, DVDRegionPlaybackControlInfo,
                   kDVDKeyFormatSetRegion );

    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM;
    dvdbs.driveRegion = i_pdrc;

    i_ret = ioctl( i_fd, DKIOCDVDSENDKEY, &dvd );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/2k/XP */
    {
        INIT_SPTD( GPCMD_SEND_KEY, 8 );

        sptd.Cdb[ 10 ] = DVD_SEND_RPC;

        p_buffer[ 1 ] = 6;
        p_buffer[ 4 ] = i_pdrc;

        i_ret = SEND_SPTD( i_fd, &sptd, &tmp );
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_SEND_RPC;

        p_buffer[ 1 ] = 6;
        p_buffer[ 4 ] = i_pdrc;

        i_ret = WinSendSSC( i_fd, &ssc );
    }

#elif defined( __QNXNTO__ )

    INIT_CPT( GPCMD_SEND_KEY, 8 );

    p_cpt->cam_cdb[ 10 ] = DVD_SEND_RPC;

    p_buffer[ 1 ] = 6;
    p_buffer[ 4 ] = i_pdrc;

    i_ret = devctl(i_fd, DCMD_CAM_PASS_THRU, p_cpt, structSize, NULL);

#elif defined( SYS_OS2 )
    INIT_SSC( GPCMD_SEND_KEY, 8 );

    sdc.command[ 10 ] = DVD_SEND_RPC;

    p_buffer[ 1 ] = 6;
    p_buffer[ 4 ] = i_pdrc;

    i_ret = DosDevIOCtl( i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                         &sdc, sizeof(sdc), &ulParamLen,
                         p_buffer, sizeof(p_buffer), &ulDataLen );

#else
#   error "DVD ioctls are unavailable on this system"

#endif
    return i_ret;
}

/* Local prototypes */

#if defined( SYS_BEOS )
/*****************************************************************************
 * BeInitRDC: initialize a RDC structure for the BeOS kernel
 *****************************************************************************
 * This function initializes a BeOS raw device command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void BeInitRDC( raw_device_command *p_rdc, int i_type )
{
    memset( p_rdc->data, 0, p_rdc->data_length );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            /* leave the flags to 0 */
            break;

        case GPCMD_READ_DVD_STRUCTURE: case GPCMD_REPORT_KEY:
    p_rdc->flags = B_RAW_DEVICE_DATA_IN; break; }

    p_rdc->command[ 0 ]      = i_type;

    p_rdc->command[ 8 ]      = (p_rdc->data_length >> 8) & 0xff;
    p_rdc->command[ 9 ]      =  p_rdc->data_length       & 0xff;
    p_rdc->command_length    = 12;

    p_rdc->sense_data        = NULL;
    p_rdc->sense_data_length = 0;

    p_rdc->timeout           = 1000000;
}
#endif

#if defined( HPUX_SCTL_IO )
/*****************************************************************************
 * HPUXInitSCTL: initialize a sctl_io structure for the HP-UX kernel
 *****************************************************************************
 * This function initializes a HP-UX command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void HPUXInitSCTL( struct sctl_io *sctl_io, int i_type )
{
    memset( sctl_io->data, 0, sctl_io->data_length );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            /* leave the flags to 0 */
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            sctl_io->flags = SCTL_READ;
            break;
    }

    sctl_io->cdb[ 0 ]        = i_type;

    sctl_io->cdb[ 8 ]        = (sctl_io->data_length >> 8) & 0xff;
    sctl_io->cdb[ 9 ]        =  sctl_io->data_length       & 0xff;
    sctl_io->cdb_length      = 12;

    sctl_io->max_msecs       = 1000000;
}
#endif

#if defined( SOLARIS_USCSI )
/*****************************************************************************
 * SolarisInitUSCSI: initialize a USCSICMD structure for the Solaris kernel
 *****************************************************************************
 * This function initializes a Solaris userspace scsi command structure for
 * future use, either a read command or a write command.
 *****************************************************************************/
static void SolarisInitUSCSI( struct uscsi_cmd *p_sc, int i_type )
{
    union scsi_cdb *rs_cdb;
    memset( p_sc->uscsi_cdb, 0, sizeof( union scsi_cdb ) );
    memset( p_sc->uscsi_bufaddr, 0, p_sc->uscsi_buflen );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_sc->uscsi_flags = USCSI_ISOLATE | USCSI_WRITE;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_sc->uscsi_flags = USCSI_ISOLATE | USCSI_READ;
            break;
    }

    rs_cdb = (union scsi_cdb *)p_sc->uscsi_cdb;

    rs_cdb->scc_cmd = i_type;

    rs_cdb->cdb_opaque[ 8 ] = (p_sc->uscsi_buflen >> 8) & 0xff;
    rs_cdb->cdb_opaque[ 9 ] =  p_sc->uscsi_buflen       & 0xff;
    p_sc->uscsi_cdblen = 12;

    USCSI_TIMEOUT( p_sc, 15 );
}

/*****************************************************************************
 * SolarisSendUSCSI: send a USCSICMD structure to the Solaris kernel
 * for execution
 *****************************************************************************
 * When available, this function uses the function smedia_uscsi_cmd()
 * from Solaris' libsmedia library (Solaris 9 or newer) to execute the
 * USCSI command.  smedia_uscsi_cmd() allows USCSI commands for
 * non-root users on removable media devices on Solaris 9; sending the
 * USCSI command directly to the device using the USCSICMD ioctl fails
 * with an EPERM error on Solaris 9.
 *
 * The code will fall back to the USCSICMD ioctl method, when
 * libsmedia.so is not available or does not export the
 * smedia_uscsi_cmd() function (on Solaris releases upto and including
 * Solaris 8). Fortunatelly, on these old releases non-root users are
 * allowed to perform USCSICMD ioctls on removable media devices.
 *****************************************************************************/
static int SolarisSendUSCSI( int i_fd, struct uscsi_cmd *p_sc )
{
    void *p_handle;

    /* We use static variables to keep track of the libsmedia symbols, which
     * is harmless even in a multithreaded program because the library and
     * its symbols will always be mapped at the same address. */
    static int b_tried = 0;
    static int b_have_sm = 0;
    static void * (*p_get_handle) ( int32_t );
    static int (*p_uscsi_cmd) ( void *, struct uscsi_cmd * );
    static int (*p_release_handle) ( void * );

    if( !b_tried )
    {
        void *p_lib;

        p_lib = dlopen( "libsmedia.so", RTLD_NOW );
        if( p_lib )
        {
            p_get_handle = dlsym( p_lib, "smedia_get_handle" );
            p_uscsi_cmd = dlsym( p_lib, "smedia_uscsi_cmd" );
            p_release_handle = dlsym( p_lib, "smedia_release_handle" );

            if( p_get_handle && p_uscsi_cmd && p_release_handle )
            {
                b_have_sm = 1;
            }
            else
            {
                dlclose( p_lib );
            }
        }

        b_tried = 1;
    }

    if( b_have_sm && (p_handle = p_get_handle(i_fd)) )
    {
        int i_ret = p_uscsi_cmd( p_handle, p_sc );
        p_release_handle( p_handle );
        return i_ret;
    }

    return ioctl( i_fd, USCSICMD, p_sc );
}
#endif

#if defined( WIN32 )
/*****************************************************************************
 * WinInitSPTD: initialize a sptd structure
 *****************************************************************************
 * This function initializes a SCSI pass through command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void WinInitSPTD( SCSI_PASS_THROUGH_DIRECT *p_sptd, int i_type )
{
    memset( p_sptd->DataBuffer, 0, p_sptd->DataTransferLength );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_sptd->DataIn = SCSI_IOCTL_DATA_OUT;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_sptd->DataIn = SCSI_IOCTL_DATA_IN;
            break;
    }

    p_sptd->Cdb[ 0 ] = i_type;
    p_sptd->Cdb[ 8 ] = (uint8_t)(p_sptd->DataTransferLength >> 8) & 0xff;
    p_sptd->Cdb[ 9 ] = (uint8_t) p_sptd->DataTransferLength       & 0xff;
    p_sptd->CdbLength = 12;

    p_sptd->TimeOutValue = 2;
}

/*****************************************************************************
 * WinInitSSC: initialize a ssc structure for the win32 aspi layer
 *****************************************************************************
 * This function initializes a ssc raw device command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void WinInitSSC( struct SRB_ExecSCSICmd *p_ssc, int i_type )
{
    memset( p_ssc->SRB_BufPointer, 0, p_ssc->SRB_BufLen );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_ssc->SRB_Flags = SRB_DIR_OUT;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_ssc->SRB_Flags = SRB_DIR_IN;
            break;
    }

    p_ssc->SRB_Cmd      = SC_EXEC_SCSI_CMD;
    p_ssc->SRB_Flags    |= SRB_EVENT_NOTIFY;

    p_ssc->CDBByte[ 0 ] = i_type;

    p_ssc->CDBByte[ 8 ] = (uint8_t)(p_ssc->SRB_BufLen >> 8) & 0xff;
    p_ssc->CDBByte[ 9 ] = (uint8_t) p_ssc->SRB_BufLen       & 0xff;
    p_ssc->SRB_CDBLen   = 12;

    p_ssc->SRB_SenseLen = SENSE_LEN;
}

/*****************************************************************************
 * WinSendSSC: send a ssc structure to the aspi layer
 *****************************************************************************/
static int WinSendSSC( int i_fd, struct SRB_ExecSCSICmd *p_ssc )
{
    HANDLE hEvent = NULL;
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;

    hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( hEvent == NULL )
    {
        return -1;
    }

    p_ssc->SRB_PostProc  = hEvent;
    p_ssc->SRB_HaId      = LOBYTE( fd->i_sid );
    p_ssc->SRB_Target    = HIBYTE( fd->i_sid );

    ResetEvent( hEvent );
    if( fd->lpSendCommand( (void*) p_ssc ) == SS_PENDING )
        WaitForSingleObject( hEvent, INFINITE );

    CloseHandle( hEvent );

    return p_ssc->SRB_Status == SS_COMP ? 0 : -1;
}
#endif

#if defined( __QNXNTO__ )
/*****************************************************************************
 * QNXInitCPT: initialize a CPT structure for QNX Neutrino
 *****************************************************************************
 * This function initializes a cpt command structure for future use,
 * either a read command or a write command.
 *****************************************************************************/
static void QNXInitCPT( CAM_PASS_THRU * p_cpt, int i_type )
{
    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_cpt->cam_flags = CAM_DIR_OUT;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_cpt->cam_flags = CAM_DIR_IN;
            break;
    }

    p_cpt->cam_cdb[0] = i_type;

    p_cpt->cam_cdb[ 8 ] = (p_cpt->cam_dxfer_len >> 8) & 0xff;
    p_cpt->cam_cdb[ 9 ] =  p_cpt->cam_dxfer_len       & 0xff;
    p_cpt->cam_cdb_len = 12;

    p_cpt->cam_timeout = CAM_TIME_DEFAULT;
}
#endif

#if defined( SYS_OS2 )
/*****************************************************************************
 * OS2InitSDC: initialize a SDC structure for the Execute SCSI-command
 *****************************************************************************
 * This function initializes a OS2 'execute SCSI command' structure for
 * future use, either a read command or a write command.
 *****************************************************************************/
static void OS2InitSDC( struct OS2_ExecSCSICmd *p_sdc, int i_type )
{
    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_sdc->flags = 0;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_sdc->flags = EX_DIRECTION_IN;
            break;
    }

    p_sdc->command[ 0 ] = i_type;
    p_sdc->command[ 8 ] = (p_sdc->data_length >> 8) & 0xff;
    p_sdc->command[ 9 ] = p_sdc->data_length        & 0xff;
    p_sdc->id_code      = 0x31304443;    // 'CD01'
    p_sdc->cmd_length   = 12;
}
#endif
