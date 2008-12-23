
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"


#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <errno.h>

#include "x11_common.h"

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

static int Shmem_Flag;

//static int Quiet_Flag;  Here also what is this for. It's used but isn't initialized?
static XShmSegmentInfo Shminfo[1];
static int gXErrorFlag;
static int CompletionType = -1;
#endif

#include "sub.h"

#include "libswscale/swscale.h"
#include "libmpcodecs/vf_scale.h"
#define MODE_RGB  0x1
#define MODE_BGR  0x2

#include "mp_msg.h"
#include "help_mp.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#include "mplayer.h"
#endif

static const vo_info_t info = {
    "X11 ( XImage/Shm )",
    "x11",
    "Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
    ""
};

const LIBVO_EXTERN(x11)
/* private prototypes */
static void (*draw_alpha_fnc) (int x0, int y0, int w, int h,
                               unsigned char *src, unsigned char *srca,
                               int stride);

/* local data */
static unsigned char *ImageData;
//! original unaligned pointer for free
static unsigned char *ImageDataOrig;

/* X11 related variables */
static XImage *myximage = NULL;
static int depth, bpp;
static XWindowAttributes attribs;

static int int_pause;

static int Flip_Flag;
static int zoomFlag;


static uint32_t image_width;
static uint32_t image_height;
static uint32_t in_format;
static uint32_t out_format = 0;
static int out_offset;
static int srcW = -1;
static int srcH = -1;

static int old_vo_dwidth = -1;
static int old_vo_dheight = -1;

static void check_events(void)
{
    int ret = vo_x11_check_events(mDisplay);

    if (ret & VO_EVENT_RESIZE)
        vo_x11_clearwindow(mDisplay, vo_window);
    else if (ret & VO_EVENT_EXPOSE)
        vo_x11_clearwindow_part(mDisplay, vo_window, myximage->width,
                                myximage->height, 0);
    if (ret & VO_EVENT_EXPOSE && int_pause)
        flip_page();
}

static void draw_alpha_32(int x0, int y0, int w, int h, unsigned char *src,
                          unsigned char *srca, int stride)
{
    vo_draw_alpha_rgb32(w, h, src, srca, stride,
                        ImageData + 4 * (y0 * image_width + x0),
                        4 * image_width);
}

static void draw_alpha_24(int x0, int y0, int w, int h, unsigned char *src,
                          unsigned char *srca, int stride)
{
    vo_draw_alpha_rgb24(w, h, src, srca, stride,
                        ImageData + 3 * (y0 * image_width + x0),
                        3 * image_width);
}

static void draw_alpha_16(int x0, int y0, int w, int h, unsigned char *src,
                          unsigned char *srca, int stride)
{
    vo_draw_alpha_rgb16(w, h, src, srca, stride,
                        ImageData + 2 * (y0 * image_width + x0),
                        2 * image_width);
}

static void draw_alpha_15(int x0, int y0, int w, int h, unsigned char *src,
                          unsigned char *srca, int stride)
{
    vo_draw_alpha_rgb15(w, h, src, srca, stride,
                        ImageData + 2 * (y0 * image_width + x0),
                        2 * image_width);
}

static void draw_alpha_null(int x0, int y0, int w, int h,
                            unsigned char *src, unsigned char *srca,
                            int stride)
{
}

static struct SwsContext *swsContext = NULL;
static int dst_width;
extern int sws_flags;

static XVisualInfo vinfo;

static void getMyXImage(void)
{
#ifdef HAVE_SHM
    if (mLocalDisplay && XShmQueryExtension(mDisplay))
        Shmem_Flag = 1;
    else
    {
        Shmem_Flag = 0;
        mp_msg(MSGT_VO, MSGL_WARN,
               "Shared memory not supported\nReverting to normal Xlib\n");
    }
    if (Shmem_Flag)
        CompletionType = XShmGetEventBase(mDisplay) + ShmCompletion;

    if (Shmem_Flag)
    {
        myximage =
            XShmCreateImage(mDisplay, vinfo.visual, depth, ZPixmap, NULL,
                            &Shminfo[0], image_width, image_height);
        if (myximage == NULL)
        {
            mp_msg(MSGT_VO, MSGL_WARN,
                   "Shared memory error,disabling ( Ximage error )\n");
            goto shmemerror;
        }
        Shminfo[0].shmid = shmget(IPC_PRIVATE,
                                  myximage->bytes_per_line *
                                  myximage->height, IPC_CREAT | 0777);
        if (Shminfo[0].shmid < 0)
        {
            XDestroyImage(myximage);
            mp_msg(MSGT_VO, MSGL_V, "%s\n", strerror(errno));
            //perror( strerror( errno ) );
            mp_msg(MSGT_VO, MSGL_WARN,
                   "Shared memory error,disabling ( seg id error )\n");
            goto shmemerror;
        }
        Shminfo[0].shmaddr = (char *) shmat(Shminfo[0].shmid, 0, 0);

        if (Shminfo[0].shmaddr == ((char *) -1))
        {
            XDestroyImage(myximage);
            if (Shminfo[0].shmaddr != ((char *) -1))
                shmdt(Shminfo[0].shmaddr);
            mp_msg(MSGT_VO, MSGL_WARN,
                   "Shared memory error,disabling ( address error )\n");
            goto shmemerror;
        }
        myximage->data = Shminfo[0].shmaddr;
        ImageData = (unsigned char *) myximage->data;
        Shminfo[0].readOnly = False;
        XShmAttach(mDisplay, &Shminfo[0]);

        XSync(mDisplay, False);

        if (gXErrorFlag)
        {
            XDestroyImage(myximage);
            shmdt(Shminfo[0].shmaddr);
            mp_msg(MSGT_VO, MSGL_WARN, "Shared memory error,disabling.\n");
            gXErrorFlag = 0;
            goto shmemerror;
        } else
            shmctl(Shminfo[0].shmid, IPC_RMID, 0);

        {
            static int firstTime = 1;

            if (firstTime)
            {
                mp_msg(MSGT_VO, MSGL_V, "Sharing memory.\n");
                firstTime = 0;
            }
        }
    } else
    {
      shmemerror:
        Shmem_Flag = 0;
#endif
        myximage = XCreateImage(mDisplay, vinfo.visual, depth, ZPixmap,
                             0, NULL, image_width, image_height, 8, 0);
        ImageDataOrig = malloc(myximage->bytes_per_line * image_height + 32);
        myximage->data = ImageDataOrig + 16 - ((long)ImageDataOrig & 15);
        memset(myximage->data, 0, myximage->bytes_per_line * image_height);
        ImageData = myximage->data;
#ifdef HAVE_SHM
    }
#endif
}

static void freeMyXImage(void)
{
#ifdef HAVE_SHM
    if (Shmem_Flag)
    {
        XShmDetach(mDisplay, &Shminfo[0]);
        XDestroyImage(myximage);
        shmdt(Shminfo[0].shmaddr);
    } else
#endif
    {
        myximage->data = ImageDataOrig;
        XDestroyImage(myximage);
        ImageDataOrig = NULL;
    }
    myximage = NULL;
    ImageData = NULL;
}

#ifdef WORDS_BIGENDIAN
#define BO_NATIVE    MSBFirst
#define BO_NONNATIVE LSBFirst
#else
#define BO_NATIVE    LSBFirst
#define BO_NONNATIVE MSBFirst
#endif
const struct fmt2Xfmtentry_s {
  uint32_t mpfmt;
  int byte_order;
  unsigned red_mask;
  unsigned green_mask;
  unsigned blue_mask;
} fmt2Xfmt[] = {
  {IMGFMT_RGB8,  BO_NATIVE,    0x00000007, 0x00000038, 0x000000C0},
  {IMGFMT_RGB8,  BO_NONNATIVE, 0x00000007, 0x00000038, 0x000000C0},
  {IMGFMT_BGR8,  BO_NATIVE,    0x000000E0, 0x0000001C, 0x00000003},
  {IMGFMT_BGR8,  BO_NONNATIVE, 0x000000E0, 0x0000001C, 0x00000003},
  {IMGFMT_RGB15, BO_NATIVE,    0x0000001F, 0x000003E0, 0x00007C00},
  {IMGFMT_BGR15, BO_NATIVE,    0x00007C00, 0x000003E0, 0x0000001F},
  {IMGFMT_RGB16, BO_NATIVE,    0x0000001F, 0x000007E0, 0x0000F800},
  {IMGFMT_BGR16, BO_NATIVE,    0x0000F800, 0x000007E0, 0x0000001F},
  {IMGFMT_RGB24, MSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
  {IMGFMT_RGB24, LSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
  {IMGFMT_BGR24, MSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
  {IMGFMT_BGR24, LSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
  {IMGFMT_RGB32, BO_NATIVE,    0x000000FF, 0x0000FF00, 0x00FF0000},
  {IMGFMT_RGB32, BO_NONNATIVE, 0xFF000000, 0x00FF0000, 0x0000FF00},
  {IMGFMT_BGR32, BO_NATIVE,    0x00FF0000, 0x0000FF00, 0x000000FF},
  {IMGFMT_BGR32, BO_NONNATIVE, 0x0000FF00, 0x00FF0000, 0xFF000000},
  {IMGFMT_ARGB,  MSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
  {IMGFMT_ARGB,  LSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
  {IMGFMT_ABGR,  MSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
  {IMGFMT_ABGR,  LSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
  {IMGFMT_RGBA,  MSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
  {IMGFMT_RGBA,  LSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
  {IMGFMT_BGRA,  MSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
  {IMGFMT_BGRA,  LSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
  {0, 0, 0, 0, 0}
};

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t flags, char *title,
                       uint32_t format)
{
// int screen;

// int interval, prefer_blank, allow_exp, nothing;
    unsigned int fg, bg;
    Colormap theCmap;
    XSetWindowAttributes xswa;
    unsigned long xswamask;
    const struct fmt2Xfmtentry_s *fmte = fmt2Xfmt;

#ifdef CONFIG_XF86VM
    int vm = flags & VOFLAG_MODESWITCHING;
#endif
    Flip_Flag = flags & VOFLAG_FLIPPING;
    zoomFlag = flags & VOFLAG_SWSCALE;

    old_vo_dwidth = -1;
    old_vo_dheight = -1;

    int_pause = 0;
    if (!title)
        title = "MPlayer X11 (XImage/Shm) render";

    in_format = format;
    srcW = width;
    srcH = height;

    XGetWindowAttributes(mDisplay, mRootWin, &attribs);
    depth = attribs.depth;

    if (depth != 15 && depth != 16 && depth != 24 && depth != 32)
    {
        Visual *visual;

        depth = vo_find_depth_from_visuals(mDisplay, mScreen, &visual);
    }
    if (!XMatchVisualInfo(mDisplay, mScreen, depth, DirectColor, &vinfo) ||
        (WinID > 0
         && vinfo.visualid != XVisualIDFromVisual(attribs.visual)))
        XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

    /* set image size (which is indeed neither the input nor output size), 
       if zoom is on it will be changed during draw_slice anyway so we don't duplicate the aspect code here 
     */
    image_width = (width + 7) & (~7);
    image_height = height;

#ifdef CONFIG_GUI
    if (use_gui)
        guiGetEvent(guiSetShVideo, 0);  // the GUI will set up / resize the window
    else
#endif
    {
#ifdef CONFIG_XF86VM
        if (vm)
        {
            vo_vm_switch();
        }
#endif
        bg = WhitePixel(mDisplay, mScreen);
        fg = BlackPixel(mDisplay, mScreen);

        theCmap = vo_x11_create_colormap(&vinfo);

        xswa.background_pixel = 0;
        xswa.border_pixel = 0;
        xswa.colormap = theCmap;
        xswamask = CWBackPixel | CWBorderPixel | CWColormap;

#ifdef CONFIG_XF86VM
        if (vm)
        {
            xswa.override_redirect = True;
            xswamask |= CWOverrideRedirect;
        }
#endif

            vo_x11_create_vo_window(&vinfo, vo_dx, vo_dy, vo_dwidth, vo_dheight,
                    flags, theCmap, "x11", title);
        if (WinID > 0)
            depth = vo_x11_update_geometry();

#ifdef CONFIG_XF86VM
        if (vm)
        {
            /* Grab the mouse pointer in our window */
            if (vo_grabpointer)
                XGrabPointer(mDisplay, vo_window, True, 0,
                             GrabModeAsync, GrabModeAsync,
                             vo_window, None, CurrentTime);
            XSetInputFocus(mDisplay, vo_window, RevertToNone, CurrentTime);
        }
#endif
    }

    if (myximage)
    {
        freeMyXImage();
        sws_freeContext(swsContext);
    }
    getMyXImage();

    while (fmte->mpfmt) {
      int depth = IMGFMT_RGB_DEPTH(fmte->mpfmt);
      /* bits_per_pixel in X seems to be set to 16 for 15 bit formats
         => force depth to 16 so that only the color masks are used for the format check */
      if (depth == 15)
          depth = 16;

      if (depth            == myximage->bits_per_pixel &&
          fmte->byte_order == myximage->byte_order &&
          fmte->red_mask   == myximage->red_mask   &&
          fmte->green_mask == myximage->green_mask &&
          fmte->blue_mask  == myximage->blue_mask)
        break;
      fmte++;
    }
    if (!fmte->mpfmt) {
      mp_msg(MSGT_VO, MSGL_ERR,
             "X server image format not supported, please contact the developers\n");
      return -1;
    }
    out_format = fmte->mpfmt;
    switch ((bpp = myximage->bits_per_pixel))
    {
        case 24:
            draw_alpha_fnc = draw_alpha_24;
            break;
        case 32:
            draw_alpha_fnc = draw_alpha_32;
            break;
        case 15:
        case 16:
            if (depth == 15)
                draw_alpha_fnc = draw_alpha_15;
            else
                draw_alpha_fnc = draw_alpha_16;
            break;
        default:
            draw_alpha_fnc = draw_alpha_null;
    }
    out_offset = 0;
    // for these formats conversion is currently not support and
    // we can easily "emulate" them.
    if (out_format & 64 && (IMGFMT_IS_RGB(out_format) || IMGFMT_IS_BGR(out_format))) {
      out_format &= ~64;
#ifdef WORDS_BIGENDIAN
      out_offset = 1;
#else
      out_offset = -1;
#endif
    }

    /* always allocate swsContext as size could change between frames */
    swsContext =
        sws_getContextFromCmdLine(width, height, in_format, width, height,
                                  out_format);
    if (!swsContext)
        return -1;

    dst_width = width;
    //printf( "X11 bpp: %d  color mask:  R:%lX  G:%lX  B:%lX\n",bpp,myximage->red_mask,myximage->green_mask,myximage->blue_mask );

    return 0;
}

static void Display_Image(XImage * myximage, uint8_t * ImageData)
{
    int x = (vo_dwidth - dst_width) / 2;
    int y = (vo_dheight - myximage->height) / 2;

    // do not draw if the image needs rescaling
    if ((old_vo_dwidth != vo_dwidth || old_vo_dheight != vo_dheight) && zoomFlag)
      return;

    if (WinID == 0) {
      x = vo_dx;
      y = vo_dy;
    }
    myximage->data += out_offset;
#ifdef HAVE_SHM
    if (Shmem_Flag)
    {
        XShmPutImage(mDisplay, vo_window, vo_gc, myximage,
                     0, 0,
                     x, y, dst_width,
                     myximage->height, True);
    } else
#endif
    {
        XPutImage(mDisplay, vo_window, vo_gc, myximage,
                  0, 0,
                  x, y, dst_width,
                  myximage->height);
    }
    myximage->data -= out_offset;
}

static void draw_osd(void)
{
    vo_draw_text(image_width, image_height, draw_alpha_fnc);
}

static void flip_page(void)
{
    Display_Image(myximage, ImageData);
    XSync(mDisplay, False);
}

static int draw_slice(uint8_t * src[], int stride[], int w, int h,
                           int x, int y)
{
    uint8_t *dst[3];
    int dstStride[3];

    if ((old_vo_dwidth != vo_dwidth
         || old_vo_dheight != vo_dheight) /*&& y==0 */  && zoomFlag)
    {
        int newW = vo_dwidth;
        int newH = vo_dheight;
        struct SwsContext *oldContext = swsContext;

        old_vo_dwidth = vo_dwidth;
        old_vo_dheight = vo_dheight;

        if (vo_fs)
            aspect(&newW, &newH, A_ZOOM);
        if (sws_flags == 0)
            newW &= (~31);      // not needed but, if the user wants the FAST_BILINEAR SCALER, then its needed

        swsContext = sws_getContextFromCmdLine(srcW, srcH, in_format,
                                               newW, newH, out_format);
        if (swsContext)
        {
            image_width = (newW + 7) & (~7);
            image_height = newH;

            freeMyXImage();
            getMyXImage();
            sws_freeContext(oldContext);
        } else
        {
            swsContext = oldContext;
        }
        dst_width = newW;
    }
    dstStride[1] = dstStride[2] = 0;
    dst[1] = dst[2] = NULL;

    dstStride[0] = image_width * ((bpp + 7) / 8);
    dst[0] = ImageData;
    if (Flip_Flag)
    {
        dst[0] += dstStride[0] * (image_height - 1);
        dstStride[0] = -dstStride[0];
    }
    sws_scale_ordered(swsContext, src, stride, y, h, dst, dstStride);
    return 0;
}

static int draw_frame(uint8_t * src[])
{
    return VO_ERROR;
}

static uint32_t get_image(mp_image_t * mpi)
{
    if (zoomFlag ||
        !IMGFMT_IS_BGR(mpi->imgfmt) ||
        (IMGFMT_BGR_DEPTH(mpi->imgfmt) != vo_depthonscreen) ||
        ((mpi->type != MP_IMGTYPE_STATIC)
         && (mpi->type != MP_IMGTYPE_TEMP))
        || (mpi->flags & MP_IMGFLAG_PLANAR)
        || (mpi->flags & MP_IMGFLAG_YUV) || (mpi->width != image_width)
        || (mpi->height != image_height))
        return VO_FALSE;

    if (Flip_Flag)
    {
        mpi->stride[0] = -image_width * ((bpp + 7) / 8);
        mpi->planes[0] = ImageData - mpi->stride[0] * (image_height - 1);
    } else
    {
        mpi->stride[0] = image_width * ((bpp + 7) / 8);
        mpi->planes[0] = ImageData;
    }
    mpi->flags |= MP_IMGFLAG_DIRECT;

    return VO_TRUE;
}

static int query_format(uint32_t format)
{
    mp_msg(MSGT_VO, MSGL_DBG2,
           "vo_x11: query_format was called: %x (%s)\n", format,
           vo_format_name(format));
    if (IMGFMT_IS_BGR(format))
    {
        if (IMGFMT_BGR_DEPTH(format) <= 8)
            return 0;           // TODO 8bpp not yet fully implemented
        if (IMGFMT_BGR_DEPTH(format) == vo_depthonscreen)
            return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_SWSCALE | VFCAP_FLIP |
                VFCAP_ACCEPT_STRIDE;
        else
            return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_SWSCALE | VFCAP_FLIP |
                VFCAP_ACCEPT_STRIDE;
    }

    switch (format)
    {
//   case IMGFMT_BGR8:  
//   case IMGFMT_BGR15:
//   case IMGFMT_BGR16:
//   case IMGFMT_BGR24:
//   case IMGFMT_BGR32:
//    return 0x2;
//   case IMGFMT_YUY2: 
        case IMGFMT_I420:
        case IMGFMT_IYUV:
        case IMGFMT_YV12:
            return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_SWSCALE | VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}


static void uninit(void)
{
    if (!myximage)
        return;

    freeMyXImage();

#ifdef CONFIG_XF86VM
    vo_vm_close();
#endif

    zoomFlag = 0;
    vo_x11_uninit();

    sws_freeContext(swsContext);
}

static int preinit(const char *arg)
{
    if (arg)
    {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_x11: Unknown subdevice: %s\n", arg);
        return ENOSYS;
    }

    if (!vo_init())
        return -1;              // Can't open X11
    return 0;
}

static int control(uint32_t request, void *data, ...)
{
    switch (request)
    {
        case VOCTRL_PAUSE:
            return int_pause = 1;
        case VOCTRL_RESUME:
            return int_pause = 0;
        case VOCTRL_QUERY_FORMAT:
            return query_format(*((uint32_t *) data));
        case VOCTRL_GET_IMAGE:
            return get_image(data);
        case VOCTRL_GUISUPPORT:
            return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            vo_x11_fullscreen();
            vo_x11_clearwindow(mDisplay, vo_window);
            return VO_TRUE;
        case VOCTRL_SET_EQUALIZER:
            {
                va_list ap;
                int value;

                va_start(ap, data);
                value = va_arg(ap, int);

                va_end(ap);
                return vo_x11_set_equalizer(data, value);
            }
        case VOCTRL_GET_EQUALIZER:
            {
                va_list ap;
                int *value;

                va_start(ap, data);
                value = va_arg(ap, int *);

                va_end(ap);
                return vo_x11_get_equalizer(data, value);
            }
        case VOCTRL_ONTOP:
            vo_x11_ontop();
            return VO_TRUE;
        case VOCTRL_UPDATE_SCREENINFO:
            update_xinerama_info();
            return VO_TRUE;
    }
    return VO_NOTIMPL;
}
