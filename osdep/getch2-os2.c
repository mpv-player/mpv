/*
 * getch2-os2.c : OS/2 TermIO for MPlayer
 *
 * Copyright (c) 2007 KO Myung-Hun (komh@chollian.net)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define INCL_KBD
#define INCL_VIO
#define INCL_DOS
#include <os2.h>

#include <stdio.h>

#include "config.h"
#include "keycodes.h"
#include "input/input.h"
#include "mp_fifo.h"

#if defined(HAVE_LANGINFO) && defined(CONFIG_ICONV)
#include <locale.h>
#include <langinfo.h>
#endif

int mp_input_slave_cmd_func( int fd, char *dest, int size )
{
    PPIB    ppib;
    CHAR    szPipeName[ 100 ];
    HFILE   hpipe;
    ULONG   ulAction;
    ULONG   cbActual;
    ULONG   rc;

    DosGetInfoBlocks( NULL, &ppib );

    sprintf( szPipeName, "\\PIPE\\MPLAYER\\%lx", ppib->pib_ulpid );

    rc = DosOpen( szPipeName, &hpipe, &ulAction, 0, FILE_NORMAL,
                  OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
                  NULL );
    if( rc )
        return MP_INPUT_NOTHING;

    rc = DosRead( hpipe, dest, size, &cbActual );
    if( rc )
        return MP_INPUT_NOTHING;

    rc = cbActual;

    // Send ACK
    DosWrite( hpipe, &rc, sizeof( ULONG ), &cbActual );

    DosClose( hpipe );

    return rc;
}


int screen_width = 80;
int screen_height = 24;
char *erase_to_end_of_line = NULL;

void get_screen_size( void )
{
    VIOMODEINFO vmi;

    vmi.cb = sizeof( VIOMODEINFO );

    VioGetMode( &vmi, 0 );

    screen_width = vmi.col;
    screen_height = vmi.row;
}

static int getch2_status = 0;

static int getch2_internal( void )
{
    KBDKEYINFO kki;

    if( !getch2_status )
        return -1;

    if( KbdCharIn( &kki, IO_NOWAIT, 0 ))
        return -1;

    // key pressed ?
    if( kki.fbStatus )
    {
        // extended key ?
        if(( kki.chChar == 0x00 ) || ( kki.chChar == 0xE0 ))
        {
            switch( kki.chScan )
            {
                case 0x4B : // Left
                    return KEY_LEFT;

                case 0x48 : // Up
                    return KEY_UP;

                case 0x4D : // Right
                    return KEY_RIGHT;

                case 0x50 : // Down
                    return KEY_DOWN;

                case 0x53 : // Delete
                    return KEY_DELETE;

                case 0x52 : // Insert
                    return KEY_INSERT;

                case 0x47 : // Home
                    return KEY_HOME;

                case 0x4F : // End
                    return KEY_END;

                case 0x49 : // Page Up
                    return KEY_PAGE_UP;

                case 0x51 : // Page Down
                    return KEY_PAGE_DOWN;
            }
        }
        else
        {
            switch( kki.chChar )
            {
                case 0x08 : // Backspace
                    return KEY_BS;

                case 0x1B : // Esc
                    return KEY_ESC;

                case 0x0D : // Enter
                    // Keypad Enter ?
                    if( kki.chScan == 0xE0 )
                        return KEY_KPENTER;
                    break;
            }

            return kki.chChar;
        }
    }

    return -1;
}

void getch2( void )
{
    int key;

    key = getch2_internal();
    if( key != -1 )
        mplayer_put_key( key );
}

void getch2_enable( void )
{
    getch2_status = 1;
}

void getch2_disable( void )
{
    getch2_status = 0;
}

#ifdef CONFIG_ICONV
char *get_term_charset( void )
{
    char *charset = NULL;

#ifdef HAVE_LANGINFO
    setlocale( LC_CTYPE, "");
    charset = nl_langinfo( CODESET );
    setlocale( LC_CTYPE, "C");
#endif

    return charset;
}
#endif

