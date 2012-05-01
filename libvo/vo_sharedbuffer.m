/*
 * OSX Shared Buffer Video Output (extracted from mplayer's corevideo)
 *
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This video output was extracted from mplayer's corevideo. Its purpose is
 * to copy mp_image data to a shared buffer using mmap and to do simple
 * coordination with the GUIs using Distributed Objects.
 */

#include <sys/mman.h>

#include "vo_sharedbuffer.h"
#include "video_out.h"
#include "m_option.h"
#include "talloc.h"

#include "libmpcodecs/vfcap.h"
#include "libmpcodecs/mp_image.h"
#include "fastmemcpy.h"

#include "sub/sub.h"
#include "osd.h"

// declarations
struct priv {
    char *buffer_name;
    unsigned char *image_data;
    unsigned int image_bytespp;
    unsigned int image_width;
    unsigned int image_height;

    void (*vo_draw_alpha_fnc)(int w, int h, unsigned char* src,
        unsigned char *srca, int srcstride, unsigned char* dstbase,
        int dststride);

    NSDistantObject *mposx_proxy;
    id <MPlayerOSXVOProto> mposx_proto;
};

// implementation
static void draw_alpha(void *ctx, int x0, int y0, int w, int h,
                            unsigned char *src, unsigned char *srca,
                            int stride)
{
    struct priv *p = ((struct vo *) ctx)->priv;
    p->vo_draw_alpha_fnc(w, h, src, srca, stride,
        p->image_data + (x0 + y0 * p->image_width) * p->image_bytespp,
        p->image_width * p->image_bytespp);
}

static unsigned int image_bytes(struct priv *p)
{
    return p->image_width * p->image_height * p->image_bytespp;
}

static int preinit(struct vo *vo, const char *arg)
{
    return 0;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    [p->mposx_proto render];
    [pool release];
}

static void check_events(struct vo *vo) { }

static uint32_t draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;
    memcpy_pic(p->image_data, mpi->planes[0],
               (p->image_width) * (p->image_bytespp), p->image_height,
               (p->image_width) * (p->image_bytespp), mpi->stride[0]);
    return 0;
}

static void draw_osd(struct vo *vo, struct osd_state *osd) {
    struct priv *p = vo->priv;
    osd_draw_text(osd, p->image_width, p->image_height, draw_alpha, vo);
}

static void free_buffers(struct priv *p)
{
    [p->mposx_proto stop];
    p->mposx_proto = nil;
    [p->mposx_proxy release];
    p->mposx_proxy = nil;

    if (p->image_data) {
        if (munmap(p->image_data, image_bytes(p)) == -1)
            mp_msg(MSGT_VO, MSGL_FATAL, "[vo_sharedbuffer] uninit: munmap "
                                        "failed. Error: %s\n", strerror(errno));

        if (shm_unlink(p->buffer_name) == -1)
            mp_msg(MSGT_VO, MSGL_FATAL, "[vo_sharedbuffer] uninit: shm_unlink "
                                        "failed. Error: %s\n", strerror(errno));
    }
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *p = vo->priv;
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    free_buffers(p);

    p->image_width = width;
    p->image_height = height;

    mp_msg(MSGT_VO, MSGL_INFO, "[vo_sharedbuffer] writing output to a shared "
        "buffer named \"%s\"\n", p->buffer_name);

    // create shared memory
    int shm_fd = shm_open(p->buffer_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        mp_msg(MSGT_VO, MSGL_FATAL,
            "[vo_sharedbuffer] failed to open shared memory. Error: %s\n",
            strerror(errno));
        goto err_out;
    }

    if (ftruncate(shm_fd, image_bytes(p)) == -1) {
        mp_msg(MSGT_VO, MSGL_FATAL,
            "[vo_sharedbuffer] failed to size shared memory, possibly "
            "already in use. Error: %s\n", strerror(errno));
        close(shm_fd);
        shm_unlink(p->buffer_name);
        goto err_out;
    }

    p->image_data = mmap(NULL, image_bytes(p),
        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (p->image_data == MAP_FAILED) {
        mp_msg(MSGT_VO, MSGL_FATAL,
            "[vo_sharedbuffer] failed to map shared memory. "
            "Error: %s\n", strerror(errno));
        shm_unlink(p->buffer_name);
        goto err_out;
    }

    //connect to mplayerosx
    p->mposx_proxy = [NSConnection
        rootProxyForConnectionWithRegisteredName:
                  [NSString stringWithUTF8String:p->buffer_name] host:nil];

    if ([p->mposx_proxy conformsToProtocol:@protocol(MPlayerOSXVOProto)]) {
        [p->mposx_proxy setProtocolForProxy:@protocol(MPlayerOSXVOProto)];
        p->mposx_proto = (id <MPlayerOSXVOProto>)p->mposx_proxy;
        [p->mposx_proto startWithWidth:p->image_width
                            withHeight:p->image_height
                             withBytes:p->image_bytespp
                            withAspect:d_width*100/d_height];
    } else {
        mp_msg(MSGT_VO, MSGL_ERR,
            "[vo_sharedbuffer] distributed object doesn't conform "
            "to the correct protocol.\n");
        [p->mposx_proxy release];
        p->mposx_proxy = nil;
        p->mposx_proto = nil;
    }

    [pool release];
    return 0;
err_out:
    [pool release];
    return 1;
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct priv *p = vo->priv;
    unsigned int image_depth = 0;
    switch (format) {
    case IMGFMT_YUY2:
        p->vo_draw_alpha_fnc = vo_draw_alpha_yuy2;
        image_depth = 16;
        goto supported;
    case IMGFMT_RGB24:
        p->vo_draw_alpha_fnc = vo_draw_alpha_rgb24;
        image_depth = 24;
        goto supported;
    case IMGFMT_ARGB:
        p->vo_draw_alpha_fnc = vo_draw_alpha_rgb32;
        image_depth = 32;
        goto supported;
    case IMGFMT_BGRA:
        p->vo_draw_alpha_fnc = vo_draw_alpha_rgb32;
        image_depth = 32;
        goto supported;
    }
    return 0;

supported:
    p->image_bytespp = (image_depth + 7) / 8;
    return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
        VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN |
        VOCAP_NOSLICES;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    free_buffers(p);
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
        case VOCTRL_DRAW_IMAGE:
            return draw_image(vo, data);
        case VOCTRL_FULLSCREEN:
            [p->mposx_proto toggleFullscreen];
            return VO_TRUE;
        case VOCTRL_QUERY_FORMAT:
            return query_format(vo, *(uint32_t*)data);
        case VOCTRL_ONTOP:
            [p->mposx_proto ontop];
            return VO_TRUE;
    }
    return VO_NOTIMPL;
}

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_sharedbuffer = {
    .is_new = true,
    .info = &(const vo_info_t) {
        "Mac OS X Shared Buffer (headless video output for GUIs)",
        "sharedbuffer",
        "Stefano Pigozzi <stefano.pigozzi@gmail.com> and others.",
        ""
    },
    .preinit = preinit,
    .config = config,
    .control = control,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
    .draw_osd = draw_osd,
    .privsize = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING("buffer_name", buffer_name, 0, OPTDEF_STR("mplayerosx")),
        {NULL},
    },
};
