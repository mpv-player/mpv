#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "tv.h"
#include "tvi_vbi.h"
#include "mp_msg.h"
#include "libmpcodecs/img_format.h"

#ifdef USE_ICONV
#include <iconv.h>
#endif

#define VBI_TEXT_CHARSET    "UTF-8"

char* tv_param_tdevice=NULL;        ///< teletext vbi device
char* tv_param_tformat="gray";      ///< format: text,bw,gray,color
int tv_param_tpage=100;             ///< page number


#ifdef USE_ICONV
/*
------------------------------------------------------------------
    zvbi-0.2.25/src/exp-txt.c skip debug "if(1) fprintf(stderr,) " message
------------------------------------------------------------------
*/

/**
 *  libzvbi - Text export functions
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

/** $Id$ **/

static vbi_bool
print_unicode(iconv_t cd, int endian, int unicode, char **p, int n)
{
    char in[2], *ip, *op;
    size_t li, lo, r;

    in[0 + endian] = unicode;
    in[1 - endian] = unicode >> 8;
    ip = in; op = *p;
    li = sizeof(in); lo = n;

    r = iconv(cd, &ip, &li, &op, &lo);

    if ((size_t) -1 == r
        || (**p == 0x40 && unicode != 0x0040)) {
        in[0 + endian] = 0x20;
        in[1 - endian] = 0;
        ip = in; op = *p;
        li = sizeof(in); lo = n;

        r = iconv(cd, &ip, &li, &op, &lo);

        if ((size_t) -1 == r
            || (r == 1 && **p == 0x40))
            goto error;
    }

    *p = op;

    return TRUE;

  error:
    return FALSE;
}

static int
vbi_print_page_region_nodebug(vbi_page * pg, char *buf, int size,
                              const char *format, vbi_bool table,
                              vbi_bool rtl, int column, int row, int width,
                              int height)
{
    int endian = vbi_ucs2be();
    int column0, column1, row0, row1;
    int x, y, spaces, doubleh, doubleh0;
    iconv_t cd;
    char *p;

    rtl = rtl;

#if 0
    if (1)
        fprintf (stderr, "vbi_print_page_region '%s' "
            "table=%d col=%d row=%d width=%d height=%d\n",
             format, table, column, row, width, height);
#endif

    column0 = column;
    row0 = row;
    column1 = column + width - 1;
    row1 = row + height - 1;

    if (!pg || !buf || size < 0 || !format
        || column0 < 0 || column1 >= pg->columns
        || row0 < 0 || row1 >= pg->rows
        || endian < 0)
        return 0;

    if ((cd = iconv_open(format, "UCS-2")) == (iconv_t) -1)
        return 0;

    p = buf;

    doubleh = 0;

    for (y = row0; y <= row1; y++) {
        int x0, x1, xl;

        x0 = (table || y == row0) ? column0 : 0;
        x1 = (table || y == row1) ? column1 : (pg->columns - 1);

        xl = (table || y != row0 || (y + 1) != row1) ? -1 : column1;

        doubleh0 = doubleh;

        spaces = 0;
        doubleh = 0;

        for (x = x0; x <= x1; x++) {
            vbi_char ac = pg->text[y * pg->columns + x];

            if (table) {
                if (ac.size > VBI_DOUBLE_SIZE)
                    ac.unicode = 0x0020;
            } else {
                switch (ac.size) {
                case VBI_NORMAL_SIZE:
                case VBI_DOUBLE_WIDTH:
                    break;

                case VBI_DOUBLE_HEIGHT:
                case VBI_DOUBLE_SIZE:
                    doubleh++;
                    break;

                case VBI_OVER_TOP:
                case VBI_OVER_BOTTOM:
                    continue;

                case VBI_DOUBLE_HEIGHT2:
                case VBI_DOUBLE_SIZE2:
                    if (y > row0)
                        ac.unicode = 0x0020;
                    break;
                }

                /*
                 *  Special case two lines row0 ... row1, and all chars
                 *  in row0, column0 ... column1 are double height: Skip
                 *  row1, don't wrap around.
                 */
                if (x == xl && doubleh >= (x - x0)) {
                    x1 = xl;
                    y = row1;
                }

                if (ac.unicode == 0x20 || !vbi_is_print(ac.unicode)) {
                    spaces++;
                    continue;
                } else {
                    if (spaces < (x - x0) || y == row0) {
                        for (; spaces > 0; spaces--)
                            if (!print_unicode(cd, endian, 0x0020,
                                               &p, buf + size - p))
                                goto failure;
                    } else /* discard leading spaces */
                        spaces = 0;
                }
            }

            if (!print_unicode(cd, endian, ac.unicode, &p, buf + size - p))
                goto failure;
        }

        /* if !table discard trailing spaces and blank lines */

        if (y < row1) {
            int left = buf + size - p;

            if (left < 1)
                goto failure;

            if (table) {
                *p++ = '\n'; /* XXX convert this (eg utf16) */
            } else if (spaces >= (x1 - x0)) {
                ; /* suppress blank line */
            } else {
                /* exactly one space between adjacent rows */
                if (!print_unicode(cd, endian, 0x0020, &p, left))
                    goto failure;
            }
        } else {
            if (doubleh0 > 0) {
                ; /* prentend this is a blank double height lower row */
            } else {
                for (; spaces > 0; spaces--)
                    if (!print_unicode(cd, endian, 0x0020, &p, buf + size - p))
                        goto failure;
            }
        }
    }

    iconv_close(cd);
    return p - buf;

  failure:
    iconv_close(cd);
    return 0;
}
#endif
/*
 end of zvbi-0.2.25/src/exp-txt.c part
*/


/*
------------------------------------------------------------------
  Private routines
------------------------------------------------------------------
*/

/**
 * \brief Decode event handler
 * \param ev VBI event
 * \param data pointer to user defined data
 *
 */
static void event_handler(vbi_event * ev, void *data)
{
    priv_vbi_t *user_vbi = (priv_vbi_t *) data;
    vbi_page pg;
    char *s;
    int i;

    switch (ev->type) {
    case VBI_EVENT_CAPTION:
        mp_msg(MSGT_TV,MSGL_DBG3,"caption\n");
        break;
    case VBI_EVENT_NETWORK:
        s = ev->ev.network.name;
        if (s) {
            pthread_mutex_lock(&(user_vbi->buffer_mutex));
            if (user_vbi->network_name)
                free(user_vbi->network_name);
            user_vbi->network_name = strdup(s);
            pthread_mutex_unlock(&(user_vbi->buffer_mutex));
        }
        break;
    case VBI_EVENT_NETWORK_ID:
        s = ev->ev.network.name;
        if (s) {
            pthread_mutex_lock(&(user_vbi->buffer_mutex));
            if (user_vbi->network_id)
                free(user_vbi->network_id);
            user_vbi->network_id = strdup(s);
            pthread_mutex_unlock(&(user_vbi->buffer_mutex));
        }
        break;
    case VBI_EVENT_TTX_PAGE:
        pthread_mutex_lock(&(user_vbi->buffer_mutex));
        user_vbi->curr_pgno = ev->ev.ttx_page.pgno;     // page number
        user_vbi->curr_subno = ev->ev.ttx_page.subno;   // subpage
        i = vbi_bcd2dec(ev->ev.ttx_page.pgno);
        if (i > 0 && i < 1000) {
            if (!user_vbi->cache[i])
                user_vbi->cache[i] = (vbi_page *) malloc(sizeof(vbi_page));
            vbi_fetch_vt_page(user_vbi->decoder,        // fetch page
                              user_vbi->cache[i],
                              ev->ev.ttx_page.pgno,
                              ev->ev.ttx_page.subno,
                              VBI_WST_LEVEL_3p5, 25, TRUE);
            memcpy(user_vbi->theader, user_vbi->cache[i]->text,
                sizeof(user_vbi->theader));
        }
        pthread_mutex_unlock(&(user_vbi->buffer_mutex));
        break;
        }
}

/**
 * \brief Prepares page to be shown on screen
 * \param priv_vbi private data structure
 *
 * This routine adds page number, current time, etc to page header
 *
 */
static void process_page(priv_vbi_t * priv_vbi)
{
    char *pagesptr;
    int csize, i, j, subtitle = 0, sflg, send;
    void *canvas;
    char cpage[5];
    vbi_page page;

    memcpy(&(page), priv_vbi->page, sizeof(vbi_page));
    if (priv_vbi->pgno != priv_vbi->page->pgno) {
        //don't clear first line
        for (i = page.columns; i < 1056; i++) {
            page.text[i].unicode = ' ';
            page.text[i].background = VBI_TRANSPARENT_COLOR;
        }
        snprintf(cpage, sizeof(cpage), "%03X", priv_vbi->pgno);
        page.text[1].unicode = cpage[0];
        page.text[2].unicode = cpage[1];
        page.text[3].unicode = cpage[2];
        page.text[4].unicode = ' ';
        page.text[5].unicode = ' ';
        page.text[6].unicode = ' ';
    }

    //background page number & title
    j=vbi_bcd2dec(priv_vbi->curr_pgno);
    if (j>0 && j<1000 && priv_vbi->cache[j]){
        for(i=8;i<priv_vbi->cache[j]->columns;i++){
            page.text[i].unicode = priv_vbi->cache[j]->text[i].unicode;
        }
    }

    if (page.text[1].unicode == ' ' && page.text[2].unicode == ' ' &&
        page.text[3].unicode == ' ' && page.text[4].unicode == ' ' &&
        page.text[5].unicode == ' ' && page.text[5].unicode == ' '
        && !priv_vbi->half)
        subtitle = 1;           // subtitle page
    if (priv_vbi->pagenumdec) {
        i = (priv_vbi->pagenumdec >> 12) & 0xf;
        switch (i) {
        case 1:
            page.text[1].unicode = '0' + ((priv_vbi->pagenumdec >> 0) & 0xf);
            page.text[2].unicode = '-';
            page.text[3].unicode = '-';
            break;
        case 2:
            page.text[1].unicode = '0' + ((priv_vbi->pagenumdec >> 4) & 0xf);
            page.text[2].unicode = '0' + ((priv_vbi->pagenumdec >> 0) & 0xf);
            page.text[3].unicode = '-';
            break;
        }
        page.text[4].unicode = ' ';
        page.text[5].unicode = ' ';
        page.text[6].unicode = ' ';
        page.text[1].foreground = VBI_WHITE;
        page.text[2].foreground = VBI_WHITE;
        page.text[3].foreground = VBI_WHITE;
    }
    priv_vbi->columns = page.columns;
    priv_vbi->rows = page.rows;
    if (!subtitle) {       // update time in header
        memcpy(&(page.text[VBI_TIME_LINEPOS]),
               &(priv_vbi->theader[VBI_TIME_LINEPOS]),
               sizeof(vbi_char) * (priv_vbi->columns - VBI_TIME_LINEPOS));
    }
    switch (priv_vbi->tformat) {
    case VBI_TFORMAT_TEXT:        // mode: text
        if (priv_vbi->txtpage) {
#ifdef USE_ICONV
            vbi_print_page_region_nodebug(&(page), priv_vbi->txtpage, 
                VBI_TXT_PAGE_SIZE, VBI_TEXT_CHARSET, TRUE, 
                0, 0, 0, page.columns, page.rows);      // vbi_page to text without message
#else
            vbi_print_page(&(page), priv_vbi->txtpage, 
                VBI_TXT_PAGE_SIZE, VBI_TEXT_CHARSET, TRUE, 0);
#endif
        }
        priv_vbi->valid_page = 1;
        break;
    case VBI_TFORMAT_BW:        // mode: black & white
        for (i=0; i < (priv_vbi->pgno!=page.pgno?page.columns:1056); i++) {
            if (priv_vbi->foreground){
                page.text[i].foreground = VBI_BLACK;
                page.text[i].background = VBI_WHITE;
	    }else{
                page.text[i].foreground = VBI_WHITE;
                page.text[i].background = VBI_BLACK;
	    }
        }
    case VBI_TFORMAT_GRAY:        // mode: grayscale
    case VBI_TFORMAT_COLOR:       // mode: color (request color spu patch!)



        page.color_map[VBI_TRANSPARENT_COLOR] = 0;
        if (priv_vbi->alpha) {
            if (subtitle) {
                for (i = 0; i < page.rows; i++) {
                    sflg = 0;
                    send = 0;
                    for (j = 0; j < page.columns; j++) {
                        if (page.text[i * page.columns + j].unicode != ' ') {
                            sflg = 1;
                            send = j;
                        }
                        if (sflg == 0)
                            page.text[i * page.columns + j].background =
                                VBI_TRANSPARENT_COLOR;
                    }
                    for (j = send + 1; j < page.columns; j++)
                        page.text[i * page.columns + j].background =
                            VBI_TRANSPARENT_COLOR;
                }
            } else {
                for (i = 0; i < 1056; i++)
                    page.text[i].background = VBI_TRANSPARENT_COLOR;
            }
        }
        csize = page.columns * page.rows * 12 * 10 * sizeof(vbi_rgba);
        if (csize == 0)
            break;
        if (csize > priv_vbi->canvas_size) {        // test canvas size
            if (priv_vbi->canvas)
                free(priv_vbi->canvas);
            priv_vbi->canvas = malloc(csize);
            priv_vbi->canvas_size = 0;
            if (priv_vbi->canvas)
                priv_vbi->canvas_size = csize;
        }
        if (priv_vbi->canvas) {
            vbi_draw_vt_page(&(page),
                             priv_vbi->fmt,
                             priv_vbi->canvas,
                             priv_vbi->reveal, priv_vbi->flash_on);
            priv_vbi->csize = csize;
        }
        priv_vbi->spudec_proc = 1;
        priv_vbi->valid_page = 1;
        break;
    }
}

/**
 * \brief Update page in cache
 * \param priv_vbi private data structure
 *
 * Routine also calls process_page to refresh currently visible page (if so)
 * every time it was received from VBI by background thread.
 *
 */
static void update_page(priv_vbi_t * priv_vbi)
{
    int i;
    int index;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    /*
       priv_vbi->redraw=1   - page redraw requested
       pgno!=page->pgno     - page was switched
       curr_pgno==pgno      - backgound process just fetched current page, refresh it
     */
    if (priv_vbi->redraw ||
        priv_vbi->pgno != priv_vbi->page->pgno ||
        priv_vbi->curr_pgno == priv_vbi->pgno) {
        index = vbi_bcd2dec(priv_vbi->pgno);
        if ( index <= 0 || index > 999 || !priv_vbi->cache[index]) {
            // curr_pgno is last decoded page
            index = vbi_bcd2dec(priv_vbi->curr_pgno);
        }

	if (index <=0 || index >999 || !priv_vbi->cache[index]){
            priv_vbi->valid_page = 0;
            memset(priv_vbi->page, 0, sizeof(vbi_page));
	}else
	{
            memcpy(priv_vbi->page, priv_vbi->cache[index], sizeof(vbi_page));
            process_page(priv_vbi);//prepare page to be shown on screen
	}
    }
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
}

/**
 * \brief background grabber routine
 * \param data user-defined data
 *
 */
static void *grabber(void *data)
{
    priv_vbi_t *user_vbi = (priv_vbi_t *) data;
    vbi_capture_buffer *sliced_buffer;
    struct timeval timeout;
    unsigned int n_lines;
    int r, err_count = 0;

    while (!user_vbi->eof) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 500;
        r = vbi_capture_pull(user_vbi->capture, NULL, &sliced_buffer, &timeout); // grab slices
        if (user_vbi->eof)
            return NULL;
        switch (r) {
        case -1: // read error
            if (err_count++ > 4)
                user_vbi->eof = 1;
            break;
        case 0:  // time out
            break;
        default:
            err_count = 0;
        }
        if (r != 1)
            continue;
        n_lines = sliced_buffer->size / sizeof(vbi_sliced);
        vbi_decode(user_vbi->decoder, (vbi_sliced *) sliced_buffer->data, 
            n_lines, sliced_buffer->timestamp); // decode slice
        update_page(user_vbi);
    }
    switch (r) {
    case -1:
        mp_msg(MSGT_TV, MSGL_ERR, "VBI read error %d (%s)\n",
               errno, strerror(errno));
        return NULL;
    case 0:
        mp_msg(MSGT_TV, MSGL_ERR, "VBI read timeout\n");
        return NULL;
    }
    return NULL;
}

/**
 * \brief calculate increased/decreased by given value page number
 * \param curr  current page number in hexadecimal for
 * \param direction decimal value (can be negative) to add to value or curr parameter
 * \return new page number in hexadecimal form
 *
 * VBI page numbers are represented in special hexadecimal form, e.g.
 * page with number 123 (as seen by user) internally has number 0x123.
 * and equation 0x123+8 should be equal to 0x131 instead of regular 0x12b.
 * Page numbers 0xYYY (where Y is not belongs to (0..9) and pages below 0x100 and
 * higher 0x999 are reserved for internal use.
 *
 */
static int steppage(int curr, int direction)
{
    int newpage = vbi_dec2bcd(vbi_bcd2dec(curr) + direction);
    if (newpage < 0x100)
        newpage = 0x100;
    if (newpage > 0x999)
        newpage = 0x999;
    return newpage;
}

/**
 * \brief toggles teletext page displaying mode
 * \param priv_vbi private data structure
 * \param flag new mode
 * \return 
 *   TVI_CONTROL_TRUE is success,
 *   TVI_CONTROL_FALSE otherwise
 *
 * flag:
 * 0 - off
 * 1 - on & opaque
 * 2 - on & transparent
 * 3 - on & transparent  with black foreground color (only in bw mode)
 *
 */
static int teletext_set_mode(priv_vbi_t * priv_vbi, int flag)
{
    if (flag<0 || flag>3)
        return TVI_CONTROL_FALSE;
        
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));

    priv_vbi->on = flag;

    if (priv_vbi->on > 2 && priv_vbi->tformat != VBI_TFORMAT_BW)
        priv_vbi->on = 0;

    priv_vbi->foreground = 0;
    priv_vbi->pagenumdec = 0;
    priv_vbi->spudec_proc = 1;
    priv_vbi->redraw = 1;
    switch (priv_vbi->on) {
    case 0:
        priv_vbi->csize = 0;
        break;
    case 1:
        priv_vbi->alpha = 0;
        break;
    case 2:
        priv_vbi->alpha = 1;
        break;
    case 3:
        priv_vbi->alpha = 1;
        priv_vbi->foreground = 1;
        break;
    }
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
    return TVI_CONTROL_TRUE;
}

/**
 * \brief get half page mode (only in SPU mode)
 * \param priv_vbi private data structure
 * \return current mode
 *     0 : half mode off
 *     1 : top half page
 *     2 : bottom half page
 */
static int vbi_get_half(priv_vbi_t * priv_vbi)
{
    int flag = 0;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    if (priv_vbi->valid_page)
        flag = priv_vbi->half;
    priv_vbi->pagenumdec = 0;
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
    return flag;
}

/**
 * \brief set half page mode (only in SPU mode)
 * \param priv_vbi private data structure
 * \param flag new half page mode
 * \return 
 *   TVI_CONTROL_TRUE is success,
 *   TVI_CONTROL_FALSE otherwise
 *
 *
 *  flag:
 *     0 : half mode off
 *     1 : top half page
 *     2 : bottom half page
 */
static int teletext_set_half_page(priv_vbi_t * priv_vbi, int flag)
{
    if (flag<0 || flag>2)
        return TVI_CONTROL_FALSE;

    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    priv_vbi->half = flag;
    if (priv_vbi->tformat == VBI_TFORMAT_TEXT && priv_vbi->half > 1)
        priv_vbi->half = 0;
    priv_vbi->redraw = 1;
    priv_vbi->pagenumdec = 0;
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
    return TVI_CONTROL_TRUE;
}

/**
 * \brief displays specified page
 * \param priv_vbi private data structure
 * \param pgno page number to display
 * \param subno subpage number
 *
 */
static void vbi_setpage(priv_vbi_t * priv_vbi, int pgno, int subno)
{
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    priv_vbi->pgno = steppage(0, pgno);
    priv_vbi->subno = subno;
    priv_vbi->redraw = 1;
    priv_vbi->pagenumdec = 0;
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
}

/**
 * \brief steps over pages by a given value
 * \param priv_vbi private data structure
 * \param direction decimal step value (can be negative)
 *
 */
static void vbi_steppage(priv_vbi_t * priv_vbi, int direction)
{
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    priv_vbi->pgno = steppage(priv_vbi->pgno, direction);
    priv_vbi->redraw = 1;
    priv_vbi->pagenumdec = 0;
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
}

/**
 * \brief append just entered digit to editing page number
 * \param priv_vbi private data structure
 * \param dec decimal digit to append
 *
 *  dec: 
 *   '0'..'9' append digit
 *    '-' remove last digit (backspace emulation)
 *
 * This routine allows user to jump to arbitrary page.
 * It implements simple page number editing algorithm.
 *
 * Subsystem can be on one of two modes: normal and page number edit mode.
 * Zero value of priv_vbi->pagenumdec means normal mode
 * Non-zero value means page number edit mode and equals to packed
 * decimal number of already entered part of page number.
 *
 * How this works.
 * Let's assume that current mode is normal (pagenumdec is zero), teletext page 
 * 100 are displayed as usual. topmost left corner of page contains page number.
 * Then vbi_add_dec is sequentally called (through slave 
 * command of course) with 1,4,-,2,3 * values of dec parameter.
 *
 * +-----+------------+------------------+
 * | dec | pagenumxec | displayed number |
 * +-----+------------+------------------+
 * |     | 0x000      | 100              | 
 * +-----+------------+------------------+
 * | 1   | 0x001      | __1              |
 * +-----+------------+------------------+
 * | 4   | 0x014      | _14              |
 * +-----+------------+------------------+
 * | -   | 0x001      | __1              |
 * +-----+------------+------------------+
 * | 2   | 0x012      | _12              |
 * +-----+------------+------------------+
 * | 3   | 0x123      | 123              |
 * +-----+------------+------------------+
 * |     | 0x000      | 123              | 
 * +-----+------------+------------------+
 *
 * pagenumdec will automatically receive zero value after third digit of page number
 * is entered and current page will be switched to another one with entered page number.
 *
 */
static void vbi_add_dec(priv_vbi_t * priv_vbi, char *dec)
{
    int count, shift;
    if (!dec)
        return;
    if (!priv_vbi->on)
        return;
    if ((*dec < '0' || *dec > '9') && *dec != '-')
        return;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    count = (priv_vbi->pagenumdec >> 12) & 0xf;
    if (*dec == '-') {
        count--;
        if (count)
            priv_vbi->pagenumdec = ((priv_vbi->pagenumdec >> 4) & 0xfff) | (count << 12);
        else
            priv_vbi->pagenumdec = 0;
    } else {
        shift = count * 4;
        count++;
        priv_vbi->pagenumdec =
            (((priv_vbi->pagenumdec) << 4 | (*dec -'0')) & 0xfff) | (count << 12);
        if (count == 3) {
            priv_vbi->pgno = priv_vbi->pagenumdec & 0xfff;
            priv_vbi->subno = 0;
            priv_vbi->redraw = 1;
            priv_vbi->pagenumdec = 0;
        }
    }
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
}

/**
 * \brief follows link specified on current page
 * \param priv_vbi private data structure
 * \param linkno link number (0..6)
 * \return 
 *    TVI_CONTROL_FALSE if linkno is outside 0..6 range or if
 *                      teletext is switched off
 *    TVI_CONTROL_TRUE otherwise
 *
 * linkno: 
 *    0: tpage in tv parameters (starting page, usually 100)
 * 1..6: follows link on current page with given number
 *
 * FIXME: quick test shows that this is working strange
 * FIXME: routine does not checks whether links exists on page or not
 * TODO: more precise look
 *
 */
static int vbi_golink(priv_vbi_t * priv_vbi, int linkno)
{
    if (linkno < 0 || linkno > 6)
        return TVI_CONTROL_FALSE;
    if (!priv_vbi->on)
        return TVI_CONTROL_FALSE;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    if (linkno == 0) {
        priv_vbi->pgno = priv_vbi->tpage;
        priv_vbi->subno = priv_vbi->page->nav_link[linkno].subno;
        priv_vbi->redraw = 1;
        priv_vbi->pagenumdec = 0;
    } else {
        linkno--;
        if (priv_vbi->pgno == priv_vbi->page->pgno) {
            priv_vbi->pgno = priv_vbi->page->nav_link[linkno].pgno;
            priv_vbi->subno = priv_vbi->page->nav_link[linkno].subno;
            priv_vbi->redraw = 1;
            priv_vbi->pagenumdec = 0;
        }
    }
    priv_vbi->pagenumdec = 0;
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
    return TVI_CONTROL_TRUE;
}

/**
 * \brief get pointer to current teletext page
 * \param priv_vbi private data structure
 * \return pointer to vbi_page structure if teletext is
 *         switched on and current page is valid, NULL - otherwise
 *    
 */
static vbi_page *vbi_getpage(priv_vbi_t * priv_vbi)
{
    vbi_page *page = NULL;

    if (!priv_vbi->on)
        return NULL;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    if (priv_vbi->valid_page)
        if (page = malloc(sizeof(vbi_page)))
            memcpy(page, priv_vbi->page, sizeof(vbi_page));
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
    return page;
}

/**
 * \brief get pointer to current teletext page
 * \param priv_vbi private data structure
 * \return pointer to character string, containing text-only data of
 *         teletext page. If teletext is switched off, current page is invalid
 *         or page format if not equal to "text" then returning value is NULL.
 * 
 */
static char *vbi_getpagetext(priv_vbi_t * priv_vbi)
{
    char *page = NULL;

    if (!priv_vbi->on)
        return NULL;
    if (priv_vbi->tformat != VBI_TFORMAT_TEXT && priv_vbi->canvas)
        return NULL;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    if (priv_vbi->valid_page)
        page = priv_vbi->txtpage;
    if (!page)
        page = priv_vbi->header;
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
    return page;
}

/**
 * \brief get current page RGBA32 image (only in SPU mode)
 * \param priv_vbi private data structure
 * \return pointer to tv_teletext_img_t structure, containing among
 *         other things rendered RGBA32 image of current teletext page.
 *         return NULL is image is not available for some reason.
 *
 */
static tv_teletext_img_t *vbi_getpageimg(priv_vbi_t * priv_vbi)
{
    tv_teletext_img_t *img = NULL;

    if (priv_vbi->tformat == VBI_TFORMAT_TEXT)
        return NULL;
    if (priv_vbi->spudec_proc == 0)
        return NULL;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    if (NULL != (img = malloc(sizeof(tv_teletext_img_t)))) {
        img->tformat = priv_vbi->tformat;        // format: bw|gray|color
        img->tformat = VBI_TFORMAT_GRAY;         // format: bw|gray|color
        img->half = priv_vbi->half;              // half mode
        img->columns = priv_vbi->columns;        // page size
        img->rows = priv_vbi->rows;
        img->width = priv_vbi->columns * 12;
        img->width = priv_vbi->rows * 10;
        img->canvas = NULL;
        // is page ok?
        if (priv_vbi->canvas && priv_vbi->on && priv_vbi->csize && priv_vbi->valid_page) { 
            
            if (NULL != (img->canvas = malloc(priv_vbi->csize)))
                memcpy(img->canvas, priv_vbi->canvas, priv_vbi->csize);
        }
    }
    priv_vbi->spudec_proc = 0;
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
    return img;
}

/**
 * \brief start teletext sybsystem
 * \param priv_vbi private data structure
 *
 * initializes cache, vbi decoder and starts background thread
 *
 */
static void vbi_start(priv_vbi_t * priv_vbi)
{
    if (!priv_vbi)
        return;
    if (NULL != (priv_vbi->txtpage = malloc(VBI_TXT_PAGE_SIZE)))        // alloc vbi_page
        memset(priv_vbi->txtpage, 0, VBI_TXT_PAGE_SIZE);
    priv_vbi->page = malloc(sizeof(vbi_page));
    priv_vbi->cache = (vbi_page **) malloc(1000 * sizeof(vbi_page *));
    memset(priv_vbi->cache, 0, 1000 * sizeof(vbi_page *));
    priv_vbi->decoder = vbi_decoder_new();
    priv_vbi->subno = 0;
    priv_vbi->fmt = VBI_PIXFMT_RGBA32_LE;
    memset(priv_vbi->theader, 0, sizeof(priv_vbi->theader));
    snprintf(priv_vbi->header, sizeof(priv_vbi->header), "%s", VBI_NO_TELETEXT);
    vbi_event_handler_add(priv_vbi->decoder, ~0, event_handler, (void *) priv_vbi);        // add event handler
    pthread_create(&priv_vbi->grabber_thread, NULL, grabber, priv_vbi);        // add grab function
    pthread_mutex_init(&priv_vbi->buffer_mutex, NULL);
    priv_vbi->valid_page = 0;
    priv_vbi->pagenumdec = 0;
    mp_msg(MSGT_TV, MSGL_INFO, "Teletext device: %s\n", priv_vbi->device);
}

/**
 * \brief Teletext reset
 * \param priv_vbi private data structure
 *
 * should be called during frequency, norm change, etc
 *
 */
static void vbi_reset(priv_vbi_t * priv_vbi)
{
    int i;
    pthread_mutex_lock(&(priv_vbi->buffer_mutex));
    if (priv_vbi->canvas)
        free(priv_vbi->canvas);
    priv_vbi->canvas = NULL;
    priv_vbi->canvas_size = 0;
    priv_vbi->redraw = 1;
    priv_vbi->csize = 0;
    priv_vbi->valid_page = 0;
    priv_vbi->spudec_proc = 1;
    priv_vbi->pagenumdec = 0;
    if (priv_vbi->page)
        memset(priv_vbi->page, 0, sizeof(vbi_page));
    if (priv_vbi->txtpage)
        memset(priv_vbi->txtpage, 0, VBI_TXT_PAGE_SIZE);
    memset(priv_vbi->theader, 0, sizeof(priv_vbi->theader));
    if (priv_vbi->cache) {
        for (i = 0; i < 1000; i++) {
            if (priv_vbi->cache[i])
                free(priv_vbi->cache[i]);
            priv_vbi->cache[i] = NULL;
        }
    }
    snprintf(priv_vbi->header, sizeof(priv_vbi->header), "%s",
             VBI_NO_TELETEXT);
    pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
}

/*
---------------------------------------------------------------------------------
    Public routines
---------------------------------------------------------------------------------
*/

/**
 * \brief teletext subsystem init
 * \note  Routine uses global variables tv_param_tdevice, tv_param_tpage
 *        and tv_param_tformat for initialization.
 *
 */
priv_vbi_t *teletext_init(void)
{
    priv_vbi_t *priv_vbi;
    int formatid, startpage;
    unsigned int services = VBI_SLICED_TELETEXT_B |
             VBI_SLICED_CAPTION_525 |
             VBI_SLICED_CAPTION_625 |
             VBI_SLICED_VBI_525 |
             VBI_SLICED_VBI_625 |
             VBI_SLICED_WSS_625 |
             VBI_SLICED_WSS_CPR1204 |
             VBI_SLICED_VPS;

    if (!tv_param_tdevice)
        return NULL;

    if (NULL == (priv_vbi = malloc(sizeof(priv_vbi_t))))
        return NULL;
    memset(priv_vbi, 0, sizeof(priv_vbi_t));
    formatid = VBI_TFORMAT_TEXT;         // default
    if (tv_param_tformat != NULL) {
        if (strcmp(tv_param_tformat, "text") == 0)
            formatid = VBI_TFORMAT_TEXT;
        if (strcmp(tv_param_tformat, "bw") == 0)
            formatid = VBI_TFORMAT_BW;
        if (strcmp(tv_param_tformat, "gray") == 0)
            formatid = VBI_TFORMAT_GRAY;
        if (strcmp(tv_param_tformat, "color") == 0)
            formatid = VBI_TFORMAT_COLOR;
    }
    startpage = steppage(0, tv_param_tpage);         // page number is HEX
    if (startpage < 0x100 || startpage > 0x999)
        startpage = 0x100;
    priv_vbi->device = strdup(tv_param_tdevice);
    priv_vbi->tformat = formatid;
    priv_vbi->tpage = startpage;  // page number
    priv_vbi->pgno = startpage;          // page number


    if (!priv_vbi->capture) {
        priv_vbi->services = services;  // probe v4l2
        priv_vbi->capture = vbi_capture_v4l2_new(priv_vbi->device,      // device 
                                                 20,                    // buffer numbers
                                                 &(priv_vbi->services), // services
                                                 0,                     // strict
                                                 &(priv_vbi->errstr),   // error string
                                                 0);                    // trace
    }
    services = priv_vbi->services;
    if (priv_vbi->capture == NULL) {
        priv_vbi->services = services;  // probe v4l
        priv_vbi->capture = vbi_capture_v4l_new(priv_vbi->device,
                                                20,
                                                &(priv_vbi->services),
                                                0, &(priv_vbi->errstr), 0);
    }

    if (!priv_vbi->capture) {
        free(priv_vbi->device);
        free(priv_vbi);
        mp_msg(MSGT_TV, MSGL_INFO, "No teletext\n");
        return NULL;
    }
    return priv_vbi;
}

/**
 * \brief teletext subsystem uninitialization
 * \param priv_vbi private data structure
 *
 * closes vbi capture, decode and and frees priv_vbi structure
 *
 */
void teletext_uninit(priv_vbi_t * priv_vbi)
{
    int i;
    if (priv_vbi == NULL)
        return;
    priv_vbi->eof = 1;
    if (priv_vbi->capture){
        vbi_capture_delete(priv_vbi->capture);
        priv_vbi->capture = NULL;
    }
    if (priv_vbi->decoder){
        vbi_event_handler_remove(priv_vbi->decoder, event_handler);
        vbi_decoder_delete(priv_vbi->decoder);
        priv_vbi->decoder = NULL;
    }
    if (priv_vbi->grabber_thread)
        pthread_join(priv_vbi->grabber_thread, NULL);
    pthread_mutex_destroy(&priv_vbi->buffer_mutex);
    if (priv_vbi->device){
        free(priv_vbi->device);
        priv_vbi->device = NULL;
    }
    if (priv_vbi->errstr){
        free(priv_vbi->errstr);
        priv_vbi->errstr = NULL;
    }
    if (priv_vbi->canvas){
        free(priv_vbi->canvas);
        priv_vbi->canvas = NULL;
    }
    if (priv_vbi->txtpage){
        free(priv_vbi->txtpage);
        priv_vbi->txtpage = NULL;
    }
    if (priv_vbi->network_name){
        free(priv_vbi->network_name);
        priv_vbi->network_name = NULL;
    }
    if (priv_vbi->network_id){
        free(priv_vbi->network_id);
        priv_vbi->network_id = NULL;
    }
    if (priv_vbi->page){
        free(priv_vbi->page);
        priv_vbi->page = NULL;
    }
    if (priv_vbi->cache) {
        for (i = 0; i < 1000; i++) {
            if (priv_vbi->cache[i])
                free(priv_vbi->cache[i]);
        }
        free(priv_vbi->cache);
        priv_vbi->cache = NULL;
    }
    free(priv_vbi);
}

/**
 * \brief Teletext control routine
 * \param priv_vbi private data structure
 * \param cmd command 
 * \param arg command parameter (has to be not null)
 *
 */
int teletext_control(priv_vbi_t * priv_vbi, int cmd, void *arg)
{
    vbi_page *page = NULL;
    char *txtpage = NULL;
    tv_teletext_img_t *img = NULL;
    if (!priv_vbi)
        return TVI_CONTROL_FALSE;
    if (!arg)
        return TVI_CONTROL_FALSE;
    switch (cmd) {
    case TVI_CONTROL_VBI_RESET:
        vbi_reset(priv_vbi);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_START:
        vbi_start(priv_vbi);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_GET_FORMAT:
        pthread_mutex_lock(&(priv_vbi->buffer_mutex));
        *(int*)arg=priv_vbi->tformat;
        pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
	return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_SET_MODE:
        return teletext_set_mode(priv_vbi, *(int *) arg);
    case TVI_CONTROL_VBI_GET_MODE:
        pthread_mutex_lock(&(priv_vbi->buffer_mutex));
        *(int*)arg=priv_vbi->on;
        pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
	return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_STEP_MODE:
    {
        int val;
        pthread_mutex_lock(&(priv_vbi->buffer_mutex));
	val=(priv_vbi->on+*(int*)arg)%4;
        pthread_mutex_unlock(&(priv_vbi->buffer_mutex));
	if (val<0)
	    val+=4;
	return teletext_set_mode(priv_vbi,val);
    }
    case TVI_CONTROL_VBI_GET_HALF_PAGE:
        *(void **) arg = (void *) vbi_get_half(priv_vbi);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_SET_HALF_PAGE:
        return teletext_set_half_page(priv_vbi, *(int *) arg);
    case TVI_CONTROL_VBI_STEP_HALF_PAGE:
    {
        int val;
	val=(vbi_get_half(priv_vbi)+*(int*)arg)%3;
	
	if (val<0)
	    val+=3;
	return teletext_set_half_page(priv_vbi,val);
    }

    case TVI_CONTROL_VBI_SET_PAGE:
        vbi_setpage(priv_vbi, *(int *) arg, 0);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_STEP_PAGE:
        vbi_steppage(priv_vbi, *(int *) arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_ADD_DEC:
        vbi_add_dec(priv_vbi, *(char **) arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_GO_LINK:
        return vbi_golink(priv_vbi, *(int *) arg);
    case TVI_CONTROL_VBI_GET_PAGE:
        *(int*) arg = priv_vbi->pgno;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_GET_VBIPAGE:
        if (NULL == (page = vbi_getpage(priv_vbi)))
            return TVI_CONTROL_FALSE;
        *(void **) arg = (void *) page;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_GET_TXTPAGE:
        if (NULL == (txtpage = vbi_getpagetext(priv_vbi)))
            return TVI_CONTROL_FALSE;
        *(void **) arg = (void *) txtpage;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VBI_GET_IMGPAGE:
        if (NULL == (img = vbi_getpageimg(priv_vbi)))
            return TVI_CONTROL_FALSE;
        *(void **) arg = (void *) img;
        return TVI_CONTROL_TRUE;
    }
    return TVI_CONTROL_UNKNOWN;
}
