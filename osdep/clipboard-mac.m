/*
 * Clipboard access for macOS
 *
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
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <libavutil/buffer.h>
#include <libswscale/swscale.h>
#include <stdint.h>

#include "clipboard.h"

#include "video/sws_utils.h"

static void release_ns_obj(void* ptr)
{
    [(id)ptr release];
}

static void free_ns_obj(void *opaque, uint8_t *data)
{
    [(id)opaque release];
}

static NSPasteboardItem *make_file_item(const char* path)
{
    NSString *str = [NSString stringWithUTF8String:path];
    NSURL *url = [NSURL fileURLWithPath:str];

    NSPasteboardItem *pbi = [[NSPasteboardItem alloc] init];
    [pbi setString:[url lastPathComponent] forType:@"public.utf8-plain-text"];
    [pbi setData:[[url absoluteString] dataUsingEncoding:NSUTF8StringEncoding] forType:NSPasteboardTypeFileURL];

    return pbi;
}

static NSArray *clipboard_item_to_ns(const struct m_clipboard_item *item)
{
    switch (item->type) {
    case CLIPBOARD_TEXT:
        return @[[NSString stringWithUTF8String:item->string]];
    case CLIPBOARD_URL: {
        NSString *str = [NSString stringWithUTF8String:item->string];
        NSURL *url = [NSURL URLWithString:str];
        return @[url, str];
    }
    case CLIPBOARD_PATH: {
        return @[make_file_item(item->string)];
    }
    case CLIPBOARD_PATHS: {
        NSMutableArray *ret = [NSMutableArray array];

        for (size_t i = 0; item->string_list[i]; i++) {
            [ret addObject:make_file_item(item->string_list[i])];
        }

        return ret;
    }
    case CLIPBOARD_IMAGE: {
        struct mp_image *image = item->image;

        enum mp_imgfmt imgfmt = image->imgfmt;

        bool compatible = false;
        switch (imgfmt) {
        case IMGFMT_YAP8:
        case IMGFMT_YAP16:
        case IMGFMT_Y8:
        case IMGFMT_Y16:
        case IMGFMT_ARGB:
        case IMGFMT_RGBA:
        case IMGFMT_RGB0:
        case IMGFMT_RGBA64:
            compatible = true;
            break;
        default:
            break;
        }

        if (image->params.repr.levels != PL_COLOR_LEVELS_FULL)
            compatible = false;

        static_assert(MP_MAX_PLANES <= 5, "Too many MP_MAX_PLANES");
        unsigned char *planes[5] = {NULL};
        NSInteger bps = image->fmt.comps[0].size;
        NSInteger spp = mp_imgfmt_desc_get_num_comps(&image->fmt);
        bool alpha = (image->fmt.flags & MP_IMGFLAG_ALPHA);
        bool gray = (image->fmt.flags & MP_IMGFLAG_GRAY);
        bool alphaFirst = alpha && (image->fmt.comps[3].plane == 0 &&
                                    image->fmt.comps[3].offset == 0);
        NSColorSpaceName csp = gray ? NSCalibratedWhiteColorSpace : NSCalibratedRGBColorSpace;
        NSBitmapFormat formatFlags = (alphaFirst ? NSBitmapFormatAlphaFirst : 0) |
                                     ((image->params.repr.alpha == PL_ALPHA_INDEPENDENT) ? NSBitmapFormatAlphaNonpremultiplied : 0);
        NSInteger bpp = image->fmt.bpp[0];
        NSInteger bytesPerRow = image->stride[0];
        bool planar = image->num_planes > 1;

        for (int i = 0; i < image->num_planes; i++) {
            if (image->stride[0] != bytesPerRow) {
                compatible = false;
                break;
            }
        }

        if (compatible) {
            for (int i = 0; i < image->num_planes; i++) {
                planes[i] = image->planes[i];
            }

            if (bpp == 24)
                bpp = 32;
        } else {
            bps = bps <= 8 ? 8 : 16;
            formatFlags &= ~NSBitmapFormatAlphaFirst;
            if (gray) {
                if (bps > 8) {
                    imgfmt = alpha ? IMGFMT_YAP16 : IMGFMT_Y16;
                    bpp = 16;
                } else {
                    imgfmt = alpha ? IMGFMT_YAP8 : IMGFMT_Y8;
                    bpp = 8;
                }
            } else {
                if (bps > 8) {
                    imgfmt = IMGFMT_RGBA64;
                    bpp = 64;
                } else {
                    imgfmt = alpha ? IMGFMT_RGBA : IMGFMT_RGB0;
                    bpp = 32;
                }
            }

            bytesPerRow = 0;
            planar = (gray && alpha);
            spp = (gray ? 1 : 3) + (alpha ? 1 : 0);
        }

        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:planes
            pixelsWide:image->w
            pixelsHigh:image->h
            bitsPerSample:bps
            samplesPerPixel:spp
            hasAlpha:alpha
            isPlanar:planar
            colorSpaceName:csp
            bitmapFormat:formatFlags
            bytesPerRow:bytesPerRow
            bitsPerPixel:bpp];

        if (!rep)
            return nil;

        CFStringRef cgSpaceName = NULL;
        struct pl_color_space plcsp = image->params.color;

        if (gray) {
            plcsp.primaries = PL_COLOR_PRIM_BT_709;
            if (image->params.color.transfer == PL_COLOR_TRC_LINEAR) {
                cgSpaceName = kCGColorSpaceLinearGray;
            } else if (image->params.color.transfer == PL_COLOR_TRC_GAMMA22) {
                cgSpaceName = kCGColorSpaceGenericGrayGamma2_2;
            } else {
                plcsp.transfer = PL_COLOR_TRC_GAMMA22;
                compatible = false;
            }
        } else {
            switch (image->params.color.primaries) {
            case PL_COLOR_PRIM_DISPLAY_P3:
                if (image->params.color.transfer == PL_COLOR_TRC_BT_1886) {
                    cgSpaceName = kCGColorSpaceDisplayP3;
                } else if (image->params.color.transfer == PL_COLOR_TRC_HLG) {
                    cgSpaceName = kCGColorSpaceDisplayP3_HLG;
                }
                break;
            case PL_COLOR_PRIM_BT_709:
                if (image->params.color.transfer == PL_COLOR_TRC_LINEAR) {
                    cgSpaceName = kCGColorSpaceLinearSRGB;
                } else if (image->params.color.transfer == PL_COLOR_TRC_BT_1886) {
                    cgSpaceName = kCGColorSpaceITUR_709;
                } else if (image->params.color.transfer == PL_COLOR_TRC_SRGB) {
                    cgSpaceName = kCGColorSpaceSRGB;
                }
                break;
            case PL_COLOR_PRIM_DCI_P3:
                if (image->params.color.transfer == PL_COLOR_TRC_BT_1886) {
                    cgSpaceName = kCGColorSpaceDCIP3;
                }
                break;
            case PL_COLOR_PRIM_BT_2020:
                if (image->params.color.transfer == PL_COLOR_TRC_BT_1886) {
                    cgSpaceName = kCGColorSpaceITUR_2020;
                }
                break;
            case PL_COLOR_PRIM_ADOBE:
                cgSpaceName = kCGColorSpaceAdobeRGB1998;
                break;
            case PL_COLOR_PRIM_APPLE:
                if (image->params.color.transfer == PL_COLOR_TRC_LINEAR) {
                    cgSpaceName = kCGColorSpaceGenericRGBLinear;
                }
                break;
            default:
                break;
            }

            if (!cgSpaceName) {
                compatible = false;
                cgSpaceName = kCGColorSpaceSRGB;
                plcsp.primaries = PL_COLOR_PRIM_BT_709;
                plcsp.transfer = PL_COLOR_TRC_SRGB;
            }
        }

        NSColorSpace *nscsp = nil;
        if (!gray && image->icc_profile) {
            nscsp = [[NSColorSpace alloc] initWithICCProfileData:[NSData dataWithBytes:image->icc_profile->data length:image->icc_profile->size]];
        } else if (cgSpaceName) {
            CGColorSpaceRef cgspace = CGColorSpaceCreateWithName(cgSpaceName);
            nscsp = [[NSColorSpace alloc] initWithCGColorSpace:cgspace];
            CFRelease(cgspace);
        }

        if (nscsp) {
            rep = [rep bitmapImageRepByRetaggingWithColorSpace:nscsp];
        }

        if (!compatible) {
            struct mp_image dest = {0};

            mp_image_setfmt(&dest, imgfmt);
            mp_image_set_size(&dest, image->w, image->h);

            [rep getBitmapDataPlanes:planes];
            for (int i = 0; i < MP_MAX_PLANES; i++) {
                dest.planes[i] = planes[i];
                dest.stride[i] = rep.bytesPerRow;
            }

            dest.params.repr = (struct pl_color_repr){
                .sys = PL_COLOR_SYSTEM_RGB,
                .levels = PL_COLOR_LEVELS_FULL,
                .alpha = rep.alpha ? ((rep.bitmapFormat & NSBitmapFormatAlphaNonpremultiplied) ? PL_ALPHA_INDEPENDENT : PL_ALPHA_PREMULTIPLIED) : PL_ALPHA_UNKNOWN,
                .bits = (struct pl_bit_encoding){
                    .sample_depth = bps,
                    .color_depth = bps,
                    .bit_shift = 0,
                },
            };

            dest.params.color = plcsp;

            if (mp_image_swscale(&dest, image, SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP | SWS_ACCURATE_RND) < 0)
                return nil;
        }

        return @[[[NSImage alloc] initWithCGImage:[rep CGImage] size:NSZeroSize]];
    }
    default:
        return nil;
    }
}

int m_clipboard_set(struct MPContext *ctx, const struct m_clipboard_item *item)
{
    @autoreleasepool {
        id arr = clipboard_item_to_ns(item);
        if (!arr)
            return CLIPBOARD_FAILED;

        [[NSPasteboard generalPasteboard] clearContents];
        bool success = [[NSPasteboard generalPasteboard] writeObjects:arr];

        return success ? CLIPBOARD_OK : CLIPBOARD_FAILED;
    }
}

static enum mp_imgfmt lookup_imgfmt(NSBitmapImageRep *rep)
{
    if (rep.samplesPerPixel > MP_NUM_COMPONENTS || rep.numberOfPlanes > MP_MAX_PLANES ||
        (rep.bitmapFormat & (NSBitmapFormatSixteenBitBigEndian | NSBitmapFormatThirtyTwoBitBigEndian)))
        return IMGFMT_NONE;

    NSInteger sample_bits = rep.bitsPerPixel / (rep.planar ? 1 : rep.samplesPerPixel);

    struct mp_regular_imgfmt reg = {
        .component_type = (rep.bitmapFormat & NSBitmapFormatFloatingPointSamples) ? MP_COMPONENT_TYPE_FLOAT : MP_COMPONENT_TYPE_UINT,
        .forced_csp = PL_COLOR_SYSTEM_UNKNOWN,
        .component_size = (sample_bits + 7) / 8,
        .component_pad = sample_bits % 8,
        .num_planes = rep.numberOfPlanes,
    };

    NSInteger alphaChannel = rep.alpha ? ((rep.bitmapFormat & NSBitmapFormatAlphaFirst) ? 0 : rep.samplesPerPixel - 1) : -1;
    int alphaShift = alphaChannel == 0;
    for (NSInteger i = 0; i < rep.samplesPerPixel; i++) {
        if (rep.planar) {
            reg.planes[i].num_components = 1;
        } else {
            reg.planes[0].num_components = rep.samplesPerPixel;
        }

        reg.planes[rep.planar ? 0 : i].components[rep.planar ? i : 0] = (i == alphaChannel) ? 0 : (i - alphaShift);
    }

    return mp_find_regular_imgfmt(&reg);
}

int m_clipboard_get(struct MPContext *ctx, struct m_clipboard_item *item)
{
    @autoreleasepool {
        switch (item->type) {
        case CLIPBOARD_IMAGE: {
            NSImage *image;
            @try {
                image = [[NSImage alloc] initWithPasteboard:[NSPasteboard generalPasteboard]];
            } @catch (id ex) {
            }

            if (!image)
                return CLIPBOARD_NONE;

            assert(!item->image);

            CGImageRef cgi = [image CGImageForProposedRect:nil context:nil hints:nil];
            NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cgi];

            enum mp_imgfmt fmt = lookup_imgfmt(rep);
            if (!fmt)
                return CLIPBOARD_NONE;

            item->image = mp_image_new_custom_ref(NULL, image, &release_ns_obj);

            mp_image_set_size(item->image, rep.pixelsWide, rep.pixelsHigh);
            mp_image_setfmt(item->image, fmt);

            item->image->num_planes = rep.numberOfPlanes;

            unsigned char* planes[5];
            [rep getBitmapDataPlanes:planes];
            static_assert(MP_MAX_PLANES <= 5, "Too many MP_MAX_PLANES");
            for (int i = 0; i < MP_MAX_PLANES; i++) {
                item->image->planes[i] = planes[i];
                item->image->stride[i] = rep.bytesPerRow;
            }

            item->image->params.repr = (struct pl_color_repr){
                .sys = PL_COLOR_SYSTEM_RGB,
                .levels = PL_COLOR_LEVELS_FULL,
                .alpha = rep.alpha ? ((rep.bitmapFormat & NSBitmapFormatAlphaNonpremultiplied) ? PL_ALPHA_INDEPENDENT : PL_ALPHA_PREMULTIPLIED) : PL_ALPHA_UNKNOWN,
                .bits = (struct pl_bit_encoding){
                    .sample_depth = rep.bitsPerPixel / (rep.planar ? 1 : rep.samplesPerPixel),
                    .color_depth = rep.bitsPerPixel / (rep.planar ? 1 : rep.samplesPerPixel),
                    .bit_shift = 0,
                },
            };

            // Default color to a reasonable guess
            item->image->params.color = (struct pl_color_space){
                .primaries = PL_COLOR_PRIM_BT_709,
                .transfer = PL_COLOR_TRC_SRGB,
            };

            CGColorSpaceRef cgspace = rep.colorSpace.CGColorSpace;
            if (cgspace) {
                CFStringRef name = CGColorSpaceCopyName(cgspace);
                if (CFEqual(name, kCGColorSpaceDisplayP3)) {
                    item->image->params.color.primaries = PL_COLOR_PRIM_DISPLAY_P3;
                    item->image->params.color.transfer = PL_COLOR_TRC_BT_1886;
                } else if (CFEqual(name, kCGColorSpaceDisplayP3_HLG)) {
                    item->image->params.color.primaries = PL_COLOR_PRIM_DISPLAY_P3;
                    item->image->params.color.transfer = PL_COLOR_TRC_HLG;
                } else if (CFEqual(name, kCGColorSpaceExtendedLinearDisplayP3)) {
                    item->image->params.color.primaries = PL_COLOR_PRIM_DISPLAY_P3;
                    item->image->params.color.transfer = PL_COLOR_TRC_LINEAR;
                } else if (CFEqual(name, kCGColorSpaceLinearSRGB) ||
                           CFEqual(name, kCGColorSpaceExtendedLinearSRGB)) {
                    item->image->params.color.transfer = PL_COLOR_TRC_LINEAR;
                } else if (CFEqual(name, kCGColorSpaceGenericGrayGamma2_2) ||
                           CFEqual(name, kCGColorSpaceExtendedGray)) {
                    item->image->params.color.transfer = PL_COLOR_TRC_GAMMA22;
                } else if (CFEqual(name, kCGColorSpaceLinearGray) ||
                           CFEqual(name, kCGColorSpaceExtendedLinearGray)) {
                    item->image->params.color.transfer = PL_COLOR_TRC_LINEAR;
                } else if (CFEqual(name, kCGColorSpaceGenericRGBLinear)) {
                    item->image->params.color.primaries = PL_COLOR_PRIM_APPLE;
                    item->image->params.color.transfer = PL_COLOR_TRC_LINEAR;
                } else if (CFEqual(name, kCGColorSpaceAdobeRGB1998)) {
                    item->image->params.color.primaries = PL_COLOR_PRIM_ADOBE;
                } else if (CFEqual(name, kCGColorSpaceDCIP3)) {
                    item->image->params.color.primaries = PL_COLOR_PRIM_DCI_P3;
                    item->image->params.color.transfer = PL_COLOR_TRC_BT_1886;
                } else if (CFEqual(name, kCGColorSpaceITUR_709)) {
                    item->image->params.color.transfer = PL_COLOR_TRC_BT_1886;
                } else if (CFEqual(name, kCGColorSpaceITUR_2020) ||
                           CFEqual(name, kCGColorSpaceExtendedLinearITUR_2020)) {
                    item->image->params.color.primaries = PL_COLOR_PRIM_BT_2020;
                    item->image->params.color.transfer = PL_COLOR_TRC_BT_1886;
                }
            }

            NSData *icc = rep.colorSpace.ICCProfileData;
            if (icc) {
                item->image->icc_profile = av_buffer_create((void*)icc.bytes, icc.length, free_ns_obj, [icc retain], AV_BUFFER_FLAG_READONLY);
            }

            return CLIPBOARD_OK;
        }
        case CLIPBOARD_TEXT: {
            NSString* contents = [[NSPasteboard generalPasteboard] stringForType:@"public.url-name"];
            if (!contents)
                contents = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];

            if (!contents)
                return CLIPBOARD_NONE;

            item->string = ta_strdup(NULL, [contents UTF8String]);
            return CLIPBOARD_OK;
        }
        case CLIPBOARD_PATH:
        case CLIPBOARD_URL: {
            bool path = item->type == CLIPBOARD_PATH;
            NSURL *url = [NSURL URLFromPasteboard:[NSPasteboard generalPasteboard]];

            if (!url)
                return CLIPBOARD_NONE;

            if (path && !url.fileURL)
                return CLIPBOARD_NONE;

            NSString *str = path ? url.path : url.absoluteString;

            item->string = ta_strdup(NULL, [str UTF8String]);
            return CLIPBOARD_OK;
        }
        case CLIPBOARD_PATHS: {
            NSArray *urls = [[NSPasteboard generalPasteboard] readObjectsForClasses:@[[NSURL class]] options:@{NSPasteboardURLReadingFileURLsOnlyKey: @(YES)}];

            if (!urls || !urls.count)
                return CLIPBOARD_NONE;

            size_t count = urls.count;
            item->string_list = talloc_array(NULL, char*, count + 1);

            for (NSUInteger i = 0; i < count; i++) {
                item->string_list[i] = ta_strdup(item->string_list, [[[urls objectAtIndex:i] path] UTF8String]);
            }

            return CLIPBOARD_OK;
        }
        default:
            return CLIPBOARD_FAILED;
        }
    }
}

bool m_clipboard_poll(struct MPContext *ctx)
{
    long current = [NSPasteboard generalPasteboard].changeCount;
    bool ret = current != ctx->clipboard->changeCount;
    ctx->clipboard->changeCount = current;
    return ret;
}
