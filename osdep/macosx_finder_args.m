/*
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
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include "macosx_finder_args.h"

static play_tree_t *files = NULL;

void macosx_wait_fileopen_events(void);
void macosx_redirect_output_to_logfile(const char *filename);
bool psn_matches_current_process(char *psn_arg_to_check);

@interface FileOpenDelegate : NSObject
- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames;
@end

@implementation FileOpenDelegate
- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    files = play_tree_new();
    play_tree_t *last_entry = nil;
    for (NSString *filename in filenames) {
        play_tree_t *entry = play_tree_new();
        play_tree_add_file(entry, [filename UTF8String]);

        if (last_entry)
          play_tree_append_entry(files, entry);
        else
          play_tree_set_child(files, entry);

        last_entry = entry;
    }
    [NSApp stop:nil]; // stop the runloop (give back control to mplayer2 code)
}
@end

void macosx_wait_fileopen_events()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApp = [NSApplication sharedApplication];
    [NSApp setDelegate: [[[FileOpenDelegate alloc] init] autorelease]];
    [NSApp run]; // block until we recive the fileopen events
    [pool release];
}

void macosx_redirect_output_to_logfile(const char *filename)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *log_path = [NSHomeDirectory() stringByAppendingPathComponent:
        [@"Library/Logs/" stringByAppendingFormat:@"%s.log", filename]];
    freopen([log_path fileSystemRepresentation], "a", stdout);
    freopen([log_path fileSystemRepresentation], "a", stderr);
    [pool release];
}

bool psn_matches_current_process(char *psn_arg_to_check)
{
    ProcessSerialNumber psn;
    char psn_arg[5+10+1+10+1];

    GetCurrentProcess(&psn);
    snprintf(psn_arg, 5+10+1+10+1, "-psn_%u_%u",
             psn.highLongOfPSN, psn.lowLongOfPSN);
    psn_arg[5+10+1+10]=0;

    return strcmp(psn_arg, psn_arg_to_check) == 0;
}

play_tree_t *macosx_finder_args(m_config_t *config, int argc, char **argv)
{
    if (argc==2 && psn_matches_current_process(argv[1])) {
        macosx_redirect_output_to_logfile("mplayer2");
        m_config_set_option0(config, "quiet", NULL, false);
        macosx_wait_fileopen_events();
    }

    return files;
}
