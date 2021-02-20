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
#include "common/common.h"

#import "macosx_menubar_objc.h"
#import "osdep/macosx_application_objc.h"
#include "osdep/macosx_compat.h"

@implementation MenuBar
{
    NSArray *menuTree;
}

- (id)init
{
    if (self = [super init]) {
        NSUserDefaults *userDefaults =[NSUserDefaults standardUserDefaults];
        [userDefaults setBool:NO forKey:@"NSFullScreenMenuItemEverywhere"];
        [userDefaults setBool:YES forKey:@"NSDisabledDictationMenuItem"];
        [userDefaults setBool:YES forKey:@"NSDisabledCharacterPaletteMenuItem"];

        if ([NSWindow respondsToSelector:@selector(setAllowsAutomaticWindowTabbing:)])
            [NSWindow setAllowsAutomaticWindowTabbing: NO];

        menuTree = @[
            @{
                @"name": @"Apple",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"About mpv",
                        @"action"     : @"about",
                        @"key"        : @"",
                        @"target"     : self
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Preferences…",
                        @"action"     : @"preferences:",
                        @"key"        : @",",
                        @"target"     : self,
                        @"file"       : @"mpv.conf",
                        @"alertTitle1": @"No Application found to open your config file.",
                        @"alertText1" : @"Please open the mpv.conf file with "
                                        "your preferred text editor in the now "
                                        "open folder to edit your config.",
                        @"alertTitle2": @"No config file found.",
                        @"alertText2" : @"Please create a mpv.conf file with your "
                                        "preferred text editor in the now open folder.",
                        @"alertTitle3": @"No config path or file found.",
                        @"alertText3" : @"Please create the following path ~/.config/mpv/ "
                                        "and a mpv.conf file within with your preferred "
                                        "text editor."
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Keyboard Shortcuts Config…",
                        @"action"     : @"preferences:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"file"       : @"input.conf",
                        @"alertTitle1": @"No Application found to open your config file.",
                        @"alertText1" : @"Please open the input.conf file with "
                                        "your preferred text editor in the now "
                                        "open folder to edit your config.",
                        @"alertTitle2": @"No config file found.",
                        @"alertText2" : @"Please create a input.conf file with your "
                                        "preferred text editor in the now open folder.",
                        @"alertTitle3": @"No config path or file found.",
                        @"alertText3" : @"Please create the following path ~/.config/mpv/ "
                                        "and a input.conf file within with your preferred "
                                        "text editor."
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Services",
                        @"key"        : @"",
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Hide mpv",
                        @"action"     : @"hide:",
                        @"key"        : @"h",
                        @"target"     : NSApp
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Hide Others",
                        @"action"     : @"hideOtherApplications:",
                        @"key"        : @"h",
                        @"modifiers"  : [NSNumber numberWithUnsignedInteger:
                                            NSEventModifierFlagCommand |
                                            NSEventModifierFlagOption],
                        @"target"     : NSApp
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Show All",
                        @"action"     : @"unhideAllApplications:",
                        @"key"        : @"",
                        @"target"     : NSApp
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Quit and Remember Position",
                        @"action"     : @"quit:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"quit-watch-later"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Quit mpv",
                        @"action"     : @"quit:",
                        @"key"        : @"q",
                        @"target"     : self,
                        @"cmd"        : @"quit"
                    }]
                ]
            },
            @{
                @"name": @"File",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Open File…",
                        @"action"     : @"openFile",
                        @"key"        : @"o",
                        @"target"     : self
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Open URL…",
                        @"action"     : @"openURL",
                        @"key"        : @"O",
                        @"target"     : self
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Open Playlist…",
                        @"action"     : @"openPlaylist",
                        @"key"        : @"",
                        @"target"     : self
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Close",
                        @"action"     : @"performClose:",
                        @"key"        : @"w"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Save Screenshot",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"async screenshot"
                    }]
                ]
            },
            @{
                @"name": @"Edit",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Undo",
                        @"action"     : @"undo:",
                        @"key"        : @"z"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Redo",
                        @"action"     : @"redo:",
                        @"key"        : @"Z"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Cut",
                        @"action"     : @"cut:",
                        @"key"        : @"x"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Copy",
                        @"action"     : @"copy:",
                        @"key"        : @"c"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Paste",
                        @"action"     : @"paste:",
                        @"key"        : @"v"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Select All",
                        @"action"     : @"selectAll:",
                        @"key"        : @"a"
                    }]
                ]
            },
            @{
                @"name": @"View",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Fullscreen",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle fullscreen"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Float on Top",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle ontop"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Visibility on All Workspaces",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle on-all-workspaces"
                    }],
#if HAVE_MACOS_TOUCHBAR
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Customize Touch Bar…",
                        @"action"     : @"toggleTouchBarCustomizationPalette:",
                        @"key"        : @"",
                        @"target"     : NSApp
                    }]
#endif
                ]
            },
            @{
                @"name": @"Video",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Zoom Out",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add panscan -0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Zoom In",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add panscan 0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Reset Zoom",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set panscan 0"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Aspect Ratio 4:3",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set video-aspect \"4:3\""
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Aspect Ratio 16:9",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set video-aspect \"16:9\""
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Aspect Ratio 1.85:1",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set video-aspect \"1.85:1\""
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Aspect Ratio 2.35:1",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set video-aspect \"2.35:1\""
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Reset Aspect Ratio",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set video-aspect \"-1\""
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Half Size",
                        @"key"        : @"0",
                        @"cmdSpecial" : [NSNumber numberWithInt:MPM_H_SIZE]
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Normal Size",
                        @"key"        : @"1",
                        @"cmdSpecial" : [NSNumber numberWithInt:MPM_N_SIZE]
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Double Size",
                        @"key"        : @"2",
                        @"cmdSpecial" : [NSNumber numberWithInt:MPM_D_SIZE]
                    }]
                ]
            },
            @{
                @"name": @"Audio",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Next Audio Track",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle audio"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Previous Audio Track",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle audio down"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Mute",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle mute"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Play Audio Later",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add audio-delay 0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Play Audio Earlier",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add audio-delay -0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Reset Audio Delay",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set audio-delay 0.0 "
                    }]
                ]
            },
            @{
                @"name": @"Subtitle",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Next Subtitle Track",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle sub"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Previous Subtitle Track",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle sub down"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Force Style",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle-values sub-ass-override \"force\" \"no\""
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Display Subtitles Later",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add sub-delay 0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Display Subtitles Earlier",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add sub-delay -0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Reset Subtitle Delay",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set sub-delay 0.0"
                    }]
                ]
            },
            @{
                @"name": @"Playback",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Pause",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle pause"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Increase Speed",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add speed 0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Decrease Speed",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add speed -0.1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Reset Speed",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"set speed 1.0"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Show Playlist",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"script-message osc-playlist"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Show Chapters",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"script-message osc-chapterlist"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Show Tracks",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"script-message osc-tracklist"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Next File",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"playlist-next"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Previous File",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"playlist-prev"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Loop File",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle-values loop-file \"inf\" \"no\""
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Toggle Loop Playlist",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"cycle-values loop-playlist \"inf\" \"no\""
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Shuffle",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"playlist-shuffle"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Next Chapter",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add chapter 1"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Previous Chapter",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"add chapter -1"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Step Forward",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"frame-step"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Step Backward",
                        @"action"     : @"cmd:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"cmd"        : @"frame-back-step"
                    }]
                ]
            },
            @{
                @"name": @"Window",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Minimize",
                        @"key"        : @"m",
                        @"cmdSpecial" : [NSNumber numberWithInt:MPM_MINIMIZE]
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Zoom",
                        @"key"        : @"z",
                        @"cmdSpecial" : [NSNumber numberWithInt:MPM_ZOOM]
                    }]
                ]
            },
            @{
                @"name": @"Help",
                @"menu": @[
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"mpv Website…",
                        @"action"     : @"url:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"url"        : @"https://mpv.io"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"mpv on github…",
                        @"action"     : @"url:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"url"        : @"https://github.com/mpv-player/mpv"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Online Manual…",
                        @"action"     : @"url:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"url"        : @"https://mpv.io/manual/master/"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Online Wiki…",
                        @"action"     : @"url:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"url"        : @"https://github.com/mpv-player/mpv/wiki"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Release Notes…",
                        @"action"     : @"url:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"url"        : @"https://github.com/mpv-player/mpv/blob/master/RELEASE_NOTES"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Keyboard Shortcuts…",
                        @"action"     : @"url:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"url"        : @"https://github.com/mpv-player/mpv/blob/master/etc/input.conf"
                    }],
                    @{ @"name": @"separator" },
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Report Issue…",
                        @"action"     : @"url:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"url"        : @"https://github.com/mpv-player/mpv/issues/new/choose"
                    }],
                    [NSMutableDictionary dictionaryWithDictionary:@{
                        @"name"       : @"Show log File…",
                        @"action"     : @"showFile:",
                        @"key"        : @"",
                        @"target"     : self,
                        @"file"       : @"~/Library/Logs/mpv.log",
                        @"alertTitle" : @"No log File found.",
                        @"alertText"  : @"You deactivated logging for the Bundle."
                    }]
                ]
            }
        ];

        [NSApp setMainMenu:[self mainMenu]];
    }

    return self;
}

- (NSMenu *)mainMenu
{
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    [NSApp setServicesMenu:[[NSMenu alloc] init]];
    NSString* bundle = [[[NSProcessInfo processInfo] environment] objectForKey:@"MPVBUNDLE"];

    for(id mMenu in menuTree) {
        NSMenu *menu = [[NSMenu alloc] initWithTitle:mMenu[@"name"]];
        NSMenuItem *mItem = [mainMenu addItemWithTitle:mMenu[@"name"]
                                                action:nil
                                         keyEquivalent:@""];
        [mainMenu setSubmenu:menu forItem:mItem];

        for(id subMenu in mMenu[@"menu"]) {
            NSString *name = subMenu[@"name"];
            NSString *action = subMenu[@"action"];

#if HAVE_MACOS_TOUCHBAR
            if ([action isEqual:@"toggleTouchBarCustomizationPalette:"]) {
                if (![NSApp respondsToSelector:@selector(touchBar)])
                    continue;
            }
#endif

            if ([name isEqual:@"Show log File…"] && ![bundle isEqual:@"true"]) {
                continue;
            }

            if ([name isEqual:@"separator"]) {
                [menu addItem:[NSMenuItem separatorItem]];
            } else {
                NSMenuItem *iItem = [menu addItemWithTitle:name
                               action:NSSelectorFromString(action)
                        keyEquivalent:subMenu[@"key"]];
                [iItem setTarget:subMenu[@"target"]];
                [subMenu setObject:iItem forKey:@"menuItem"];

                NSNumber *m = subMenu[@"modifiers"];
                if (m) {
                    [iItem setKeyEquivalentModifierMask:m.unsignedIntegerValue];
                }

                if ([subMenu[@"name"] isEqual:@"Services"]) {
                    iItem.submenu = [NSApp servicesMenu];
                }
            }
        }
    }

    return mainMenu;
}

- (void)about
{
    NSDictionary *options = [NSDictionary dictionaryWithObjectsAndKeys:
        @"mpv", @"ApplicationName",
        [(Application *)NSApp getMPVIcon], @"ApplicationIcon",
        [NSString stringWithUTF8String:mpv_copyright], @"Copyright",
        [NSString stringWithUTF8String:mpv_version], @"ApplicationVersion",
        nil];
    [NSApp orderFrontStandardAboutPanelWithOptions:options];
}

- (void)preferences:(NSMenuItem *)menuItem
{
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSMutableDictionary *mItemDict = [self getDictFromMenuItem:menuItem];
    NSArray *configPaths  = @[
        [NSString stringWithFormat:@"%@/.mpv/", NSHomeDirectory()],
        [NSString stringWithFormat:@"%@/.config/mpv/", NSHomeDirectory()]];

    for (id path in configPaths) {
        NSString *fileP = [path stringByAppendingString:mItemDict[@"file"]];
        if ([fileManager fileExistsAtPath:fileP]){
            if ([workspace openFile:fileP])
                return;
            [workspace openFile:path];
            [self alertWithTitle:mItemDict[@"alertTitle1"]
                         andText:mItemDict[@"alertText1"]];
            return;
        }
        if ([workspace openFile:path]) {
            [self alertWithTitle:mItemDict[@"alertTitle2"]
                         andText:mItemDict[@"alertText2"]];
            return;
        }
    }

    [self alertWithTitle:mItemDict[@"alertTitle3"]
                 andText:mItemDict[@"alertText3"]];
}

- (void)quit:(NSMenuItem *)menuItem
{
    NSString *cmd = [self getDictFromMenuItem:menuItem][@"cmd"];
    [(Application *)NSApp stopMPV:(char *)[cmd UTF8String]];
}

- (void)openFile
{
    NSOpenPanel *panel = [[NSOpenPanel alloc] init];
    [panel setCanChooseDirectories:YES];
    [panel setAllowsMultipleSelection:YES];

    if ([panel runModal] == NSModalResponseOK){
        NSMutableArray *fileArray = [[NSMutableArray alloc] init];
        for (id url in [panel URLs])
            [fileArray addObject:[url path]];
        [(Application *)NSApp openFiles:fileArray];
    }
}

- (void)openPlaylist
{
    NSOpenPanel *panel = [[NSOpenPanel alloc] init];

    if ([panel runModal] == NSModalResponseOK){
        NSString *pl = [NSString stringWithFormat:@"loadlist \"%@\"",
                                                  [panel URLs][0].path];
        [(Application *)NSApp queueCommand:(char *)[pl UTF8String]];
    }
}

- (void)openURL
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Open URL"];
    [alert addButtonWithTitle:@"Ok"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setIcon:[(Application *)NSApp getMPVIcon]];

    NSTextField *input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)];
    [input setPlaceholderString:@"URL"];
    [alert setAccessoryView:input];

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        [input becomeFirstResponder];
    });

    if ([alert runModal] == NSAlertFirstButtonReturn && [input stringValue].length > 0) {
        NSArray *url = [NSArray arrayWithObjects:[input stringValue], nil];
        [(Application *)NSApp openFiles:url];
    }
}

- (void)cmd:(NSMenuItem *)menuItem
{
    NSString *cmd = [self getDictFromMenuItem:menuItem][@"cmd"];
    [(Application *)NSApp queueCommand:(char *)[cmd UTF8String]];
}

- (void)url:(NSMenuItem *)menuItem
{
    NSString *url = [self getDictFromMenuItem:menuItem][@"url"];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:url]];
}

- (void)showFile:(NSMenuItem *)menuItem
{
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSMutableDictionary *mItemDict = [self getDictFromMenuItem:menuItem];
    NSString *file = [mItemDict[@"file"] stringByExpandingTildeInPath];

    if ([fileManager fileExistsAtPath:file]){
        NSURL *url = [NSURL fileURLWithPath:file];
        NSArray *urlArray = [NSArray arrayWithObjects:url, nil];

        [workspace activateFileViewerSelectingURLs:urlArray];
        return;
    }

    [self alertWithTitle:mItemDict[@"alertTitle"]
                 andText:mItemDict[@"alertText"]];
}

- (void)alertWithTitle:(NSString *)title andText:(NSString *)text
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:title];
    [alert setInformativeText:text];
    [alert addButtonWithTitle:@"Ok"];
    [alert setIcon:[(Application *)NSApp getMPVIcon]];
    [alert runModal];
}

- (NSMutableDictionary *)getDictFromMenuItem:(NSMenuItem *)menuItem
{
    for(id mMenu in menuTree) {
        for(id subMenu in mMenu[@"menu"]) {
            if([subMenu[@"menuItem"] isEqual:menuItem])
                return subMenu;
        }
    }

    return nil;
}

- (void)registerSelector:(SEL)action forKey:(MPMenuKey)key
{
    for(id mMenu in menuTree) {
        for(id subMenu in mMenu[@"menu"]) {
            if([subMenu[@"cmdSpecial"] isEqual:[NSNumber numberWithInt:key]]) {
                [subMenu[@"menuItem"] setAction:action];
                return;
            }
        }
    }
}

@end
