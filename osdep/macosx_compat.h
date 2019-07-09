/*
 * Application Event Handling
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


#ifndef MPV_MACOSX_COMPAT
#define MPV_MACOSX_COMPAT

#import <Cocoa/Cocoa.h>
#include "osdep/macosx_versions.h"

#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12)

@interface NSWindow (macOS10_12_SDK)
+ (void)setAllowsAutomaticWindowTabbing:(BOOL)allow;
@end

typedef NSUInteger NSWindowStyleMask;
static const NSWindowStyleMask NSWindowStyleMaskClosable = NSClosableWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskTitled = NSTitledWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskMiniaturizable = NSMiniaturizableWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskResizable = NSResizableWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskBorderless = NSBorderlessWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskFullScreen = NSFullScreenWindowMask;

static const NSEventType NSEventTypeSystemDefined = NSSystemDefined;
static const NSEventType NSEventTypeKeyDown = NSKeyDown;
static const NSEventType NSEventTypeKeyUp = NSKeyUp;

static const NSEventMask NSEventMaskLeftMouseUp = NSLeftMouseUpMask;

#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_10)
typedef NSUInteger NSEventModifierFlags;
#endif

static const NSEventModifierFlags NSEventModifierFlagShift = NSShiftKeyMask;
static const NSEventModifierFlags NSEventModifierFlagControl = NSControlKeyMask;
static const NSEventModifierFlags NSEventModifierFlagCommand = NSCommandKeyMask;
static const NSEventModifierFlags NSEventModifierFlagOption = NSAlternateKeyMask;

#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9)
typedef NSUInteger NSModalResponse;
static const NSModalResponse NSModalResponseOK = NSFileHandlingPanelOKButton;
#endif

#endif

#endif /* MPV_MACOSX_COMPAT */
