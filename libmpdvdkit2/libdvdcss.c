/* libdvdcss.c: DVD reading library.
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * Copyright (C) 1998-2002 VideoLAN
 * $Id$
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
 */

/** 
 * \mainpage libdvdcss developer documentation
 *
 * \section intro Introduction
 *
 * \e libdvdcss is a simple library designed for accessing DVDs like a block
 * device without having to bother about the decryption. The important features
 * are:
 * \li portability: currently supported platforms are GNU/Linux, FreeBSD,
 *     NetBSD, OpenBSD, BSD/OS, BeOS, Windows 95/98, Windows NT/2000, MacOS X,
 *     Solaris, HP-UX and OS/2.
 * \li adaptability: unlike most similar projects, libdvdcss doesn't require
 *     the region of your drive to be set and will try its best to read from
 *     the disc even in the case of a region mismatch.
 * \li simplicity: a DVD player can be built around the \e libdvdcss API using
 *     no more than 4 or 5 library calls.
 *
 * \e libdvdcss is free software, released under the General Public License.
 * This ensures that \e libdvdcss remains free and used only with free
 * software.
 *
 * \section api The libdvdcss API
 *
 * The complete \e libdvdcss programming interface is documented in the
 * dvdcss.h file.
 *
 * \section env Environment variables
 *
 * Some environment variables can be used to change the behaviour of
 * \e libdvdcss without having to modify the program which uses it. These
 * variables are:
 *
 * \li \b DVDCSS_VERBOSE: sets the verbosity level.
 *     - \c 0 outputs no messages at all.
 *     - \c 1 outputs error messages to stderr.
 *     - \c 2 outputs error messages and debug messages to stderr.
 *
 * \li \b DVDCSS_METHOD: sets the authentication and decryption method
 *     that \e libdvdcss will use to read scrambled discs. Can be one
 *     of \c title, \c key or \c disc.
 *     - \c key is the default method. \e libdvdcss will use a set of
 *       calculated player keys to try and get the disc key. This can fail
 *       if the drive does not recognize any of the player keys.
 *     - \c disc is a fallback method when \c key has failed. Instead of
 *       using player keys, \e libdvdcss will crack the disc key using
 *       a brute force algorithm. This process is CPU intensive and requires
 *       64 MB of memory to store temporary data.
 *     - \c title is the fallback when all other methods have failed. It does
 *       not rely on a key exchange with the DVD drive, but rather uses a
 *       crypto attack to guess the title key. On rare cases this may fail
 *       because there is not enough encrypted data on the disc to perform
 *       a statistical attack, but in the other hand it is the only way to
 *       decrypt a DVD stored on a hard disc, or a DVD with the wrong region
 *       on an RPC2 drive.
 *
 * \li \b DVDCSS_RAW_DEVICE: specify the raw device to use.
 * 
 */

/*
 * Preamble
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "dvdcss/dvdcss.h"

#include "common.h"
#include "css.h"
#include "libdvdcss.h"
#include "ioctl.h"
#include "device.h"

/**
 * \brief Symbol for version checks.
 *
 * The name of this symbol contains the library major number, which makes it
 * easy to check which \e libdvdcss development headers are installed on the
 * system with tools such as autoconf.
 *
 * The variable itself contains the exact version number of the library,
 * which can be useful for specific feature needs.
 */
char * dvdcss_interface_2 = VERSION;

/**
 * \brief Open a DVD device or directory and return a dvdcss instance.
 *
 * \param psz_target a string containing the target name, for instance
 *        "/dev/hdc" or "E:".
 * \return a handle to a dvdcss instance or NULL on error.
 *
 * Initialize the \e libdvdcss library and open the requested DVD device or
 * directory. \e libdvdcss checks whether ioctls can be performed on the disc,
 * and when possible, the disc key is retrieved.
 *
 * dvdcss_open() returns a handle to be used for all subsequent \e libdvdcss
 * calls. If an error occured, NULL is returned.
 */
extern dvdcss_t dvdcss_open ( char *psz_target )
{
    int i_ret;

    char *psz_method = getenv( "DVDCSS_METHOD" );
    char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
#ifndef WIN32
    char *psz_raw_device = getenv( "DVDCSS_RAW_DEVICE" );
#endif

    dvdcss_t dvdcss;

    /*
     *  Allocate the library structure
     */
    dvdcss = malloc( sizeof( struct dvdcss_s ) );
    if( dvdcss == NULL )
    {
        return NULL;
    }

    /*
     *  Initialize structure with default values
     */
#ifndef WIN32
    dvdcss->i_raw_fd = -1;
#endif
    dvdcss->p_titles = NULL;
    dvdcss->psz_device = (char *)strdup( psz_target );
    dvdcss->psz_error = "no error";
    dvdcss->i_method = DVDCSS_METHOD_KEY;
    dvdcss->b_debug = 0;
    dvdcss->b_errors = 0;

    /*
     *  Find verbosity from DVDCSS_VERBOSE environment variable
     */
    if( psz_verbose != NULL )
    {
        switch( atoi( psz_verbose ) )
        {
        case 2:
            dvdcss->b_debug = 1;
        case 1:
            dvdcss->b_errors = 1;
        case 0:
            break;
        }
    }

    /*
     *  Find method from DVDCSS_METHOD environment variable
     */
    if( psz_method != NULL )
    {
        if( !strncmp( psz_method, "key", 4 ) )
        {
            dvdcss->i_method = DVDCSS_METHOD_KEY;
        }
        else if( !strncmp( psz_method, "disc", 5 ) )
        {
            dvdcss->i_method = DVDCSS_METHOD_DISC;
        }
        else if( !strncmp( psz_method, "title", 5 ) )
        {
            dvdcss->i_method = DVDCSS_METHOD_TITLE;
        }
        else
        {
            _dvdcss_error( dvdcss, "unknown decrypt method, please choose "
                                   "from 'title', 'key' or 'disc'" );
            free( dvdcss->psz_device );
            free( dvdcss );
            return NULL;
        }
    }

    /*
     *  Open device
     */
    i_ret = _dvdcss_open( dvdcss );
    if( i_ret < 0 )
    {
        free( dvdcss->psz_device );
        free( dvdcss );
        return NULL;
    }
    
    dvdcss->b_scrambled = 1; /* Assume the worst */
    dvdcss->b_ioctls = _dvdcss_use_ioctls( dvdcss );

    if( dvdcss->b_ioctls )
    {
        i_ret = _dvdcss_test( dvdcss );
	if( i_ret < 0 )
	{
	    /* Disable the CSS ioctls and hope that it works? */
            _dvdcss_debug( dvdcss,
                           "could not check whether the disc was scrambled" );
	    dvdcss->b_ioctls = 0;
	}
	else
	{
            _dvdcss_debug( dvdcss, i_ret ? "disc is scrambled"
                                         : "disc is unscrambled" );
	    dvdcss->b_scrambled = i_ret;
	}
    }

    /* If disc is CSS protected and the ioctls work, authenticate the drive */
    if( dvdcss->b_scrambled && dvdcss->b_ioctls )
    {
        i_ret = _dvdcss_disckey( dvdcss );

        if( i_ret < 0 )
        {
            _dvdcss_close( dvdcss );
            free( dvdcss->psz_device );
            free( dvdcss );
            return NULL;
        }
    }

#ifndef WIN32
    if( psz_raw_device != NULL )
    {
        _dvdcss_raw_open( dvdcss, psz_raw_device );
    }
#endif

    return dvdcss;
}

/**
 * \brief Return a string containing the latest error that occured in the
 *        given \e libdvdcss instance.
 *
 * \param dvdcss a \e libdvdcss instance.
 * \return a null-terminated string containing the latest error message.
 *
 * This function returns a constant string containing the latest error that
 * occured in \e libdvdcss. It can be used to format error messages at your
 * convenience in your application.
 */
extern char * dvdcss_error ( dvdcss_t dvdcss )
{
    return dvdcss->psz_error;
}

/**
 * \brief Seek in the disc and change the current key if requested.
 *
 * \param dvdcss a \e libdvdcss instance.
 * \param i_blocks an absolute block offset to seek to.
 * \param i_flags #DVDCSS_NOFLAGS, optionally ored with one of #DVDCSS_SEEK_KEY
 *        or #DVDCSS_SEEK_MPEG.
 * \return the new position in blocks, or a negative value in case an error
 *         happened.
 *
 * This function seeks to the requested position, in logical blocks.
 *
 * You typically set \p i_flags to #DVDCSS_NOFLAGS when seeking in a .IFO.
 *
 * If #DVDCSS_SEEK_MPEG is specified in \p i_flags and if \e libdvdcss finds it
 * reasonable to do so (ie, if the dvdcss method is not "title"), the current
 * title key will be checked and a new one will be calculated if necessary.
 * This flag is typically used when reading data from a VOB.
 *
 * If #DVDCSS_SEEK_KEY is specified, the title key will be always checked,
 * even with the "title" method. This is equivalent to using the now
 * deprecated dvdcss_title() call. This flag is typically used when seeking
 * in a new title.
 */
extern int dvdcss_seek ( dvdcss_t dvdcss, int i_blocks, int i_flags )
{
    /* title cracking method is too slow to be used at each seek */
    if( ( ( i_flags & DVDCSS_SEEK_MPEG )
             && ( dvdcss->i_method != DVDCSS_METHOD_TITLE ) ) 
       || ( i_flags & DVDCSS_SEEK_KEY ) )
    {
        /* check the title key */
        if( _dvdcss_title( dvdcss, i_blocks ) ) 
        {
            return -1;
        }
    }

    return _dvdcss_seek( dvdcss, i_blocks );
}

/**
 * \brief Read from the disc and decrypt data if requested.
 *
 * \param dvdcss a \e libdvdcss instance.
 * \param p_buffer a buffer that will contain the data read from the disc.
 * \param i_blocks the amount of blocks to read.
 * \param i_flags #DVDCSS_NOFLAGS, optionally ored with #DVDCSS_READ_DECRYPT.
 * \return the amount of blocks read, or a negative value in case an
 *         error happened.
 *
 * This function reads \p i_blocks logical blocks from the DVD.
 *
 * You typically set \p i_flags to #DVDCSS_NOFLAGS when reading data from a
 * .IFO file on the DVD.
 *
 * If #DVDCSS_READ_DECRYPT is specified in \p i_flags, dvdcss_read() will
 * automatically decrypt scrambled sectors. This flag is typically used when
 * reading data from a .VOB file on the DVD. It has no effect on unscrambled
 * discs or unscrambled sectors, and can be safely used on those.
 *
 * \warning dvdcss_read() expects to be able to write \p i_blocks *
 *          #DVDCSS_BLOCK_SIZE bytes in \p p_buffer.
 */
extern int dvdcss_read ( dvdcss_t dvdcss, void *p_buffer,
                                          int i_blocks,
                                          int i_flags )
{
    int i_ret, i_index;

    i_ret = _dvdcss_read( dvdcss, p_buffer, i_blocks );

    if( i_ret <= 0
         || !dvdcss->b_scrambled
         || !(i_flags & DVDCSS_READ_DECRYPT) )
    {
        return i_ret;
    }

    if( ! memcmp( dvdcss->css.p_title_key, "\0\0\0\0\0", 5 ) )
    {
        /* For what we believe is an unencrypted title, 
	 * check that there are no encrypted blocks */
        for( i_index = i_ret; i_index; i_index-- )
        {
            if( ((u8*)p_buffer)[0x14] & 0x30 )
            {
                _dvdcss_error( dvdcss, "no key but found encrypted block" );
                /* Only return the initial range of unscrambled blocks? */
                /* or fail completely? return 0; */
		break;
            }
            p_buffer = (void *) ((u8 *)p_buffer + DVDCSS_BLOCK_SIZE);
        }
    }
    else 
    {
        /* Decrypt the blocks we managed to read */
        for( i_index = i_ret; i_index; i_index-- )
	{
	    _dvdcss_unscramble( dvdcss->css.p_title_key, p_buffer );
	    ((u8*)p_buffer)[0x14] &= 0x8f;
            p_buffer = (void *) ((u8 *)p_buffer + DVDCSS_BLOCK_SIZE);
	}
    }
    
    return i_ret;
}

/**
 * \brief Read from the disc into multiple buffers and decrypt data if
 *        requested.
 *
 * \param dvdcss a \e libdvdcss instance.
 * \param p_iovec a pointer to an array of iovec structures that will contain
 *        the data read from the disc.
 * \param i_blocks the amount of blocks to read.
 * \param i_flags #DVDCSS_NOFLAGS, optionally ored with #DVDCSS_READ_DECRYPT.
 * \return the amount of blocks read, or a negative value in case an
 *         error happened.
 *
 * This function reads \p i_blocks logical blocks from the DVD and writes them
 * to an array of iovec structures.
 *
 * You typically set \p i_flags to #DVDCSS_NOFLAGS when reading data from a
 * .IFO file on the DVD.
 *
 * If #DVDCSS_READ_DECRYPT is specified in \p i_flags, dvdcss_readv() will
 * automatically decrypt scrambled sectors. This flag is typically used when
 * reading data from a .VOB file on the DVD. It has no effect on unscrambled
 * discs or unscrambled sectors, and can be safely used on those.
 *
 * \warning dvdcss_readv() expects to be able to write \p i_blocks *
 *          #DVDCSS_BLOCK_SIZE bytes in the buffers pointed by \p p_iovec.
 *          Moreover, all iov_len members of the iovec structures should be
 *          multiples of #DVDCSS_BLOCK_SIZE.
 */
extern int dvdcss_readv ( dvdcss_t dvdcss, void *p_iovec,
                                           int i_blocks,
                                           int i_flags )
{
    struct iovec *_p_iovec = (struct iovec *)p_iovec;
    int i_ret, i_index;
    void *iov_base;
    size_t iov_len;

    i_ret = _dvdcss_readv( dvdcss, _p_iovec, i_blocks );

    if( i_ret <= 0
         || !dvdcss->b_scrambled
         || !(i_flags & DVDCSS_READ_DECRYPT) )
    {
        return i_ret;
    }

    /* Initialize loop for decryption */
    iov_base = _p_iovec->iov_base;
    iov_len = _p_iovec->iov_len;

    /* Decrypt the blocks we managed to read */
    for( i_index = i_ret; i_index; i_index-- )
    {
        /* Check that iov_len is a multiple of 2048 */
        if( iov_len & 0x7ff )
        {
            return -1;
        }

        while( iov_len == 0 )
        {
            _p_iovec++;
            iov_base = _p_iovec->iov_base;
            iov_len = _p_iovec->iov_len;
        }

        _dvdcss_unscramble( dvdcss->css.p_title_key, iov_base );
        ((u8*)iov_base)[0x14] &= 0x8f;

        iov_base = (void *) ((u8*)iov_base + DVDCSS_BLOCK_SIZE);
        iov_len -= DVDCSS_BLOCK_SIZE;
    }

    return i_ret;
}

/**
 * \brief Close the DVD and clean up the library.
 *
 * \param dvdcss a \e libdvdcss instance.
 * \return zero in case of success, a negative value otherwise.
 *
 * This function closes the DVD device and frees all the memory allocated
 * by \e libdvdcss. On return, the #dvdcss_t is invalidated and may not be
 * used again.
 */
extern int dvdcss_close ( dvdcss_t dvdcss )
{
    dvd_title_t *p_title;
    int i_ret;

    /* Free our list of keys */
    p_title = dvdcss->p_titles;
    while( p_title )
    {
        dvd_title_t *p_tmptitle = p_title->p_next;
        free( p_title );
        p_title = p_tmptitle;
    }

    i_ret = _dvdcss_close( dvdcss );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    free( dvdcss->psz_device );
    free( dvdcss );

    return 0;
}

/*
 *  Deprecated. See dvdcss_seek().
 */
#undef dvdcss_title
extern int dvdcss_title ( dvdcss_t dvdcss, int i_block )
{
    return _dvdcss_title( dvdcss, i_block );
}

