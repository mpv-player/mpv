/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: hcb_2.h,v 1.3 2003/09/09 18:12:01 menno Exp $
**/

/* 2-step huffman table HCB_2 */


/* 1st step: 5 bits
 *           2^5 = 32 entries
 *
 * Used to find offset into 2nd step table and number of extra bits to get
 */
static hcb hcb2_1[] = {
    { /* 00000 */ 0, 0 },
    { /*       */ 0, 0 },
    { /*       */ 0, 0 },
    { /*       */ 0, 0 },
    { /* 00100 */ 1, 0 },
    { /*       */ 1, 0 },
    { /* 00110 */ 2, 0 },
    { /* 00111 */ 3, 0 },
    { /* 01000 */ 4, 0 },
    { /* 01001 */ 5, 0 },
    { /* 01010 */ 6, 0 },
    { /* 01011 */ 7, 0 },
    { /* 01100 */ 8, 0 },

    /* 6 bit codewords */
    { /* 01101 */ 9,  1 },
    { /* 01110 */ 11, 1 },
    { /* 01111 */ 13, 1 },
    { /* 10000 */ 15, 1 },
    { /* 10001 */ 17, 1 },
    { /* 10010 */ 19, 1 },
    { /* 10011 */ 21, 1 },
    { /* 10100 */ 23, 1 },
    { /* 10101 */ 25, 1 },
    { /* 10110 */ 27, 1 },
    { /* 10111 */ 29, 1 },
    { /* 11000 */ 31, 1 },

    /* 7 bit codewords */
    { /* 11001 */ 33, 2 },
    { /* 11010 */ 37, 2 },
    { /* 11011 */ 41, 2 },

    /* 7/8 bit codewords */
    { /* 11100 */ 45, 3 },

    /* 8 bit codewords */
    { /* 11101 */ 53, 3 },
    { /* 11110 */ 61, 3 },

    /* 8/9 bit codewords */
    { /* 11111 */ 69, 4 }
};

/* 2nd step table
 *
 * Gives size of codeword and actual data (x,y,v,w)
 */
static hcb_2_quad hcb2_2[] = {
    /* 3 bit codeword */
    { 3,  0,  0,  0,  0 },

    /* 4 bit codeword */
    { 4,  1,  0,  0,  0 },

    /* 5 bit codewords */
    { 5, -1,  0,  0,  0 },
    { 5,  0,  0,  0,  1 },
    { 5,  0,  0, -1,  0 },
    { 5,  0,  0,  0, -1 },
    { 5,  0, -1,  0,  0 },
    { 5,  0,  0,  1,  0 },
    { 5,  0,  1,  0,  0 },

    /* 6 bit codewords */
    { 6,  0, -1,  1,  0 },
    { 6, -1,  1,  0,  0 },
    { 6,  0,  1, -1,  0 },
    { 6,  0,  0,  1, -1 },
    { 6,  0,  1,  0, -1 },
    { 6,  0,  0, -1,  1 },
    { 6, -1,  0,  0, -1 },
    { 6,  1, -1,  0,  0 },
    { 6,  1,  0, -1,  0 },
    { 6, -1, -1,  0,  0 },
    { 6,  0,  0, -1, -1 },
    { 6,  1,  0,  1,  0 },
    { 6,  1,  0,  0,  1 },
    { 6,  0, -1,  0,  1 },
    { 6, -1,  0,  1,  0 },
    { 6,  0,  1,  0,  1 },
    { 6,  0, -1, -1,  0 },
    { 6, -1,  0,  0,  1 },
    { 6,  0, -1,  0, -1 },
    { 6, -1,  0, -1,  0 },
    { 6,  1,  1,  0,  0 },
    { 6,  0,  1,  1,  0 },
    { 6,  0,  0,  1,  1 },
    { 6,  1,  0,  0, -1 },

    /* 7 bit codewords */
    { 7,  0,  1, -1,  1 },
    { 7,  1,  0, -1,  1 },
    { 7, -1,  1, -1,  0 },
    { 7,  0, -1,  1, -1 },
    { 7,  1, -1,  1,  0 },
    { 7,  1,  1,  0, -1 },
    { 7,  1,  0,  1,  1 },
    { 7, -1,  1,  1,  0 },
    { 7,  0, -1, -1,  1 },
    { 7,  1,  1,  1,  0 },
    { 7, -1,  0,  1, -1 },
    { 7, -1, -1, -1,  0 },

    /* 7/8 bit codewords */
    { 7, -1,  0, -1,  1 }, { 7, -1,  0, -1,  1 },
    { 7,  1, -1, -1,  0 }, { 7,  1, -1, -1,  0 },
    { 7,  1,  1, -1,  0 }, { 7,  1,  1, -1,  0 },
    { 8,  1, -1,  0,  1 },
    { 8, -1,  1,  0, -1 },

    /* 8 bit codewords */
    { 8, -1, -1,  1,  0 },
    { 8, -1,  0,  1,  1 },
    { 8, -1, -1,  0,  1 },
    { 8, -1, -1,  0, -1 },
    { 8,  0, -1, -1, -1 },
    { 8,  1,  0,  1, -1 },
    { 8,  1,  0, -1, -1 },
    { 8,  0,  1, -1, -1 },
    { 8,  0,  1,  1,  1 },
    { 8, -1,  1,  0,  1 },
    { 8, -1,  0, -1, -1 },
    { 8,  0,  1,  1, -1 },
    { 8,  1, -1,  0, -1 },
    { 8,  0, -1,  1,  1 },
    { 8,  1,  1,  0,  1 },
    { 8,  1, -1,  1, -1 },

    /* 8/9 bit codewords */
    { 8, -1,  1, -1,  1 }, { 8, -1,  1, -1,  1 },
    { 9,  1, -1, -1,  1 },
    { 9, -1, -1, -1, -1 },
    { 9, -1,  1,  1, -1 },
    { 9, -1,  1,  1,  1 },
    { 9,  1,  1,  1,  1 },
    { 9, -1, -1,  1, -1 },
    { 9,  1, -1,  1,  1 },
    { 9, -1,  1, -1, -1 },
    { 9, -1, -1,  1,  1 },
    { 9,  1,  1, -1, -1 },
    { 9,  1, -1, -1, -1 },
    { 9, -1, -1, -1,  1 },
    { 9,  1,  1, -1,  1 },
    { 9,  1,  1,  1, -1 }
};
