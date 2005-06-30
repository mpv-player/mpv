#ifndef DVD_INPUT_H_INCLUDED
#define DVD_INPUT_H_INCLUDED

/*
 * Copyright (C) 2001, 2002 Samuel Hocevar <sam@zoy.org>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
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
 * Defines and flags.  Make sure they fit the libdvdcss API!
 */
#define DVDINPUT_NOFLAGS         0

#define DVDINPUT_READ_DECRYPT    (1 << 0)

typedef struct dvd_input_s *dvd_input_t;

/**
 * Pointers which will be filled either the input methods functions.
 */
extern dvd_input_t (*dvdinput_open)  (const char *);
extern int         (*dvdinput_close) (dvd_input_t);
extern int         (*dvdinput_seek)  (dvd_input_t, int);
extern int         (*dvdinput_title) (dvd_input_t, int); 
extern int         (*dvdinput_read)  (dvd_input_t, void *, int, int);
extern char *      (*dvdinput_error) (dvd_input_t);

/**
 * Setup function accessed by dvd_reader.c.  Returns 1 if there is CSS support.
 */
int dvdinput_setup(void);

#endif /* DVD_INPUT_H_INCLUDED */
