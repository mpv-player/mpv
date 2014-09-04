/*
 * Cocoa OpenGL Adapter
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_COCOA_ADAPTER_H
#define MPLAYER_COCOA_ADAPTER_H

#include "video/out/vo.h"

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
@interface MpvCocoaAdapter : NSObject
- (void)setNeedsResize;
- (void)signalMouseMovement:(NSPoint)point;
- (void)putKey:(int)mpkey withModifiers:(int)modifiers;
- (void)putAxis:(int)mpkey delta:(float)delta;
- (void)putCommand:(char*)cmd;
- (void)performAsyncResize:(NSSize)size;
- (void)handleFilesArray:(NSArray *)files;
- (void)didChangeWindowedScreenProfile:(NSScreen *)screen;

- (BOOL)isInFullScreenMode;
- (NSScreen *)fsScreen;
@property(nonatomic, assign) struct vo *vout;

@end
#endif /* __OBJC__ */

struct vo_cocoa_state;

int vo_cocoa_init(struct vo *vo);
void vo_cocoa_uninit(struct vo *vo);

int vo_cocoa_config_window(struct vo *vo, uint32_t flags, void *gl_ctx);

void vo_cocoa_set_current_context(struct vo *vo, bool current);
void vo_cocoa_swap_buffers(struct vo *vo);
int vo_cocoa_check_events(struct vo *vo);
int vo_cocoa_control(struct vo *vo, int *events, int request, void *arg);

void vo_cocoa_register_resize_callback(struct vo *vo,
                                       void (*cb)(struct vo *vo, int w, int h));

void vo_cocoa_register_gl_clear_callback(struct vo *vo, void *ctx,
                                         void (*cb)(void *ctx));

void vo_cocoa_create_nsgl_ctx(struct vo *vo, void *ctx);
void vo_cocoa_release_nsgl_ctx(struct vo *vo);
void *vo_cocoa_get_nsgl_ctx(struct vo *vo);

#endif /* MPLAYER_COCOA_ADAPTER_H */
