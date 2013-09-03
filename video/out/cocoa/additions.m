/*
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

#import "additions.h"

@implementation NSScreen (mpvadditions)
- (BOOL)hasDock
{
    NSRect vF = [self visibleFrame];
    NSRect f  = [self frame];
    return
        // The visible frame's width is smaller: dock is on left or right end
        // of this method's receiver.
        vF.size.width < f.size.width ||
        // The visible frame's veritical origin is bigger: dock is
        // on the bottom of this method's receiver.
        vF.origin.y > f.origin.y;

}

- (BOOL)hasMenubar
{
    return [self isEqual: [NSScreen screens][0]];
}
@end

@implementation NSEvent (mpvadditions)
- (int)mpvButtonNumber
{
    int buttonNumber = [self buttonNumber];
    switch (buttonNumber) {
        case 1:  return 2;
        case 2:  return 1;
        default: return buttonNumber;
    }
}
@end
