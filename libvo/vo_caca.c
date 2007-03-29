/*
 * MPlayer
 * 
 * Video driver for libcaca
 * 
 * by Pigeon <pigeon@pigeond.net>
 *
 * Some functions/codes/ideas are from x11 and aalib vo
 *
 * TODO: support those draw_alpha stuff?
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "sub.h"

#include "osdep/keycodes.h"
#include "mp_msg.h"
#include "mp_fifo.h"

#include <caca.h>
#ifdef CACA_API_VERSION_1
  /* Include the pre-1.x compatibility header.
   * Once libcaca 1.x is widespread, vo_caca should be fully
   * converted to the new API. A patch exists:
   * http://lists.mplayerhq.hu/pipermail/mplayer-dev-eng/2006-July/044674.html
   */
  #include <caca0.h>
#endif

static vo_info_t info = {
  "libcaca",
  "caca",
  "Pigeon <pigeon@pigeond.net>",
  ""
};

LIBVO_EXTERN (caca)

/* caca stuff */
static struct caca_bitmap *cbitmap = NULL;

/* image infos */
static int image_format;
static int image_width;
static int image_height;

static int screen_w, screen_h;

/* We want 24bpp always for now */
static unsigned int bpp = 24;
static unsigned int depth = 3;
static unsigned int rmask = 0xff0000;
static unsigned int gmask = 0x00ff00;
static unsigned int bmask = 0x0000ff;
static unsigned int amask = 0;

#define MESSAGE_SIZE 512
#define MESSAGE_DURATION 5

static time_t stoposd = 0;
static int showosdmessage = 0;
static char osdmessagetext[MESSAGE_SIZE];
static char posbar[MESSAGE_SIZE];

static int osdx = 0, osdy = 0;
static int posbary = 2;

static void osdmessage(int duration, const char *fmt, ...)
{
    /*
     * for outputting a centered string at the bottom
     * of our window for a while
     */
    va_list ar;
    char m[MESSAGE_SIZE];

    va_start(ar, fmt);
    vsprintf(m, fmt, ar);
    va_end(ar);
    strcpy(osdmessagetext, m);

    showosdmessage = 1;
    stoposd = time(NULL) + duration;
    osdx = (screen_w - strlen (osdmessagetext)) / 2;
    posbar[0] = '\0';
}

static void osdpercent(int duration, int min, int max, int val, const char *desc, const char *unit)
{
    /*
     * prints a bar for setting values
     */
    float step;
    int where, i;

    step = (float)screen_w / (float)(max - min);
    where = (val - min) * step;
    osdmessage (duration, "%s: %i%s", desc, val, unit);
    posbar[0] = '|';
    posbar[screen_w - 1] = '|';

    for (i = 0; i < screen_w; i++)
    {
	if (i == where)
	    posbar[i] = '#';
	else
	    posbar[i] = '-';
    }
    
    if (where != 0)
	posbar[0] = '|';

    if (where != (screen_w - 1))
	posbar[screen_w - 1] = '|';

    posbar[screen_w] = '\0';
}

static int resize ()
{
    screen_w = caca_get_width();
    screen_h = caca_get_height();

    if (cbitmap)
	caca_free_bitmap(cbitmap);

    cbitmap = caca_create_bitmap(bpp, image_width, image_height,
				depth * image_width, rmask, gmask, bmask,
				amask);

    if (!cbitmap)
	mp_msg(MSGT_VO, MSGL_FATAL, "vo_caca: caca_create_bitmap failed!\n");

    return 0;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
	uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    image_height = height;
    image_width = width;
    image_format = format;
    
    showosdmessage = 0;
    posbar[0] = '\0';

    return resize ();
}

static int draw_frame(uint8_t *src[])
{
    caca_draw_bitmap(0, 0, screen_w, screen_h, cbitmap, src[0]);
    return 0;
}

static int draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
    return 0;
}

static void flip_page(void)
{

    if (showosdmessage)
    {
	if (time(NULL) >= stoposd)
	{
	    showosdmessage = 0;
	    if (posbar)
		posbar[0] = '\0';
	} else {
	    caca_putstr(osdx, osdy, osdmessagetext);
	    
	    if (posbar)
		caca_putstr(0, posbary, posbar);
	}
    }

    caca_refresh();
}

static void check_events (void)
{
    unsigned int cev;

    if ((cev = caca_get_event(CACA_EVENT_ANY)))
    {
	if (cev & CACA_EVENT_RESIZE)
	{
	    caca_refresh();
	    resize();
	} else if (cev & CACA_EVENT_KEY_RELEASE)
	{
	    int key = (cev & 0x00ffffff);
	    enum caca_feature cf;

	    switch (key) {
	    case 'd':
	    case 'D':
	      /* Toggle dithering method */
	      cf = 1 + caca_get_feature(CACA_DITHERING);
	      if (cf > CACA_DITHERING_MAX)
		  cf = CACA_DITHERING_MIN;
	      caca_set_feature(cf);
	      osdmessage(MESSAGE_DURATION, "Using %s", caca_get_feature_name(cf));
	      break;

	    case 'a':
	    case 'A':
	      /* Toggle antialiasing method */
	      cf = 1 + caca_get_feature(CACA_ANTIALIASING);
	      if (cf > CACA_ANTIALIASING_MAX)
		  cf = CACA_ANTIALIASING_MIN;
	      caca_set_feature(cf);
	      osdmessage(MESSAGE_DURATION, "Using %s", caca_get_feature_name(cf));
	      break;

	    case 'b':
	    case 'B':
	      /* Toggle background method */
	      cf = 1 + caca_get_feature(CACA_BACKGROUND);
	      if (cf > CACA_BACKGROUND_MAX)
		  cf = CACA_BACKGROUND_MIN;
	      caca_set_feature(cf);
	      osdmessage(MESSAGE_DURATION, "Using %s", caca_get_feature_name(cf));
	      break;

	    case CACA_KEY_UP:
	      mplayer_put_key(KEY_UP);
	      break;
	    case CACA_KEY_DOWN:
	      mplayer_put_key(KEY_DOWN);
	      break;
	    case CACA_KEY_LEFT:
	      mplayer_put_key(KEY_LEFT);
	      break;
	    case CACA_KEY_RIGHT:
	      mplayer_put_key(KEY_RIGHT);
	      break;
	    case CACA_KEY_ESCAPE:
	      mplayer_put_key(KEY_ESC);
	      break;
	    case CACA_KEY_PAGEUP:
	      mplayer_put_key(KEY_PAGE_UP);
	      break;
	    case CACA_KEY_PAGEDOWN:
	      mplayer_put_key(KEY_PAGE_DOWN);
	      break;
	    case CACA_KEY_RETURN:
	      mplayer_put_key(KEY_ENTER);
	      break;
	    case CACA_KEY_HOME:
	      mplayer_put_key(KEY_HOME);
	      break;
	    case CACA_KEY_END:
	      mplayer_put_key(KEY_END);
	      break;
	    default:
	      if (key <= 255)
		  mplayer_put_key (key);
	      break;
	    }
	}
    }
}

static void uninit(void)
{
    caca_free_bitmap(cbitmap);
    cbitmap = NULL;
    caca_end();
}


static void draw_osd(void)
{
    if (vo_osd_progbar_type != -1)
	osdpercent(MESSAGE_DURATION, 0, 255,
		  vo_osd_progbar_value, __sub_osd_names[vo_osd_progbar_type],
		  "");
}

static int preinit(const char *arg)
{
    if (arg)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "vo_caca: Unknown subdevice: %s\n", arg);
	return ENOSYS;
    }

    if (caca_init())
    {
	mp_msg(MSGT_VO, MSGL_ERR, "vo_caca: failed to initialize\n");
	return ENOSYS;
    }

    caca_set_window_title("MPlayer");

    /* Default libcaca features */
    caca_set_feature(CACA_ANTIALIASING_PREFILTER);
    caca_set_feature(CACA_DITHERING_RANDOM);

    return 0;
}

static int query_format(uint32_t format)
{
    if (format == IMGFMT_BGR24)
      return VFCAP_OSD | VFCAP_CSP_SUPPORTED;

    return 0;
}

static int control(uint32_t request, void *data, ...)
{
    switch(request)
    {
    case VOCTRL_QUERY_FORMAT:
      return query_format(*((uint32_t *)data));
    default:
      return VO_NOTIMPL;
    }
}
