/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <https://www.gnu.org/licenses/>.
 */

#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>

#include <mpv/render_gl.h>

#include "libmpv_common.h"

static CGLContextObj gl_context;
static mpv_render_context *render_context;
static GLuint framebuffer, texture;
static char image_path[] = "/tmp/mpv-render-gl-XXXXXX";

static void cleanup_player(void)
{
    mpv_render_context_free(render_context);
    render_context = NULL;
    if (ctx)
        exit_cleanup();
}

static void cleanup(void)
{
    cleanup_player();
    if (gl_context) {
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &texture);
        CGLSetCurrentContext(NULL);
        CGLDestroyContext(gl_context);
    }
    unlink(image_path);
}

static void check_result(int result, const char *operation)
{
    if (result < 0)
        fail("%s failed: %s\n", operation, mpv_error_string(result));
}

static void check_invalid(int result, const char *operation)
{
    if (result != MPV_ERROR_INVALID_PARAMETER)
        fail("%s: expected invalid parameter, got %s\n",
             operation, mpv_error_string(result));
}

static void *get_proc_address(void *opaque, const char *name)
{
    return dlsym(RTLD_DEFAULT, name);
}

static void create_image(void)
{
    int fd = mkstemp(image_path);
    if (fd < 0)
        fail("Could not create image fixture.\n");
    FILE *file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        fail("Could not open image fixture.\n");
    }
    fputs("P6\n8 8\n255\n", file);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            unsigned char pixel[] = {y < 4 ? 255 : 0, 0, y < 4 ? 0 : 255};
            if (x < 2)
                memset(pixel, 128, sizeof(pixel));
            if (fwrite(pixel, 1, sizeof(pixel), file) != sizeof(pixel))
                fail("Could not write image fixture.\n");
        }
    }
    if (fclose(file))
        fail("Could not close image fixture.\n");
}

static void resize_target(int width, int height)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fail("Incomplete test framebuffer.\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void render(int width, int height, int flip, int depth,
                    unsigned char *pixels)
{
    mpv_opengl_fbo fbo = {framebuffer, width, height, GL_RGBA8};
    int block = 0;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_DEPTH, &depth},
        {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
        {0},
    };
    check_result(mpv_render_context_render(render_context, params), "render");
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (glGetError() != GL_NO_ERROR)
        fail("OpenGL error after rendering.\n");
    mpv_render_context_report_swap(render_context);
}

static bool has_image(const unsigned char *pixels, int width, int height)
{
    const unsigned char *upper = pixels + 4 * ((height / 4) * width + width / 2);
    const unsigned char *lower = pixels + 4 * ((3 * height / 4) * width + width / 2);
    return upper[0] > 160 && upper[2] < 80 &&
           lower[2] > 160 && lower[0] < 80;
}

static void test_depth(const char *api)
{
    set_property_string("dither", "ordered");
    set_property_string("dither-depth", "auto");
    set_property_string("temporal-dither", "no");
    unsigned char full[16 * 16 * 4], low[32 * 32 * 4], override[sizeof(full)];
    render(16, 16, 0, 8, full);
    // Resizing forces a fresh render even when a backend caches still frames.
    resize_target(32, 32);
    render(32, 32, 0, 2, low);
    set_property_string("dither-depth", "8");
    resize_target(16, 16);
    render(16, 16, 0, 2, override);
    for (int y = 2; y < 14; y++) {
        for (int x = 1; x < 3; x++) {
            for (int c = 0; c < 3; c++) {
                int offset = 4 * (y * 16 + x) + c;
                int low_offset = 4 * ((2 * y) * 32 + 2 * x) + c;
                int quantized = ((low[low_offset] + 42) / 85) * 85;
                if (abs(low[low_offset] - quantized) > 2 ||
                    abs(full[offset] - low[low_offset]) < 20)
                    fail("%s did not quantize to the requested surface depth "
                         "at %d,%d channel %d: full=%u low=%u override=%u.\n",
                         api, x, y, c, full[offset], low[low_offset], override[offset]);
                if (abs(full[offset] - override[offset]) > 2)
                    fail("%s did not honor the dither-depth option override "
                         "at %d,%d channel %d: full=%u low=%u override=%u.\n",
                         api, x, y, c, full[offset], low[low_offset], override[offset]);
            }
        }
    }
}

static void test_api(const char *api)
{
    printf("Testing %s\n", api);
    ctx = mpv_create();
    if (!ctx)
        fail("Could not create mpv handle.\n");
    const char *options[][2] = {
        {"config", "no"}, {"load-scripts", "no"},
        {"vo", "libmpv"}, {"ao", "null"}, {"hwdec", "no"},
        {"pause", "yes"}, {"keep-open", "yes"},
        {"image-display-duration", "inf"}, {"osd-level", "0"},
        {"scale", "nearest"}, {"dither", "no"},
    };
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++)
        check_result(mpv_set_option_string(ctx, options[i][0], options[i][1]),
                     options[i][0]);
    if (!strcmp(api, MPV_RENDER_API_TYPE_OPENGL_NEXT)) {
        check_result(mpv_set_option_string(ctx, "hdr-reference-white", "100"),
                     "reference white");
        check_result(mpv_set_option_string(ctx, "target-peak", "500"), "target peak");
    }
    check_result(mpv_initialize(ctx), "initialize");

    mpv_opengl_init_params gl_init = {get_proc_address, NULL};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void *)api},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
        {0},
    };
    mpv_render_param no_init[] = {{MPV_RENDER_PARAM_API_TYPE, (void *)api}, {0}};
    check_invalid(mpv_render_context_create(&render_context, ctx, no_init),
                  "missing initialization parameters");
    if (!strcmp(api, MPV_RENDER_API_TYPE_OPENGL_NEXT)) {
        gl_init.get_proc_address = NULL;
        check_invalid(mpv_render_context_create(&render_context, ctx, params),
                      "missing OpenGL callback");
        gl_init.get_proc_address = get_proc_address;
    }

    for (int i = 0; i < 3; i++) {
        check_result(mpv_render_context_create(&render_context, ctx, params),
                     "create render context");
        if (i < 2) {
            mpv_render_context_free(render_context);
            render_context = NULL;
        }
    }
    check_invalid(mpv_render_context_render(render_context,
                                            (mpv_render_param[]){{0}}),
                  "missing framebuffer");
    if (!strcmp(api, MPV_RENDER_API_TYPE_OPENGL_NEXT)) {
        const char malformed[] = "invalid ICC profile";
        mpv_byte_array blob = {(void *)malformed, sizeof(malformed)};
        mpv_render_param icc = {MPV_RENDER_PARAM_ICC_PROFILE, &blob};
        check_result(mpv_render_context_set_parameter(render_context, icc),
                     "ignore ICC profile when automatic profiles are disabled");
        set_property_string("icc-profile-auto", "yes");
        check_invalid(mpv_render_context_set_parameter(render_context, icc),
                      "malformed ICC profile");
        set_property_string("icc-profile-auto", "no");
        check_result(mpv_render_context_set_parameter(render_context, icc),
                     "ignore ICC profile after disabling automatic profiles");

        mpv_opengl_fbo invalid = {framebuffer, 0, 16, GL_RGBA8};
        mpv_render_param bad_fbo[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &invalid}, {0}};
        check_invalid(mpv_render_context_render(render_context, bad_fbo),
                      "zero framebuffer width");
        invalid.w = 16;
        invalid.h = -1;
        check_invalid(mpv_render_context_render(render_context, bad_fbo),
                      "negative framebuffer height");
        invalid.w = INT_MAX;
        invalid.h = 16;
        check_invalid(mpv_render_context_render(render_context, bad_fbo),
                      "framebuffer width beyond GPU limits");
    }

    resize_target(16, 16);
    const char *load[] = {"loadfile", image_path, NULL};
    check_result(mpv_command_async(ctx, 1, load), "load image");
    unsigned char normal[16 * 16 * 4], flipped[sizeof(normal)];
    bool loaded = false, rendered = false;
    for (int i = 0; i < 500 && !rendered; i++) {
        mpv_event *event = mpv_wait_event(ctx, 0.01);
        if (event->event_id == MPV_EVENT_FILE_LOADED)
            loaded = true;
        if (event->event_id == MPV_EVENT_END_FILE) {
            mpv_event_end_file *end = event->data;
            if (end->reason == MPV_END_FILE_REASON_ERROR)
                fail("Image loading failed: %s\n", mpv_error_string(end->error));
        }
        mpv_render_context_update(render_context);
        render(16, 16, 0, 0, normal);
        rendered = loaded && has_image(normal, 16, 16);
    }
    if (!rendered)
        fail("%s did not render the red and blue image.\n", api);

    render(16, 16, 1, 0, flipped);
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16 * 4; x++) {
            if (abs(normal[y * 16 * 4 + x] - flipped[(15 - y) * 16 * 4 + x]) > 2)
                fail("%s did not flip the rendered image.\n", api);
        }
    }

    test_depth(api);
    resize_target(32, 16);
    unsigned char resized[32 * 16 * 4];
    render(32, 16, 0, 0, resized);
    if (!has_image(resized, 32, 16))
        fail("%s did not render after resizing.\n", api);
    const unsigned char *border = resized + 4 * (8 * 32 + 2);
    if (border[0] > 2 || border[1] > 2 || border[2] > 2)
        fail("%s did not preserve the image aspect ratio.\n", api);
    cleanup_player();
}

int main(void)
{
    atexit(cleanup);
    CGLPixelFormatAttribute attributes[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
        kCGLPFAColorSize, 24, kCGLPFAAlphaSize, 8, 0,
    };
    CGLPixelFormatObj format = NULL;
    GLint count = 0;
    CGLError error = CGLChoosePixelFormat(attributes, &format, &count);
    if (error == kCGLNoError && count)
        error = CGLCreateContext(format, NULL, &gl_context);
    if (format)
        CGLDestroyPixelFormat(format);
    if (error != kCGLNoError || !gl_context) {
        printf("Skipping: no CGL OpenGL 3.2 context is available.\n");
        return 77;
    }
    if (CGLSetCurrentContext(gl_context) != kCGLNoError)
        fail("Could not make CGL context current.\n");
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture);
    create_image();
    test_api(MPV_RENDER_API_TYPE_OPENGL);
    test_api(MPV_RENDER_API_TYPE_OPENGL_NEXT);
    return 0;
}
