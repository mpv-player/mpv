#include "vaapi.h"
#include "img_format.h"
#include "assert.h"
#include <libavutil/avutil.h>

#define VA_VERBOSE(...) mp_msg(MSGT_VO, MSGL_V, "[vaapi] "  __VA_ARGS__)
#define VA_ERROR(...) mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] "  __VA_ARGS__)

struct va_info {
    VADisplay dpy;
    Display *x11;

    VAImageFormat *image_formats;
    int num_image_formats;

    int ref;

    int *talloc_ctx;
};

static struct va_info *g_va = NULL;

VADisplay va_display_ref(Display *x11) {
    if (!g_va)
        g_va = talloc_zero(NULL, struct va_info);
    if (g_va->ref) {
        ++g_va->ref;
        return g_va->dpy;
    }
    if (!x11)
        return NULL;
    VADisplay dpy = vaGetDisplay(x11);
    if (!dpy)
        return NULL;

    int major_version, minor_version;
    VAStatus status = vaInitialize(dpy, &major_version, &minor_version);
    if (!check_va_status(status, "vaInitialize()"))
        return NULL;
    VA_VERBOSE("VA API version %d.%d\n", major_version, minor_version);

    g_va->dpy = dpy;
    ++g_va->ref;

    int max_image_formats = vaMaxNumImageFormats(dpy);
    g_va->image_formats = talloc_array(g_va, VAImageFormat, max_image_formats);
    status = vaQueryImageFormats(dpy, g_va->image_formats, &g_va->num_image_formats);
    if (!check_va_status(status, "vaQueryImageFormats()"))
        return dpy;
    VA_VERBOSE("%d image formats available:\n", g_va->num_image_formats);
    for (int i=0; i<g_va->num_image_formats; ++i)
        VA_VERBOSE("  %s\n", VA_STR_FOURCC(g_va->image_formats[i].fourcc));
    return dpy;
}

void va_display_unref() {
    if (g_va) {
        --g_va->ref;
        if (g_va->ref <= 0) {
            vaTerminate(g_va->dpy);
            g_va->dpy = NULL;
            talloc_free(g_va);
            g_va = NULL;
        }
    }
}

VAImageFormat *va_image_formats_available() {
    return g_va->image_formats;
}

int va_image_formats_available_num() {
    return g_va->num_image_formats;
}

struct fmtentry {
    uint32_t va;
    enum mp_imgfmt mp;
};

static const struct fmtentry va_to_imgfmt[] = {
    {VA_FOURCC_YV12, IMGFMT_420P},
    {VA_FOURCC_I420, IMGFMT_420P},
    {VA_FOURCC_IYUV, IMGFMT_420P},
    {VA_FOURCC_NV12, IMGFMT_NV12},
    // Note: not sure about endian issues (the mp formats are byte-addressed)
    {VA_FOURCC_RGBA, IMGFMT_RGBA},
    {VA_FOURCC_RGBX, IMGFMT_RGBA},
    {VA_FOURCC_BGRA, IMGFMT_BGRA},
    {VA_FOURCC_BGRX, IMGFMT_BGRA},
    // Untested.
    {VA_FOURCC_UYVY, IMGFMT_UYVY},
    {VA_FOURCC_YUY2, IMGFMT_YUYV},
    {0             , IMGFMT_NONE}
};

enum mp_imgfmt va_fourcc_to_imgfmt(uint32_t fourcc) {
    for (const struct fmtentry *entry = va_to_imgfmt; entry->va; ++entry) {
        if (entry->va == fourcc)
            return entry->mp;
    }
    return 0;
}

VAImageFormat *va_image_format_from_imgfmt(int imgfmt) {
    if (!g_va || !g_va->image_formats || !g_va->num_image_formats)
        return NULL;
    for (int i=0; i<g_va->num_image_formats; ++i) {
        if (va_fourcc_to_imgfmt(g_va->image_formats[i].fourcc) == imgfmt)
            return &g_va->image_formats[i];
    }
    return NULL;
}

// copied from mp_image_pool.c
#if HAVE_PTHREADS
#include <pthread.h>
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
#define pool_lock() pthread_mutex_lock(&pool_mutex)
#define pool_unlock() pthread_mutex_unlock(&pool_mutex)
#else
#define pool_lock() 0
#define pool_unlock() 0
#endif

static void va_surface_release(va_surface_t *surface);
static void va_surface_destroy(va_surface_t *surface);

struct va_surface_pool {
    VADisplay display;
    int rt_format;
    int num_surfaces, lru_counter;
    va_surface_t **surfaces;
    void *ctx;
    int refs;
};

typedef struct va_surface_priv {
    VADisplay display;
    VAImage image;       // used for sofwtare decoding case
    bool is_derived;     // is image derived by vaDeriveImage()?
    bool is_used;        // referenced
    bool is_dead;        // used, but deallocate VA objects as soon as possible
    int  order;          // for LRU allocation
} va_surface_priv_t;

static va_surface_pool_t **g_pools = NULL;
static int g_num_pools = 0;

static va_surface_pool_t *find_pool(int rt_format, void *ctx) {
    if (!g_pools)
        return NULL;
    for (int i=0; i<g_num_pools; ++i) {
        va_surface_pool_t *p = g_pools[i];
        if (p && p->ctx == ctx && p->rt_format == rt_format)
            return g_pools[i];
    }
    return NULL;
}

static int find_index(const va_surface_pool_t *pool) {
    if (!g_pools)
        return -1;
    for (int i=0; i<g_num_pools; ++i) {
        if (g_pools[i] == pool)
            return i;
    }
    return -1;
}

va_surface_pool_t *va_surface_pool_ref(VADisplay display, int rt_format, void *ctx) {
    va_surface_pool_t *pool = find_pool(rt_format, ctx);
    if (!pool) {
        // find not allocated index yet or append one
        int idx = find_index(NULL);
        if (idx < 0) {
            idx = g_num_pools;
            MP_TARRAY_APPEND(NULL, g_pools, g_num_pools, NULL);
        }
        pool = g_pools[idx] = talloc_ptrtype(NULL, pool);
        *pool = (va_surface_pool_t) {
            .display = display,
            .ctx = ctx,
            .rt_format = rt_format
        };
    }
    ++(pool->refs);
    return pool;
}


void va_surface_pool_unref(va_surface_pool_t **pool) {
    if (!pool || !*pool)
        return;
    va_surface_pool_t *p = *pool;
    *pool = NULL;
    if (--(p->refs) > 0)
        return;
    va_surface_pool_clear(p);
    const int idx = find_index(p);
    if (idx >= 0)
        g_pools[idx] = NULL;
    talloc_free(p);
}

void va_surface_pool_clear(va_surface_pool_t *pool) {
    for (int i=0; i<pool->num_surfaces; ++i) {
        va_surface_t *s = pool->surfaces[i];
        pool_lock();
        if (s->p->is_used)
            s->p->is_dead = true;
        else
            va_surface_destroy(s);
        pool_unlock();
    }
    talloc_free(pool->surfaces);
    pool->num_surfaces = 0;
}

va_surface_t *va_surface_in_mp_image(struct mp_image *mpi) {
    return mpi && IMGFMT_IS_VAAPI(mpi->imgfmt) ? (va_surface_t*)(uintptr_t)mpi->planes[0] : NULL;
//    if (!mpi || !IMGFMT_IS_VAAPI(mpi->imgfmt) || !g_pools)
//        return NULL;
//    // Note: we _could_ use planes[1] or planes[2] to store a vaapi_surface
//    //       pointer, but I just don't trust libavcodec enough.
//    return (void*)(uintptr_t)mpi->planes[0];
//    VASurfaceID id = (uintptr_t)(void*)mpi->planes[3];
//    for (int i=0; i<num_pools; ++i) {
//        struct va_surface_pool *pool = g_pools[i];
//        if (pool) {
//            for (int i=0; i<pool->num_surfaces; ++i) {
//                va_surface_t *surface = pool->surfaces[n];
//                if (surface->id == id)
//                    return surface;
//            }
//        }
//    }
//    return NULL;
}

void va_surface_destroy(va_surface_t *surface) {
    if (!surface)
        return;
    if (surface->id != VA_INVALID_ID) {
        va_surface_priv_t *p = surface->p;
        assert(!p->is_used);
        if (p->image.image_id != VA_INVALID_ID)
            vaDestroyImage(p->display, p->image.image_id);
        vaDestroySurfaces(p->display, &surface->id, 1);
    }
    talloc_free(surface);
}

void va_surface_release(va_surface_t *surface) {
    if (!surface)
        return;
    pool_lock();
    surface->p->is_used = false;
    if (surface->p->is_dead)
        va_surface_destroy(surface);
    pool_unlock();
}

void va_surface_unref(va_surface_t **surface) {
    va_surface_release(*surface);
    *surface = NULL;
}

static va_surface_t *va_surface_alloc(va_surface_pool_t *pool, int w, int h) {
    VASurfaceID id = VA_INVALID_ID;
    VAStatus status = vaCreateSurfaces(pool->display, w, h, pool->rt_format, 1, &id);
    if (!check_va_status(status, "vaCreateSurfaces()"))
        return NULL;

    va_surface_t *surface = talloc_ptrtype(NULL, surface);
    if (!surface)
        return NULL;

    MP_TARRAY_APPEND(NULL, pool->surfaces, pool->num_surfaces, surface);
    surface->id = id;
    surface->w = w;
    surface->h = h;
    surface->rt_format = pool->rt_format;
    surface->p = talloc_zero(surface, va_surface_priv_t);
    surface->p->display = pool->display;
    surface->p->image.image_id = surface->p->image.buf = VA_INVALID_ID;
    return surface;
}

mp_image_t *va_surface_pool_get_wrapped(va_surface_pool_t *pool, int imgfmt, int w, int h) {
    return va_surface_wrap(va_surface_pool_get_by_imgfmt(pool, imgfmt, w, h));
}

int va_surface_pool_rt_format(const va_surface_pool_t *pool) {
    return pool->rt_format;
}

bool va_surface_pool_reserve(va_surface_pool_t *pool, int count, int w, int h) {
    for (int i=0; i<pool->num_surfaces && count > 0; ++i) {
        const va_surface_t *s = pool->surfaces[i];
        if (s->w == w && s->h == h && !s->p->is_used)
            --count;
    }
    while (count > 0) {
        if (!va_surface_alloc(pool, w, h))
            break;
        --count;
    }
    return !count;
}

va_surface_t *va_surface_pool_get(va_surface_pool_t *pool, int w, int h) {
    va_surface_t *best = NULL;
    for (int i=0; i<pool->num_surfaces; ++i) {
        va_surface_t *s = pool->surfaces[i];
        if (!s->p->is_used && s->w == w && s->h == h) {
            if (!best || best->p->order > s->p->order)
                best = s;
        }
    }
    if (!best)
        best = va_surface_alloc(pool, w, h);
    if (best) {
        best->p->is_used = true;
        best->p->order = ++pool->lru_counter;
    }
    return best;
}

static void va_surface_image_destroy(va_surface_t *surface) {
    if (!surface || surface->p->image.image_id == VA_INVALID_ID)
        return;
    va_surface_priv_t *p = surface->p;
    vaDestroyImage(p->display, p->image.image_id);
    p->image.image_id = VA_INVALID_ID;
    p->is_derived = false;
}

static VAImage *va_surface_image_alloc(va_surface_t *surface, VAImageFormat *format) {
    if (!format || !surface)
        return NULL;
    va_surface_priv_t *p = surface->p;
    if (p->image.image_id != VA_INVALID_ID && p->image.format.fourcc == format->fourcc)
        return &p->image;
    va_surface_image_destroy(surface);

    VAStatus status = vaDeriveImage(p->display, surface->id, &p->image);
    if (check_va_status(status, "vaDeriveImage()")) {
        /* vaDeriveImage() is supported, check format */
        if (p->image.format.fourcc == format->fourcc &&
                p->image.width == surface->w && p->image.height == surface->h) {
            p->is_derived = true;
            VA_VERBOSE("Using vaDeriveImage()\n");
        } else {
            vaDestroyImage(p->display, p->image.image_id);
            p->image.image_id = VA_INVALID_ID;
            status = VA_STATUS_ERROR_OPERATION_FAILED;
        }
    }
    if (status != VA_STATUS_SUCCESS) {
        status = vaCreateImage(p->display, format, surface->w, surface->h, &p->image);
        if (!check_va_status(status, "vaCreateImage()")) {
            p->image.image_id = VA_INVALID_ID;
            return NULL;
        }
    }
    return &surface->p->image;
}



va_surface_t *va_surface_pool_get_by_imgfmt(va_surface_pool_t *pool, int imgfmt, int w, int h) {
    if (imgfmt == IMGFMT_VAAPI)
        return va_surface_pool_get(pool, w, h);
    VAImageFormat *format = va_image_format_from_imgfmt(imgfmt);
    if (!format)
        return NULL;
    // WTF: no mapping from VAImageFormat -> VA_RT_FORMAT_
    va_surface_t *surface = va_surface_pool_get(pool, w, h);
    if (!surface)
        return NULL;
    if (va_surface_image_alloc(surface, format))
        return surface;
    va_surface_release(surface);
    return NULL;
}

static void free_va_surface(void *arg) {
    va_surface_release((va_surface_t*)arg);
}

mp_image_t *va_surface_wrap(va_surface_t *surface) {
    if (!surface)
        return NULL;

    mp_image_t img = {0};
    mp_image_setfmt(&img, IMGFMT_VAAPI);
    mp_image_set_size(&img, surface->w, surface->h);
    img.planes[0] = (uint8_t*)(uintptr_t)surface;
    img.planes[3] = (uint8_t*)(uintptr_t)surface->id;
    return mp_image_new_custom_ref(&img, surface, free_va_surface);
}

VASurfaceID va_surface_id(const va_surface_t *surface) {
    return surface->id;
}

VASurfaceID va_surface_id_in_mp_image(const mp_image_t *mpi) {
    return (VASurfaceID)(uintptr_t)mpi->planes[3];
}

bool va_image_map(VADisplay display, VAImage *image, mp_image_t *mpi) {
    int imgfmt = va_fourcc_to_imgfmt(image->format.fourcc);
    if (imgfmt == IMGFMT_NONE)
        return false;
    void *data = NULL;
    const VAStatus status = vaMapBuffer(display, image->buf, &data);
    if (!check_va_status(status, "vaMapBuffer()"))
        return false;

    *mpi = (struct mp_image) {0};
    mp_image_setfmt(mpi, imgfmt);
    mp_image_set_size(mpi, image->width, image->height);

    for (int p = 0; p < image->num_planes; p++) {
        mpi->stride[p] = image->pitches[p];
        mpi->planes[p] = (uint8_t *)data + image->offsets[p];
    }

    if (image->format.fourcc == VA_FOURCC_YV12) {
        FFSWAP(unsigned int, mpi->stride[1], mpi->stride[2]);
        FFSWAP(uint8_t *, mpi->planes[1], mpi->planes[2]);
    }

    return true;
}

bool va_image_unmap(VADisplay display, VAImage *image) {
    const VAStatus status = vaUnmapBuffer(display, image->buf);
    return check_va_status(status, "vaUnmapBuffer()");
}

bool va_surface_upload(va_surface_t *surface, const mp_image_t *mpi) {
    va_surface_priv_t *p = surface->p;
    if (p->image.image_id == VA_INVALID_ID)
        return false;

    struct mp_image img;
    if (!va_image_map(p->display, &p->image, &img))
        return false;
    mp_image_copy(&img, (mp_image_t*)mpi);
    va_image_unmap(p->display, &p->image);

    if (!p->is_derived) {
        VAStatus status = vaPutImage2(p->display, surface->id,
                             p->image.image_id,
                             0, 0, mpi->w, mpi->h,
                             0, 0, mpi->w, mpi->h);
        if (!check_va_status(status, "vaPutImage()"))
            return false;
    }

    return true;
}

mp_image_t *va_surface_download(const va_surface_t *surface) {
    if (!g_va)
        return NULL;
    VAStatus status = vaSyncSurface(surface->p->display, surface->id);
    if (!check_va_status(status, "vaSyncSurface()"))
        return NULL;

    // We have no clue which format will work, so try them all.
    // This code is just for screenshots, so it's ok not to cache the right
    // format (to prevent unnecessary work), and we don't attempt to use
    // vaDeriveImage() for direct access either.
    for (int i=0; i<g_va->num_image_formats; ++i) {
        VAImageFormat *format = &g_va->image_formats[i];
        const enum mp_imgfmt imgfmt = va_fourcc_to_imgfmt(format->fourcc);
        if (imgfmt == IMGFMT_NONE)
            continue;
        VAImage image;
        status = vaCreateImage(surface->p->display, format, surface->w, surface->h, &image);
        if (!check_va_status(status, "vaCreateImage()"))
            continue;
        status = vaGetImage(surface->p->display, surface->id, 0, 0, surface->w, surface->h, image.image_id);
        if (status != VA_STATUS_SUCCESS) {
            vaDestroyImage(surface->p->display, image.image_id);
            continue;
        }
        struct mp_image *dst = NULL;
        struct mp_image tmp;
        if (va_image_map(surface->p->display, &image, &tmp)) {
            assert(tmp.imgfmt == imgfmt);
            dst = mp_image_alloc(imgfmt, tmp.w, tmp.h);
            mp_image_copy(dst, &tmp);
            va_image_unmap(surface->p->display, &image);
        }
        vaDestroyImage(surface->p->display, image.image_id);
        return dst;
    }
    VA_ERROR("failed to get surface data.\n");
    return NULL;
}

