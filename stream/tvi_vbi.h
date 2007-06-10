#ifndef __TVI_VBI_H_
#define __TVI_VBI_H_

#include "libzvbi.h"
#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"
#include "tv.h"

#define VBI_MAX_SUBPAGES   64               ///< max sub pages number
#define VBI_TXT_PAGE_SIZE  42*25*2          ///< max text page size
#define VBI_MAX_LINE_SIZE  42               ///< max line size in text page

#define VBI_TFORMAT_TEXT    0               ///< text mode
#define VBI_TFORMAT_BW      1               ///< back&white mode
#define VBI_TFORMAT_GRAY    2               ///< grayscale mode
#define VBI_TFORMAT_COLOR   3               ///< color mode (require color_spu patch!)

#define VBI_NO_TELETEXT    "No teletext"

#define VBI_TRANSPARENT_COLOR    40         ///< transparent color id
#define VBI_TIME_LINEPOS    13              ///< time line pos in page header

typedef struct {
    int            on;                      ///< teletext on/off

    char*        device;                    ///< capture device
    unsigned int    services;               ///< services
    vbi_capture*    capture;                ///< vbi_capture
    int            capture_fd;              ///< capture fd (now not used)
    vbi_decoder*    decoder;                ///< vbi_decoder
    char*        errstr;                    ///< error string
    pthread_t        grabber_thread;        ///< grab thread
    pthread_mutex_t    buffer_mutex;
    pthread_mutex_t    update_mutex;
    int            eof;                     ///< end grab
    int           tpage;                    ///< tpage
    int            pgno;                    ///< seek page number
    int            subno;                   ///< seek subpage
    int            curr_pgno;               ///< current page number
    int            curr_subno;              ///< current subpage
    uint32_t       pagenumdec;              ///< set page num with dec

    vbi_page** cache;
    vbi_page         *page;                 ///< vbi_page
    int            valid_page;              ///< valid page flag
    char*        txtpage;                   ///< decoded vbi_page to text
    vbi_char    theader[VBI_MAX_LINE_SIZE]; ///< vbi header
    char        header[VBI_MAX_LINE_SIZE];  ///< text header

    int            tformat;                 ///< 0:text, 1:bw, 2:gray, 3:color
    vbi_pixfmt         fmt;                 ///< image format (only VBI_PIXFMT_RGBA32_LE supported)
    void*        canvas;                    ///< stored image data
    int            csize;                   ///< stored image size
    int            canvas_size;             ///< image buffer size
    int            reveal;                  ///< reveal (now not used)
    int            flash_on;                ///< flash_on (now not used)
    int            alpha;                   ///< opacity mode
    int            foreground;              ///< foreground black in bw mode
    int            half;                    ///< 0:half mode off, 1:top half page, 2:bottom half page
    int            redraw;                  ///< is redraw last image
    int            columns;                 ///< page size: coloumns
    int            rows;                    ///< page size: rows
    int            spudec_proc;             ///< render image request

    char*        network_name;              ///< network name
    char*        network_id;                ///< network id
    } priv_vbi_t;

/// teletext subsystem initialization
priv_vbi_t* teletext_init(void);
/// teletext subsystem uninitialization
void teletext_uninit(priv_vbi_t* priv_vbi);
/// ioctl for 
int teletext_control(priv_vbi_t* priv_vbi, int cmd, void *args);
#endif
