/*
 * This code is based on dvdudf by:
 *   Christian Wolff <scarabaeus@convergence.de>.
 *
 * Modifications by:
 *   Billy Biggs <vektor@dumbterm.net>.
 *
 * dvdudf: parse and read the UDF volume information of a DVD Video
 * Copyright (C) 1999 Christian Wolff for convergence integrated media
 * GmbH The author can be reached at scarabaeus@convergence.de, the
 * project's page is at http://linuxtv.org/dvd/
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  Or, point your browser to
 * http://www.gnu.org/copyleft/gpl.html
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#include "dvd_reader.h"
#include "dvd_udf.h"

/* Private but located in/shared with dvd_reader.c */
extern int DVDReadBlocksUDFRaw( dvd_reader_t *device, uint32_t lb_number,
				size_t block_count, unsigned char *data, 
				int encrypted );

/* It's required to either fail or deliver all the blocks asked for. */
static int DVDReadLBUDF( dvd_reader_t *device, uint32_t lb_number,
			 size_t block_count, unsigned char *data, 
			 int encrypted )
{
  int ret;
  size_t count = block_count;
  
  while(count > 0) {
    
    ret = DVDReadBlocksUDFRaw(device, lb_number, count, data, encrypted);
        
    if(ret <= 0) {
      /* One of the reads failed or nothing more to read, too bad.
       * We won't even bother returning the reads that went ok. */
      return ret;
    }
    
    count -= (size_t)ret;
    lb_number += (uint32_t)ret;
  }

  return block_count;
}


#ifndef NULL
#define NULL ((void *)0)
#endif

struct Partition {
    int valid;
    char VolumeDesc[128];
    uint16_t Flags;
    uint16_t Number;
    char Contents[32];
    uint32_t AccessType;
    uint32_t Start;
    uint32_t Length;
};

struct AD {
    uint32_t Location;
    uint32_t Length;
    uint8_t  Flags;
    uint16_t Partition;
};

/* For direct data access, LSB first */
#define GETN1(p) ((uint8_t)data[p])
#define GETN2(p) ((uint16_t)data[p] | ((uint16_t)data[(p) + 1] << 8))
#define GETN3(p) ((uint32_t)data[p] | ((uint32_t)data[(p) + 1] << 8) \
		  | ((uint32_t)data[(p) + 2] << 16))
#define GETN4(p) ((uint32_t)data[p] \
		  | ((uint32_t)data[(p) + 1] << 8) \
		  | ((uint32_t)data[(p) + 2] << 16) \
		  | ((uint32_t)data[(p) + 3] << 24))
/* This is wrong with regard to endianess */
#define GETN(p, n, target) memcpy(target, &data[p], n)

static int Unicodedecode( uint8_t *data, int len, char *target ) 
{
    int p = 1, i = 0;

    if( ( data[ 0 ] == 8 ) || ( data[ 0 ] == 16 ) ) do {
        if( data[ 0 ] == 16 ) p++;  /* Ignore MSB of unicode16 */
        if( p < len ) {
            target[ i++ ] = data[ p++ ];
        }
    } while( p < len );

    target[ i ] = '\0';
    return 0;
}

static int UDFDescriptor( uint8_t *data, uint16_t *TagID ) 
{
    *TagID = GETN2(0);
    // TODO: check CRC 'n stuff
    return 0;
}

static int UDFExtentAD( uint8_t *data, uint32_t *Length, uint32_t *Location ) 
{
    *Length   = GETN4(0);
    *Location = GETN4(4);
    return 0;
}

static int UDFShortAD( uint8_t *data, struct AD *ad, 
		       struct Partition *partition ) 
{
    ad->Length = GETN4(0);
    ad->Flags = ad->Length >> 30;
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GETN4(4);
    ad->Partition = partition->Number; // use number of current partition
    return 0;
}

static int UDFLongAD( uint8_t *data, struct AD *ad )
{
    ad->Length = GETN4(0);
    ad->Flags = ad->Length >> 30;
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GETN4(4);
    ad->Partition = GETN2(8);
    //GETN(10, 6, Use);
    return 0;
}

static int UDFExtAD( uint8_t *data, struct AD *ad )
{
    ad->Length = GETN4(0);
    ad->Flags = ad->Length >> 30;
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GETN4(12);
    ad->Partition = GETN2(16);
    //GETN(10, 6, Use);
    return 0;
}

static int UDFICB( uint8_t *data, uint8_t *FileType, uint16_t *Flags )
{
    *FileType = GETN1(11);
    *Flags = GETN2(18);
    return 0;
}


static int UDFPartition( uint8_t *data, uint16_t *Flags, uint16_t *Number,
			 char *Contents, uint32_t *Start, uint32_t *Length )
{
    *Flags = GETN2(20);
    *Number = GETN2(22);
    GETN(24, 32, Contents);
    *Start = GETN4(188);
    *Length = GETN4(192);
    return 0;
}

/**
 * Reads the volume descriptor and checks the parameters.  Returns 0 on OK, 1
 * on error.
 */
static int UDFLogVolume( uint8_t *data, char *VolumeDescriptor )
{
    uint32_t lbsize, MT_L, N_PM;
    Unicodedecode(&data[84], 128, VolumeDescriptor);
    lbsize = GETN4(212);  // should be 2048
    MT_L = GETN4(264);    // should be 6
    N_PM = GETN4(268);    // should be 1
    if (lbsize != DVD_VIDEO_LB_LEN) return 1;
    return 0;
}

static int UDFFileEntry( uint8_t *data, uint8_t *FileType, 
			 struct Partition *partition, struct AD *ad )
{
    uint16_t flags;
    uint32_t L_EA, L_AD;
    unsigned int p;

    UDFICB( &data[ 16 ], FileType, &flags );
   
    /* Init ad for an empty file (i.e. there isn't a AD, L_AD == 0 ) */
    ad->Length = GETN4( 60 ); // Really 8 bytes a 56
    ad->Flags = 0;
    ad->Location = 0; // what should we put here? 
    ad->Partition = partition->Number; // use number of current partition

    L_EA = GETN4( 168 );
    L_AD = GETN4( 172 );
    p = 176 + L_EA;
    while( p < 176 + L_EA + L_AD ) {
        switch( flags & 0x0007 ) {
            case 0: UDFShortAD( &data[ p ], ad, partition ); p += 8;  break;
            case 1: UDFLongAD( &data[ p ], ad );  p += 16; break;
            case 2: UDFExtAD( &data[ p ], ad );   p += 20; break;
            case 3:
                switch( L_AD ) {
                    case 8:  UDFShortAD( &data[ p ], ad, partition ); break;
                    case 16: UDFLongAD( &data[ p ], ad );  break;
                    case 20: UDFExtAD( &data[ p ], ad );   break;
                }
                p += L_AD;
                break;
            default:
                p += L_AD; break;
        }
    }
    return 0;
}

static int UDFFileIdentifier( uint8_t *data, uint8_t *FileCharacteristics,
			      char *FileName, struct AD *FileICB )
{
    uint8_t L_FI;
    uint16_t L_IU;

    *FileCharacteristics = GETN1(18);
    L_FI = GETN1(19);
    UDFLongAD(&data[20], FileICB);
    L_IU = GETN2(36);
    if (L_FI) Unicodedecode(&data[38 + L_IU], L_FI, FileName);
    else FileName[0] = '\0';
    return 4 * ((38 + L_FI + L_IU + 3) / 4);
}

/**
 * Maps ICB to FileAD
 * ICB: Location of ICB of directory to scan
 * FileType: Type of the file
 * File: Location of file the ICB is pointing to
 * return 1 on success, 0 on error;
 */
static int UDFMapICB( dvd_reader_t *device, struct AD ICB, uint8_t *FileType,
		      struct Partition *partition, struct AD *File ) 
{
    uint8_t LogBlock[DVD_VIDEO_LB_LEN];
    uint32_t lbnum;
    uint16_t TagID;

    lbnum = partition->Start + ICB.Location;
    do {
        if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
            TagID = 0;
        } else {
            UDFDescriptor( LogBlock, &TagID );
        }

        if( TagID == 261 ) {
            UDFFileEntry( LogBlock, FileType, partition, File );
            return 1;
        };
    } while( ( lbnum <= partition->Start + ICB.Location + ( ICB.Length - 1 )
             / DVD_VIDEO_LB_LEN ) && ( TagID != 261 ) );

    return 0;
}

/**
 * Dir: Location of directory to scan
 * FileName: Name of file to look for
 * FileICB: Location of ICB of the found file
 * return 1 on success, 0 on error;
 */
static int UDFScanDir( dvd_reader_t *device, struct AD Dir, char *FileName,
                       struct Partition *partition, struct AD *FileICB ) 
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];
    uint8_t directory[ 2 * DVD_VIDEO_LB_LEN ];
    uint32_t lbnum;
    uint16_t TagID;
    uint8_t filechar;
    unsigned int p;

    /* Scan dir for ICB of file */
    lbnum = partition->Start + Dir.Location;

    if( DVDReadLBUDF( device, lbnum, 2, directory, 0 ) <= 0 ) {
        return 0;
    }

    p = 0;
    while( p < Dir.Length ) {
        if( p > DVD_VIDEO_LB_LEN ) {
            ++lbnum;
            p -= DVD_VIDEO_LB_LEN;
            Dir.Length -= DVD_VIDEO_LB_LEN;
            if( DVDReadLBUDF( device, lbnum, 2, directory, 0 ) <= 0 ) {
                return 0;
            }
        }
        UDFDescriptor( &directory[ p ], &TagID );
        if( TagID == 257 ) {
            p += UDFFileIdentifier( &directory[ p ], &filechar,
                                    filename, FileICB );
            if( !strcasecmp( FileName, filename ) ) {
                return 1;
            }
        } else {
            return 0;
        }
    }

    return 0;
}

/**
 * Looks for partition on the disc.  Returns 1 if partition found, 0 on error.
 *   partnum: Number of the partition, starting at 0.
 *   part: structure to fill with the partition information
 */
static int UDFFindPartition( dvd_reader_t *device, int partnum,
			     struct Partition *part ) 
{
    uint8_t LogBlock[ DVD_VIDEO_LB_LEN ], Anchor[ DVD_VIDEO_LB_LEN ];
    uint32_t lbnum, MVDS_location, MVDS_length;
    uint16_t TagID;
    uint32_t lastsector;
    int i, terminate, volvalid;

    /* Find Anchor */
    lastsector = 0;
    lbnum = 256;   /* Try #1, prime anchor */
    terminate = 0;

    for(;;) {
        if( DVDReadLBUDF( device, lbnum, 1, Anchor, 0 ) > 0 ) {
            UDFDescriptor( Anchor, &TagID );
        } else {
            TagID = 0;
        }
        if (TagID != 2) {
            /* Not an anchor */
            if( terminate ) return 0; /* Final try failed */

            if( lastsector ) {

                /* We already found the last sector.  Try #3, alternative
                 * backup anchor.  If that fails, don't try again.
                 */
                lbnum = lastsector;
                terminate = 1;
            } else {
                /* TODO: Find last sector of the disc (this is optional). */
                if( lastsector ) {
                    /* Try #2, backup anchor */
                    lbnum = lastsector - 256;
                } else {
                    /* Unable to find last sector */
                    return 0;
                }
            }
        } else {
            /* It's an anchor! We can leave */
            break;
        }
    }
    /* Main volume descriptor */
    UDFExtentAD( &Anchor[ 16 ], &MVDS_length, &MVDS_location );
	
    part->valid = 0;
    volvalid = 0;
    part->VolumeDesc[ 0 ] = '\0';
    i = 1;
    do {
        /* Find Volume Descriptor */
        lbnum = MVDS_location;
        do {

            if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
                TagID = 0;
            } else {
                UDFDescriptor( LogBlock, &TagID );
            }

            if( ( TagID == 5 ) && ( !part->valid ) ) {
                /* Partition Descriptor */
                UDFPartition( LogBlock, &part->Flags, &part->Number,
                              part->Contents, &part->Start, &part->Length );
                part->valid = ( partnum == part->Number );
            } else if( ( TagID == 6 ) && ( !volvalid ) ) {
                /* Logical Volume Descriptor */
                if( UDFLogVolume( LogBlock, part->VolumeDesc ) ) {  
                    /* TODO: sector size wrong! */
                } else {
                    volvalid = 1;
                }
            }

        } while( ( lbnum <= MVDS_location + ( MVDS_length - 1 )
                 / DVD_VIDEO_LB_LEN ) && ( TagID != 8 )
                 && ( ( !part->valid ) || ( !volvalid ) ) );

        if( ( !part->valid) || ( !volvalid ) ) {
            /* Backup volume descriptor */
            UDFExtentAD( &Anchor[ 24 ], &MVDS_length, &MVDS_location );
        }
    } while( i-- && ( ( !part->valid ) || ( !volvalid ) ) );

    /* We only care for the partition, not the volume */
    return part->valid;
}

uint32_t UDFFindFile( dvd_reader_t *device, char *filename,
		      uint32_t *filesize )
{
    uint8_t LogBlock[ DVD_VIDEO_LB_LEN ];
    uint32_t lbnum;
    uint16_t TagID;
    struct Partition partition;
    struct AD RootICB, File, ICB;
    char tokenline[ MAX_UDF_FILE_NAME_LEN ];
    char *token;
    uint8_t filetype;
	
    *filesize = 0;
    tokenline[0] = '\0';
    strcat( tokenline, filename );

    /* Find partition, 0 is the standard location for DVD Video.*/
    if( !UDFFindPartition( device, 0, &partition ) ) return 0;

    /* Find root dir ICB */
    lbnum = partition.Start;
    do {
        if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
            TagID = 0;
        } else {
            UDFDescriptor( LogBlock, &TagID );
        }

        /* File Set Descriptor */
        if( TagID == 256 ) {  // File Set Descriptor
            UDFLongAD( &LogBlock[ 400 ], &RootICB );
        }
    } while( ( lbnum < partition.Start + partition.Length )
             && ( TagID != 8 ) && ( TagID != 256 ) );

    /* Sanity checks. */
    if( TagID != 256 ) return 0;
    if( RootICB.Partition != 0 ) return 0;
	
    /* Find root dir */
    if( !UDFMapICB( device, RootICB, &filetype, &partition, &File ) ) return 0;
    if( filetype != 4 ) return 0;  /* Root dir should be dir */

    /* Tokenize filepath */
    token = strtok(tokenline, "/");
    while( token != NULL ) {
        if( !UDFScanDir( device, File, token, &partition, &ICB ) ) return 0;
        if( !UDFMapICB( device, ICB, &filetype, &partition, &File ) ) return 0;
        token = strtok( NULL, "/" );
    }
    
    /* Sanity check. */
    if( File.Partition != 0 ) return 0;
   
    *filesize = File.Length;
    /* Hack to not return partition.Start for empty files. */
    if( !File.Location )
      return 0;
    else
      return partition.Start + File.Location;
}
