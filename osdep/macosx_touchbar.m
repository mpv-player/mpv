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

#include "player/client.h"
#import "macosx_touchbar.h"

@implementation TouchBar

@synthesize app = _app;
@synthesize touchbarItems = _touchbar_items;
@synthesize duration = _duration;
@synthesize position = _position;

- (id)init
{
    if (self = [super init]) {
        self.touchbarItems = @{
            seekBar: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type": @"slider",
                @"name": @"Seek Bar",
                @"cmd":  @"seek %f absolute-percent"
            }],
            play: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type":     @"button",
                @"name":     @"Play Button",
                @"cmd":      @"cycle pause",
                @"image":    [NSImage imageNamed:NSImageNameTouchBarPauseTemplate],
                @"imageAlt": [NSImage imageNamed:NSImageNameTouchBarPlayTemplate]
            }],
            previousItem: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type":  @"button",
                @"name":  @"Previous Playlist Item",
                @"cmd":   @"playlist-prev",
                @"image": [NSImage imageNamed:NSImageNameTouchBarGoBackTemplate]
            }],
            nextItem: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type":  @"button",
                @"name":  @"Next Playlist Item",
                @"cmd":   @"playlist-next",
                @"image": [NSImage imageNamed:NSImageNameTouchBarGoForwardTemplate]
            }],
            previousChapter: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type":  @"button",
                @"name":  @"Previous Chapter",
                @"cmd":   @"add chapter -1",
                @"image": [NSImage imageNamed:NSImageNameTouchBarSkipBackTemplate]
            }],
            nextChapter: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type":  @"button",
                @"name":  @"Next Chapter",
                @"cmd":   @"add chapter 1",
                @"image": [NSImage imageNamed:NSImageNameTouchBarSkipAheadTemplate]
            }],
            cycleAudio: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type":  @"button",
                @"name":  @"Cycle Audio",
                @"cmd":   @"cycle audio",
                @"image": [NSImage imageNamed:NSImageNameTouchBarAudioInputTemplate]
            }],
            cycleSubtitle: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type":  @"button",
                @"name":  @"Cycle Subtitle",
                @"cmd":   @"cycle sub",
                @"image": [NSImage imageNamed:NSImageNameTouchBarComposeTemplate]
            }],
            currentPosition: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type": @"text",
                @"name": @"Current Position"
            }],
            timeLeft: [NSMutableDictionary dictionaryWithDictionary:@{
                @"type": @"text",
                @"name": @"Time Left"
            }]
        };
    }
    return self;
}

-(void)processEvent:(struct mpv_event *)event
{
    switch (event->event_id) {
    case MPV_EVENT_END_FILE: {
        self.position = 0;
        self.duration = 0;
        break;
    }
    case MPV_EVENT_PROPERTY_CHANGE: {
        [self handlePropertyChange:(mpv_event_property *)event->data];
        break;
    }
    }
}

-(void)handlePropertyChange:(struct mpv_event_property *)property
{
    NSString *name = [NSString stringWithUTF8String:property->name];
    mpv_format format = property->format;

    if ([name isEqualToString:@"time-pos"] && format == MPV_FORMAT_DOUBLE) {
        self.position = *(double *)property->data;
        self.position = self.position < 0 ? 0 : self.position;
        [self updateTouchBarTimeItems];
    } else if ([name isEqualToString:@"duration"] && format == MPV_FORMAT_DOUBLE) {
        self.duration = *(double *)property->data;
        [self updateTouchBarTimeItems];
    } else if ([name isEqualToString:@"pause"] && format == MPV_FORMAT_FLAG) {
        NSButton *playButton = self.touchbarItems[play][@"view"];
        if (*(int *)property->data) {
            playButton.image = self.touchbarItems[play][@"imageAlt"];
        } else {
            playButton.image = self.touchbarItems[play][@"image"];
        }
    }
}

- (nullable NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar
                makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
{
    if ([self.touchbarItems[identifier][@"type"] isEqualToString:@"slider"]) {
        NSSliderTouchBarItem *tbItem = [[NSSliderTouchBarItem alloc] initWithIdentifier:identifier];
        tbItem.slider.minValue = 0.0f;
        tbItem.slider.maxValue = 100.0f;
        tbItem.target = self;
        tbItem.action = @selector(seekbarChanged:);
        tbItem.customizationLabel = self.touchbarItems[identifier][@"name"];
        [self.touchbarItems[identifier] setObject:tbItem.slider forKey:@"view"];
        return tbItem;
    } else if ([self.touchbarItems[identifier][@"type"] isEqualToString:@"button"]) {
        NSCustomTouchBarItem *tbItem = [[NSCustomTouchBarItem alloc] initWithIdentifier:identifier];
        NSImage *tbImage = self.touchbarItems[identifier][@"image"];
        NSButton *tbButton = [NSButton buttonWithImage:tbImage target:self action:@selector(buttonAction:)];
        tbItem.view = tbButton;
        tbItem.customizationLabel = self.touchbarItems[identifier][@"name"];
        [self.touchbarItems[identifier] setObject:tbButton forKey:@"view"];
        return tbItem;
    } else if ([self.touchbarItems[identifier][@"type"] isEqualToString:@"text"]) {
        NSCustomTouchBarItem *tbItem = [[NSCustomTouchBarItem alloc] initWithIdentifier:identifier];
        NSTextField *tbText = [NSTextField labelWithString:@"0:00"];
        tbText.alignment = NSTextAlignmentCenter;
        tbItem.view = tbText;
        tbItem.customizationLabel = self.touchbarItems[identifier][@"name"];
        [self.touchbarItems[identifier] setObject:tbText forKey:@"view"];
        return tbItem;
    }

    return nil;
}

- (NSString *)formatTime:(int)time
{
    int seconds = time % 60;
    int minutes = (time / 60) % 60;
    int hours = time / (60 * 60);

    NSString *stime = hours > 0 ? [NSString stringWithFormat:@"%d:", hours] : @"";
    stime = (stime.length > 0 || minutes > 9) ?
        [NSString stringWithFormat:@"%@%02d:", stime, minutes] :
        [NSString stringWithFormat:@"%d:", minutes];
    stime = [NSString stringWithFormat:@"%@%02d", stime, seconds];

    return stime;
}

- (void)removeConstraintForIdentifier:(NSTouchBarItemIdentifier)identifier
{
    NSTextField *field = self.touchbarItems[identifier][@"view"];
    [field removeConstraint:self.touchbarItems[identifier][@"constrain"]];
}

- (void)applyConstraintFromString:(NSString *)string
                    forIdentifier:(NSTouchBarItemIdentifier)identifier
{
    NSTextField *field = self.touchbarItems[identifier][@"view"];
    if (field) {
        NSString *fString = [[string componentsSeparatedByCharactersInSet:
            [NSCharacterSet decimalDigitCharacterSet]] componentsJoinedByString:@"0"];
        NSTextField *textField = [NSTextField labelWithString:fString];
        NSSize size = [textField frame].size;

        NSLayoutConstraint *con =
            [NSLayoutConstraint constraintWithItem:field
                                         attribute:NSLayoutAttributeWidth
                                         relatedBy:NSLayoutRelationEqual
                                            toItem:nil
                                         attribute:NSLayoutAttributeNotAnAttribute
                                        multiplier:1.0
                                          constant:(int)ceil(size.width*1.1)];
        [field addConstraint:con];
        [self.touchbarItems[identifier] setObject:con forKey:@"constrain"];
    }
}

- (void)updateTouchBarTimeItemConstrains
{
    [self removeConstraintForIdentifier:currentPosition];
    [self removeConstraintForIdentifier:timeLeft];

    if (self.duration <= 0) {
        [self applyConstraintFromString:[self formatTime:self.position]
                          forIdentifier:currentPosition];
    } else {
        NSString *durFormat = [self formatTime:self.duration];

        [self applyConstraintFromString:durFormat forIdentifier:currentPosition];
        [self applyConstraintFromString:[NSString stringWithFormat:@"-%@", durFormat]
                          forIdentifier:timeLeft];
    }
}

- (void)updateTouchBarTimeItems
{
    NSSlider *seekSlider = self.touchbarItems[seekBar][@"view"];
    NSTextField *curPosItem = self.touchbarItems[currentPosition][@"view"];
    NSTextField *timeLeftItem = self.touchbarItems[timeLeft][@"view"];

    if (self.duration <= 0) {
        seekSlider.enabled = NO;
        seekSlider.doubleValue = 0;
        timeLeftItem.stringValue = @"";
    }
    else {
        seekSlider.enabled = YES;
        if (!seekSlider.highlighted)
            seekSlider.doubleValue = (self.position/self.duration)*100;
        int left = (int)(floor(self.duration)-floor(self.position));
        NSString *leftFormat = [self formatTime:left];
        timeLeftItem.stringValue = [NSString stringWithFormat:@"-%@", leftFormat];
    }
    NSString *posFormat = [self formatTime:(int)floor(self.position)];
    curPosItem.stringValue = posFormat;

    [self updateTouchBarTimeItemConstrains];
}

- (NSString *)getIdentifierFromView:(id)view
{
    NSString *identifier;
    for (identifier in self.touchbarItems)
        if([self.touchbarItems[identifier][@"view"] isEqual:view])
            break;
    return identifier;
}

- (void)buttonAction:(NSButton *)sender
{
    NSString *identifier = [self getIdentifierFromView:sender];
    [self.app queueCommand:(char *)[self.touchbarItems[identifier][@"cmd"] UTF8String]];
}

- (void)seekbarChanged:(NSSliderTouchBarItem *)sender
{
    NSString *identifier = [self getIdentifierFromView:sender.slider];
    NSString *seek = [NSString stringWithFormat:
        self.touchbarItems[identifier][@"cmd"], sender.slider.doubleValue];
    [self.app queueCommand:(char *)[seek UTF8String]];
}

@end
