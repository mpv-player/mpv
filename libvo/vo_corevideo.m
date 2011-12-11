/*
 * CoreVideo video output driver
 * Copyright (c) 2005 Nicolas Plourde <nicolasplourde@gmail.com>
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

#import "vo_corevideo.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <unistd.h>
#include <CoreServices/CoreServices.h>

//MPLAYER
#include "config.h"
#include "fastmemcpy.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "mp_msg.h"
#include "m_option.h"
#include "mp_fifo.h"
#include "sub/sub.h"
#include "subopt-helper.h"

#include "input/input.h"
#include "input/keycodes.h"

#import "cocoa_common.h"

//Cocoa
NSDistantObject *mplayerosxProxy;
id <MPlayerOSXVOProto> mplayerosxProto;
NSAutoreleasePool *autoreleasepool;
OSType pixelFormat;

//shared memory
BOOL shared_buffer = false;
#define DEFAULT_BUFFER_NAME "mplayerosx"
static char *buffer_name;

int screen_id;

//CoreVideo
CVPixelBufferRef frameBuffers[2];
CVOpenGLTextureCacheRef textureCache;
CVOpenGLTextureRef texture;
NSRect textureFrame;

GLfloat lowerLeft[2];
GLfloat lowerRight[2];
GLfloat upperRight[2];
GLfloat upperLeft[2];

//image
unsigned char *image_data;
// For double buffering
static uint8_t image_page = 0;
static unsigned char *image_datas[2];

static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_depth;
static uint32_t image_bytes;
static uint32_t image_format;

static vo_info_t info =
{
    "Mac OS X Core Video",
    "corevideo",
    "Nicolas Plourde <nicolas.plourde@gmail.com> and others",
    ""
};

LIBVO_EXTERN(corevideo)

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
    switch (image_format)
    {
        case IMGFMT_RGB24:
            vo_draw_alpha_rgb24(w,h,src,srca,stride,image_data+3*(y0*image_width+x0),3*image_width);
            break;
        case IMGFMT_ARGB:
        case IMGFMT_BGRA:
            vo_draw_alpha_rgb32(w,h,src,srca,stride,image_data+4*(y0*image_width+x0),4*image_width);
            break;
        case IMGFMT_YUY2:
            vo_draw_alpha_yuy2(w,h,src,srca,stride,image_data + (x0 + y0 * image_width) * 2,image_width*2);
            break;
    }
}

static void free_file_specific(void)
{
    if(shared_buffer)
    {
        [mplayerosxProto stop];
        mplayerosxProto = nil;
        [mplayerosxProxy release];
        mplayerosxProxy = nil;

        if (munmap(image_data, image_width*image_height*image_bytes) == -1)
            mp_msg(MSGT_VO, MSGL_FATAL, "[vo_corevideo] uninit: munmap failed. Error: %s\n", strerror(errno));

        if (shm_unlink(buffer_name) == -1)
            mp_msg(MSGT_VO, MSGL_FATAL, "[vo_corevideo] uninit: shm_unlink failed. Error: %s\n", strerror(errno));
    } else {
        free(image_datas[0]);
        if (vo_doublebuffering)
            free(image_datas[1]);
        image_datas[0] = NULL;
        image_datas[1] = NULL;
        image_data = NULL;
    }
}

static void resize(int width, int height)
{
    int d_width, d_height;

    mp_msg(MSGT_VO, MSGL_V, "[vo_corevideo] New OpenGL Viewport (0, 0, %d, %d)\n", width, height);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    aspect(&d_width, &d_height, A_WINZOOM);
    textureFrame = NSMakeRect((vo_dwidth - d_width) / 2, (vo_dheight - d_height) / 2, d_width, d_height);
}

static void prepare_opengl(void)
{
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    resize(global_vo->dwidth, global_vo->dheight);
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    free_file_specific();

    //misc mplayer setup
    image_width = width;
    image_height = height;
    switch (image_format) {
        case IMGFMT_RGB24:
            image_depth = 24;
            break;
        case IMGFMT_ARGB:
        case IMGFMT_BGRA:
            image_depth = 32;
            break;
        case IMGFMT_YUY2:
            image_depth = 16;
            break;
    }

    image_bytes = (image_depth ? image_depth : 16 + 7) / 8;

    if (!shared_buffer) {
        CVReturn error;

        image_data = malloc(image_width*image_height*image_bytes);
        image_datas[0] = image_data;
        if (vo_doublebuffering)
            image_datas[1] = malloc(image_width*image_height*image_bytes);
        image_page = 0;

        CVPixelBufferRelease(frameBuffers[0]);
        frameBuffers[0] = NULL;
        CVPixelBufferRelease(frameBuffers[1]);
        frameBuffers[1] = NULL;
        CVOpenGLTextureRelease(texture);
        texture = NULL;

        vo_cocoa_create_window(global_vo, d_width, d_height, flags);
        prepare_opengl();
        vo_cocoa_swap_interval(1);

        error = CVOpenGLTextureCacheCreate(NULL, 0, vo_cocoa_cgl_context(), vo_cocoa_cgl_pixel_format(), 0, &textureCache);
        if(error != kCVReturnSuccess)
            mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL texture Cache(%d)\n", error);

        error = CVPixelBufferCreateWithBytes(NULL, image_width, image_height, pixelFormat,
                                             image_datas[0], image_width*image_bytes, NULL, NULL, NULL, &frameBuffers[0]);
        if(error != kCVReturnSuccess)
            mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create Pixel Buffer(%d)\n", error);

        if (vo_doublebuffering) {
            error = CVPixelBufferCreateWithBytes(NULL, image_width, image_height, pixelFormat,
                                                 image_datas[1], image_width*image_bytes, NULL, NULL, NULL, &frameBuffers[1]);
            if(error != kCVReturnSuccess)
                mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create Pixel Double Buffer(%d)\n", error);
        }

        error = CVOpenGLTextureCacheCreateTextureFromImage(NULL, textureCache, frameBuffers[image_page], 0, &texture);
        if(error != kCVReturnSuccess)
            mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL texture(%d)\n", error);
    } else {
        int shm_fd;
        mp_msg(MSGT_VO, MSGL_INFO, "[vo_corevideo] writing output to a shared buffer "
                "named \"%s\"\n",buffer_name);

        // create shared memory
        shm_fd = shm_open(buffer_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if (shm_fd == -1) {
            mp_msg(MSGT_VO, MSGL_FATAL,
                   "[vo_corevideo] failed to open shared memory. Error: %s\n", strerror(errno));
            return 1;
        }

        if (ftruncate(shm_fd, image_width*image_height*image_bytes) == -1) {
            mp_msg(MSGT_VO, MSGL_FATAL,
                   "[vo_corevideo] failed to size shared memory, possibly already in use. Error: %s\n", strerror(errno));
            close(shm_fd);
            shm_unlink(buffer_name);
            return 1;
        }

        image_data = mmap(NULL, image_width*image_height*image_bytes,
                    PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        close(shm_fd);

        if (image_data == MAP_FAILED) {
            mp_msg(MSGT_VO, MSGL_FATAL,
                   "[vo_corevideo] failed to map shared memory. Error: %s\n", strerror(errno));
            shm_unlink(buffer_name);
            return 1;
        }

        //connect to mplayerosx
        mplayerosxProxy=[NSConnection rootProxyForConnectionWithRegisteredName:[NSString stringWithUTF8String:buffer_name] host:nil];
        if ([mplayerosxProxy conformsToProtocol:@protocol(MPlayerOSXVOProto)]) {
            [mplayerosxProxy setProtocolForProxy:@protocol(MPlayerOSXVOProto)];
            mplayerosxProto = (id <MPlayerOSXVOProto>)mplayerosxProxy;
            [mplayerosxProto startWithWidth: image_width withHeight: image_height withBytes: image_bytes withAspect:d_width*100/d_height];
        } else {
            [mplayerosxProxy release];
            mplayerosxProxy = nil;
            mplayerosxProto = nil;
        }
    }
    return 0;
}

static void check_events(void)
{
    if (!shared_buffer) {
        int e = vo_cocoa_check_events(global_vo);
        if (e & VO_EVENT_RESIZE)
            resize(global_vo->dwidth, global_vo->dheight);
    }
}

static void draw_osd(void)
{
    vo_draw_text(image_width, image_height, draw_alpha);
}

static void prepare_texture(void)
{
    CVReturn error;
    CVOpenGLTextureRelease(texture);
    error = CVOpenGLTextureCacheCreateTextureFromImage(NULL, textureCache, frameBuffers[image_page], 0, &texture);
    if(error != kCVReturnSuccess)
        mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL texture(%d)\n", error);

    CVOpenGLTextureGetCleanTexCoords(texture, lowerLeft, lowerRight, upperRight, upperLeft);
}

static void do_render(void)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(CVOpenGLTextureGetTarget(texture));
    glBindTexture(CVOpenGLTextureGetTarget(texture), CVOpenGLTextureGetName(texture));

    glColor3f(1,1,1);
    glBegin(GL_QUADS);
    glTexCoord2f(upperLeft[0], upperLeft[1]); glVertex2i(textureFrame.origin.x-(vo_panscan_x >> 1), textureFrame.origin.y-(vo_panscan_y >> 1));
    glTexCoord2f(lowerLeft[0], lowerLeft[1]); glVertex2i(textureFrame.origin.x-(vo_panscan_x >> 1), NSMaxY(textureFrame)+(vo_panscan_y >> 1));
    glTexCoord2f(lowerRight[0], lowerRight[1]); glVertex2i(NSMaxX(textureFrame)+(vo_panscan_x >> 1), NSMaxY(textureFrame)+(vo_panscan_y >> 1));
    glTexCoord2f(upperRight[0], upperRight[1]); glVertex2i(NSMaxX(textureFrame)+(vo_panscan_x >> 1), textureFrame.origin.y-(vo_panscan_y >> 1));
    glEnd();
    glDisable(CVOpenGLTextureGetTarget(texture));
}

static void flip_page(void)
{
    if(shared_buffer) {
        NSAutoreleasePool *pool = [NSAutoreleasePool new];
        [mplayerosxProto render];
        [pool release];
    } else {
        prepare_texture();
        do_render();
        if (vo_doublebuffering) {
            image_page = 1 - image_page;
            image_data = image_datas[image_page];
        }
        vo_cocoa_swap_buffers();
    }
}

static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
    return 0;
}


static int draw_frame(uint8_t *src[])
{
    return 0;
}

static uint32_t draw_image(mp_image_t *mpi)
{
    memcpy_pic(image_data, mpi->planes[0], image_width*image_bytes, image_height, image_width*image_bytes, mpi->stride[0]);

    return 0;
}

static int query_format(uint32_t format)
{
    const int supportflags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
    image_format = format;

    switch(format)
    {
        case IMGFMT_YUY2:
            pixelFormat = kYUVSPixelFormat;
            return supportflags;

        case IMGFMT_RGB24:
            pixelFormat = k24RGBPixelFormat;
            return supportflags;

        case IMGFMT_ARGB:
            pixelFormat = k32ARGBPixelFormat;
            return supportflags;

        case IMGFMT_BGRA:
            pixelFormat = k32BGRAPixelFormat;
            return supportflags;
    }
    return 0;
}

static void uninit(void)
{
    SetSystemUIMode(kUIModeNormal, 0);
    CGDisplayShowCursor(kCGDirectMainDisplay);

    free_file_specific();

    if (!shared_buffer)
        vo_cocoa_uninit(global_vo);

    free(buffer_name);
    buffer_name = NULL;
}

static const opt_t subopts[] = {
{"device_id",     OPT_ARG_INT,  &screen_id,     NULL},
{"shared_buffer", OPT_ARG_BOOL, &shared_buffer, NULL},
{"buffer_name",   OPT_ARG_MSTRZ,&buffer_name,   NULL},
{NULL}
};

static int preinit(const char *arg)
{
    // set defaults
    screen_id = -1;
    shared_buffer = false;
    buffer_name = NULL;

    if (subopt_parse(arg, subopts) != 0) {
        mp_msg(MSGT_VO, MSGL_FATAL,
                "\n-vo corevideo command line help:\n"
                "Example: mplayer -vo corevideo:device_id=1:shared_buffer:buffer_name=mybuff\n"
                "\nOptions:\n"
                "  device_id=<0-...>\n"
                "    Set screen device ID for fullscreen.\n"
                "  shared_buffer\n"
                "    Write output to a shared memory buffer instead of displaying it.\n"
                "  buffer_name=<name>\n"
                "    Name of the shared buffer created with shm_open() as well as\n"
                "    the name of the NSConnection MPlayer will try to open.\n"
                "    Setting buffer_name implicitly enables shared_buffer.\n"
                "\n" );
        return -1;
    }

    autoreleasepool = [[NSAutoreleasePool alloc] init];

    if (!buffer_name)
        buffer_name = strdup(DEFAULT_BUFFER_NAME);
    else
        shared_buffer = true;

    if (!shared_buffer)
        vo_cocoa_init(global_vo);

    return 0;
}

static int control(uint32_t request, void *data)
{
    switch (request) {
        case VOCTRL_DRAW_IMAGE: return draw_image(data);
        case VOCTRL_QUERY_FORMAT: return query_format(*(uint32_t*)data);
        case VOCTRL_ONTOP:
            if (!shared_buffer) {
                vo_cocoa_ontop(global_vo);
            } else {
                vo_ontop = !vo_ontop;
                [mplayerosxProto ontop];
            }
            return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            if (!shared_buffer) {
                vo_cocoa_fullscreen(global_vo);
                resize(global_vo->dwidth, global_vo->dheight);
            } else {
                [mplayerosxProto toggleFullscreen];
            }
            return VO_TRUE;
        case VOCTRL_GET_PANSCAN:
            return VO_TRUE;
        case VOCTRL_SET_PANSCAN:
            panscan_calc_windowed();
            return VO_TRUE;
        case VOCTRL_UPDATE_SCREENINFO:
            if (!shared_buffer) {
                vo_cocoa_update_xinerama_info(global_vo);
                return VO_TRUE;
            } else {
                return VO_NOTIMPL;
            }
    }
    return VO_NOTIMPL;
}
