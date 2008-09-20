/* 
 * vo_quartz.h
 * Mac keyboard def taken from SDL
 * See the Subversion log for a list of changes.
 */

/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

    Sam Lantinga
    slouken@libsdl.org
 */

#ifndef MPLAYER_VO_QUARTZ_H
#define MPLAYER_VO_QUARTZ_H

/* These are the Macintosh key scancode constants -- from Inside Macintosh */
#define QZ_ESCAPE		0x35
#define QZ_F1			0x7A
#define QZ_F2			0x78
#define QZ_F3			0x63
#define QZ_F4			0x76
#define QZ_F5			0x60
#define QZ_F6			0x61
#define QZ_F7			0x62
#define QZ_F8			0x64
#define QZ_F9			0x65
#define QZ_F10			0x6D
#define QZ_F11			0x67
#define QZ_F12			0x6F
#define QZ_PRINT		0x69
#define QZ_SCROLLOCK    	0x6B
#define QZ_PAUSE		0x71
#define QZ_POWER		0x7F
#define QZ_BACKQUOTE		0x32
#define QZ_1			0x12
#define QZ_2			0x13
#define QZ_3			0x14
#define QZ_4			0x15
#define QZ_5			0x17
#define QZ_6			0x16
#define QZ_7			0x1A
#define QZ_8			0x1C
#define QZ_9			0x19
#define QZ_0			0x1D
#define QZ_MINUS		0x1B
#define QZ_EQUALS		0x18
#define QZ_BACKSPACE		0x33
#define QZ_INSERT		0x72
#define QZ_HOME			0x73
#define QZ_PAGEUP		0x74
#define QZ_NUMLOCK		0x47
#define QZ_KP_EQUALS		0x51
#define QZ_KP_DIVIDE		0x4B
#define QZ_KP_MULTIPLY		0x43
#define QZ_TAB			0x30
#define QZ_q			0x0C
#define QZ_w			0x0D
#define QZ_e			0x0E
#define QZ_r			0x0F
#define QZ_t			0x11
#define QZ_y			0x10
#define QZ_u			0x20
#define QZ_i			0x22
#define QZ_o			0x1F
#define QZ_p			0x23
#define QZ_LEFTBRACKET		0x21
#define QZ_RIGHTBRACKET		0x1E
#define QZ_BACKSLASH		0x2A
#define QZ_DELETE		0x75
#define QZ_END			0x77
#define QZ_PAGEDOWN		0x79
#define QZ_KP7			0x59
#define QZ_KP8			0x5B
#define QZ_KP9			0x5C
#define QZ_KP_MINUS		0x4E
#define QZ_CAPSLOCK		0x39
#define QZ_a			0x00
#define QZ_s			0x01
#define QZ_d			0x02
#define QZ_f			0x03
#define QZ_g			0x05
#define QZ_h			0x04
#define QZ_j			0x26
#define QZ_k			0x28
#define QZ_l			0x25
#define QZ_SEMICOLON		0x29
#define QZ_QUOTE		0x27
#define QZ_RETURN		0x24
#define QZ_KP4			0x56
#define QZ_KP5			0x57
#define QZ_KP6			0x58
#define QZ_KP_PLUS		0x45
#define QZ_LSHIFT		0x38
#define QZ_z			0x06
#define QZ_x			0x07
#define QZ_c			0x08
#define QZ_v			0x09
#define QZ_b			0x0B
#define QZ_n			0x2D
#define QZ_m			0x2E
#define QZ_COMMA		0x2B
#define QZ_PERIOD		0x2F
#define QZ_SLASH		0x2C
/* These are the same as the left versions - use left by default */
#if 0				
#define QZ_RSHIFT		0x38
#endif
#define QZ_UP			0x7E
#define QZ_KP1			0x53
#define QZ_KP2			0x54
#define QZ_KP3			0x55
#define QZ_KP_ENTER		0x4C
#define QZ_LCTRL		0x3B
#define QZ_LALT			0x3A
#define QZ_LMETA		0x37
#define QZ_SPACE		0x31
/* These are the same as the left versions - use left by default */
#if 0				
#define QZ_RMETA		0x37
#define QZ_RALT			0x3A
#define QZ_RCTRL		0x3B
#endif
#define QZ_LEFT			0x7B
#define QZ_DOWN			0x7D
#define QZ_RIGHT		0x7C
#define QZ_KP0			0x52
#define QZ_KP_PERIOD		0x41

/* Wierd, these keys are on my iBook under MacOS X */
#define QZ_IBOOK_ENTER		0x34
#define QZ_IBOOK_LEFT		0x3B
#define QZ_IBOOK_RIGHT		0x3C
#define QZ_IBOOK_DOWN		0x3D
#define QZ_IBOOK_UP		0x3E

#endif /* MPLAYER_VO_QUARTZ_H */
