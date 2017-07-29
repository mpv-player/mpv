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
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#import "macosx_menubar_objc.h"
#import "osdep/macosx_application_objc.h"

@implementation MenuBar

@synthesize menuItems = _menu_items;

- (id)init
{
    if (self = [super init]) {
        self.menuItems = [[[NSMutableDictionary alloc] init] autorelease];

        NSMenu *main_menu = [[NSMenu new] autorelease];
        [NSApp setMainMenu:main_menu];
        [NSApp performSelector:@selector(setAppleMenu:)
                    withObject:[self appleMenuWithMainMenu:main_menu]];

        [self mainMenuItemWithParent:main_menu child:[self videoMenu]];
        [self mainMenuItemWithParent:main_menu child:[self windowMenu]];
    }

    return self;
}

- (NSMenu *)appleMenuWithMainMenu:(NSMenu *)mainMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Apple Menu"];
    [self mainMenuItemWithParent:mainMenu child:menu];
    [self menuItemWithParent:menu title:@"Hide mpv" action:@selector(hide:)
                          keyEquivalent: @"h" target:NSApp];
    [menu addItem:[NSMenuItem separatorItem]];
    [self menuItemWithParent:menu title:@"Quit mpv" action:@selector(quit)
                          keyEquivalent:@"q" target:self];
    return [menu autorelease];
}

#define _R(P, T, E, K) \
    { \
        NSMenuItem *tmp = [self menuItemWithParent:(P) title:(T) \
                                            action:nil keyEquivalent:(E) \
                                            target:nil]; \
        [self registerMenuItem:tmp forKey:(K)]; \
    }

- (NSMenu *)videoMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Video"];
    _R(menu, @"Half Size",   @"0", MPM_H_SIZE)
    _R(menu, @"Normal Size", @"1", MPM_N_SIZE)
    _R(menu, @"Double Size", @"2", MPM_D_SIZE)
    return [menu autorelease];
}

- (NSMenu *)windowMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Window"];
    _R(menu, @"Minimize", @"m", MPM_MINIMIZE)
    _R(menu, @"Zoom",     @"z", MPM_ZOOM)

#if HAVE_MACOS_TOUCHBAR
    if ([NSApp respondsToSelector:@selector(touchBar)]) {
        [menu addItem:[NSMenuItem separatorItem]];
        [self menuItemWithParent:menu title:@"Customize Touch Bar…"
                          action:@selector(toggleTouchBarMenu)
                   keyEquivalent:@"" target:self];
    }
#endif

    return [menu autorelease];
}

#undef _R

- (NSMenuItem *)mainMenuItemWithParent:(NSMenu *)parent
                                 child:(NSMenu *)child
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@""
                                                  action:nil
                                           keyEquivalent:@""];
    [item setSubmenu:child];
    [parent addItem:item];
    return [item autorelease];
}

- (NSMenuItem *)menuItemWithParent:(NSMenu *)parent
                             title:(NSString *)title
                            action:(SEL)action
                     keyEquivalent:(NSString*)key
                            target:(id)target
{

    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:action
                                           keyEquivalent:key];
    if (target)
        [item setTarget:target];
    [parent addItem:item];
    return [item autorelease];
}

- (void)registerMenuItem:(NSMenuItem*)menuItem forKey:(MPMenuKey)key
{
    [self.menuItems setObject:menuItem forKey:[NSNumber numberWithInt:key]];
}

- (void)registerSelector:(SEL)action forKey:(MPMenuKey)key
{
    NSNumber *boxedKey = [NSNumber numberWithInt:key];
    NSMenuItem *item   = [self.menuItems objectForKey:boxedKey];
    if (item) {
        [item setAction:action];
    }
}

#if HAVE_MACOS_TOUCHBAR
- (void)toggleTouchBarMenu
{
    [NSApp toggleTouchBarCustomizationPalette:self];
}
#endif

- (void)quit
{
    [(Application *)NSApp stopMPV:"quit"];
}

@end
