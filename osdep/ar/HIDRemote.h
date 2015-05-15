//
//  HIDRemote.h
//  HIDRemote V1.2
//
//  Created by Felix Schwarz on 06.04.07.
//  Copyright 2007-2011 IOSPIRIT GmbH. All rights reserved.
//
//  The latest version of this class is available at
//     http://www.iospirit.com/developers/hidremote/
//
//  ** LICENSE *************************************************************************
//
//  Copyright (c) 2007-2011 IOSPIRIT GmbH (http://www.iospirit.com/)
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list
//    of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice, this
//    list of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
//  * Neither the name of IOSPIRIT GmbH nor the names of its contributors may be used to
//    endorse or promote products derived from this software without specific prior
//    written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
//  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
//  SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
//  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//  DAMAGE.
//
//  ************************************************************************************


//  ************************************************************************************
//  ********************************** DOCUMENTATION ***********************************
//  ************************************************************************************
//
//  - a reference is available at http://www.iospirit.com/developers/hidremote/reference/
//  - for a guide, please see http://www.iospirit.com/developers/hidremote/guide/
//
//  ************************************************************************************


#import <Cocoa/Cocoa.h>

#include <Carbon/Carbon.h>

#include <unistd.h>
#include <mach/mach.h>
#include <sys/types.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDShared.h>

#pragma mark -- Enums / Codes  --

typedef enum
{
        kHIDRemoteModeNone = 0L,
        kHIDRemoteModeShared,           // Share the remote with others - let's you listen to the remote control events as long as noone has an exclusive lock on it
                                        // (RECOMMENDED ONLY FOR SPECIAL PURPOSES)

        kHIDRemoteModeExclusive,        // Try to acquire an exclusive lock on the remote (NOT RECOMMENDED)

        kHIDRemoteModeExclusiveAuto     // Try to acquire an exclusive lock on the remote whenever the application has focus. Temporarily release control over the
                                        // remote when another application has focus (RECOMMENDED)
} HIDRemoteMode;

typedef enum
{
        /* A code reserved for "no button" (needed for tracking) */
        kHIDRemoteButtonCodeNone        = 0L,

        /* Standard codes - available for white plastic and aluminum remote */
        kHIDRemoteButtonCodeUp,
        kHIDRemoteButtonCodeDown,
        kHIDRemoteButtonCodeLeft,
        kHIDRemoteButtonCodeRight,
        kHIDRemoteButtonCodeCenter,
        kHIDRemoteButtonCodeMenu,

        /* Extra codes - Only available for the new aluminum version of the remote */
        kHIDRemoteButtonCodePlay,

        /* Masks */
        kHIDRemoteButtonCodeCodeMask      = 0xFFL,
        kHIDRemoteButtonCodeHoldMask      = (1L << 16L),
        kHIDRemoteButtonCodeSpecialMask   = (1L << 17L),
        kHIDRemoteButtonCodeAluminumMask  = (1L << 21L), // PRIVATE - only used internally

        /* Hold button standard codes - available for white plastic and aluminum remote */
        kHIDRemoteButtonCodeUpHold       = (kHIDRemoteButtonCodeHoldMask|kHIDRemoteButtonCodeUp),
        kHIDRemoteButtonCodeDownHold     = (kHIDRemoteButtonCodeHoldMask|kHIDRemoteButtonCodeDown),
        kHIDRemoteButtonCodeLeftHold     = (kHIDRemoteButtonCodeHoldMask|kHIDRemoteButtonCodeLeft),
        kHIDRemoteButtonCodeRightHold    = (kHIDRemoteButtonCodeHoldMask|kHIDRemoteButtonCodeRight),
        kHIDRemoteButtonCodeCenterHold   = (kHIDRemoteButtonCodeHoldMask|kHIDRemoteButtonCodeCenter),
        kHIDRemoteButtonCodeMenuHold     = (kHIDRemoteButtonCodeHoldMask|kHIDRemoteButtonCodeMenu),

        /* Hold button extra codes - Only available for aluminum version of the remote */
        kHIDRemoteButtonCodePlayHold      = (kHIDRemoteButtonCodeHoldMask|kHIDRemoteButtonCodePlay),

        /* DEPRECATED codes - compatibility with HIDRemote 1.0 */
        kHIDRemoteButtonCodePlus          = kHIDRemoteButtonCodeUp,
        kHIDRemoteButtonCodePlusHold      = kHIDRemoteButtonCodeUpHold,
        kHIDRemoteButtonCodeMinus         = kHIDRemoteButtonCodeDown,
        kHIDRemoteButtonCodeMinusHold     = kHIDRemoteButtonCodeDownHold,
        kHIDRemoteButtonCodePlayPause     = kHIDRemoteButtonCodeCenter,
        kHIDRemoteButtonCodePlayPauseHold = kHIDRemoteButtonCodeCenterHold,

        /* Special purpose codes */
        kHIDRemoteButtonCodeIDChanged  = (kHIDRemoteButtonCodeSpecialMask|(1L << 18L)), // (the ID of the connected remote has changed, you can safely ignore this)
        #ifdef _HIDREMOTE_EXTENSIONS
                #define _HIDREMOTE_EXTENSIONS_SECTION 1
                #include "HIDRemoteAdditions.h"
                #undef _HIDREMOTE_EXTENSIONS_SECTION
        #endif /* _HIDREMOTE_EXTENSIONS */
} HIDRemoteButtonCode;

typedef enum
{
        kHIDRemoteModelUndetermined = 0L,                               // Assume a white plastic remote
        kHIDRemoteModelWhitePlastic,                                    // Signal *likely* to be coming from a white plastic remote
        kHIDRemoteModelAluminum                                         // Signal *definitely* coming from an aluminum remote
} HIDRemoteModel;

typedef enum
{
        kHIDRemoteAluminumRemoteSupportLevelNone = 0L,                  // This system has no support for the Aluminum Remote at all
        kHIDRemoteAluminumRemoteSupportLevelEmulation,                  // This system possibly has support for the Aluminum Remote (via emulation)
        kHIDRemoteAluminumRemoteSupportLevelNative                      // This system has native support for the Aluminum Remote
} HIDRemoteAluminumRemoteSupportLevel;

@class HIDRemote;

#pragma mark -- Delegate protocol (mandatory) --
@protocol HIDRemoteDelegate

// Notification of button events
- (void)hidRemote:(HIDRemote *)hidRemote                                // The instance of HIDRemote sending this
        eventWithButton:(HIDRemoteButtonCode)buttonCode                 // Event for the button specified by code
        isPressed:(BOOL)isPressed                                       // The button was pressed (YES) / released (NO)
        fromHardwareWithAttributes:(NSMutableDictionary *)attributes;   // Information on the device this event comes from

@optional

// Notification of ID changes
- (void)hidRemote:(HIDRemote *)hidRemote                                // Invoked when the user switched to a remote control with a different ID
        remoteIDChangedOldID:(SInt32)old
        newID:(SInt32)newID
        forHardwareWithAttributes:(NSMutableDictionary *)attributes;

// Notification about hardware additions/removals
- (void)hidRemote:(HIDRemote *)hidRemote                                // Invoked when new hardware was found / added to HIDRemote's pool
        foundNewHardwareWithAttributes:(NSMutableDictionary *)attributes;

- (void)hidRemote:(HIDRemote *)hidRemote                                // Invoked when initialization of new hardware as requested failed
        failedNewHardwareWithError:(NSError *)error;

- (void)hidRemote:(HIDRemote *)hidRemote                                // Invoked when hardware was removed from HIDRemote's pool
        releasedHardwareWithAttributes:(NSMutableDictionary *)attributes;

// ### WARNING: Unless you know VERY PRECISELY what you are doing, do not implement any of the delegate methods below. ###

// Matching of newly found receiver hardware
- (BOOL)hidRemote:(HIDRemote *)hidRemote                                // Invoked when new hardware is inspected
        inspectNewHardwareWithService:(io_service_t)service             //
        prematchResult:(BOOL)prematchResult;                            // Return YES if HIDRemote should go on with this hardware and try
                                                                        // to use it, or NO if it should not be persued further.

// Exlusive lock lending
- (BOOL)hidRemote:(HIDRemote *)hidRemote
        lendExclusiveLockToApplicationWithInfo:(NSDictionary *)applicationInfo;

- (void)hidRemote:(HIDRemote *)hidRemote
        exclusiveLockReleasedByApplicationWithInfo:(NSDictionary *)applicationInfo;

- (BOOL)hidRemote:(HIDRemote *)hidRemote
        shouldRetryExclusiveLockWithInfo:(NSDictionary *)applicationInfo;

@end


#pragma mark -- Actual header file for class  --

@interface HIDRemote : NSObject
{
        // IOMasterPort
        mach_port_t _masterPort;

        // Notification ports
        IONotificationPortRef _notifyPort;
        CFRunLoopSourceRef _notifyRLSource;

        // Matching iterator
        io_iterator_t _matchingServicesIterator;

        // SecureInput notification
        io_object_t _secureInputNotification;

        // Service attributes
        NSMutableDictionary *_serviceAttribMap;

        // Mode
        HIDRemoteMode _mode;
        BOOL _autoRecover;
        NSTimer *_autoRecoveryTimer;

        // Delegate
        NSObject <HIDRemoteDelegate> *_delegate;

        // Last seen ID and remote model
        SInt32 _lastSeenRemoteID;
        HIDRemoteModel _lastSeenModel;
        SInt32 _lastSeenModelRemoteID;

        // Unused button codes
        NSArray *_unusedButtonCodes;

        // Simulate Plus/Minus Hold
        BOOL _simulateHoldEvents;

        // SecureEventInput workaround
        BOOL _secureEventInputWorkAround;
        UInt64 _lastSecureEventInputPIDSum;
        uid_t _lastFrontUserSession;

        // Exclusive lock lending
        BOOL _exclusiveLockLending;
        BOOL _sendExclusiveResourceReuseNotification;
        NSNumber *_waitForReturnByPID;
        NSNumber *_returnToPID;
        BOOL _isRestarting;

        // Status notifications
        BOOL _sendStatusNotifications;
        NSString *_pidString;

        // Status
        BOOL _applicationIsTerminating;
        BOOL _isStopping;

        // Thread safety
        #ifdef HIDREMOTE_THREADSAFETY_HARDENED_NOTIFICATION_HANDLING /* #define HIDREMOTE_THREADSAFETY_HARDENED_NOTIFICATION_HANDLING if you're running your HIDRemote instance on a background thread (requires OS X 10.5 or later) */
        NSThread *_runOnThread;
        #endif
}

#pragma mark -- PUBLIC: Shared HID Remote --
+ (HIDRemote *)sharedHIDRemote;

#pragma mark -- PUBLIC: System Information --
+ (BOOL)isCandelairInstalled;
+ (BOOL)isCandelairInstallationRequiredForRemoteMode:(HIDRemoteMode)remoteMode;
- (HIDRemoteAluminumRemoteSupportLevel)aluminiumRemoteSystemSupportLevel;

#pragma mark -- PUBLIC: Interface / API --
- (BOOL)startRemoteControl:(HIDRemoteMode)hidRemoteMode;
- (void)stopRemoteControl;

- (BOOL)isStarted;
- (HIDRemoteMode)startedInMode;

- (unsigned)activeRemoteControlCount;

- (SInt32)lastSeenRemoteControlID;

- (void)setLastSeenModel:(HIDRemoteModel)aModel;
- (HIDRemoteModel)lastSeenModel;

- (void)setDelegate:(NSObject <HIDRemoteDelegate> *)newDelegate;
- (NSObject <HIDRemoteDelegate> *)delegate;

- (void)setSimulateHoldEvents:(BOOL)newSimulateHoldEvents;
- (BOOL)simulateHoldEvents;

- (void)setUnusedButtonCodes:(NSArray *)newArrayWithUnusedButtonCodesAsNSNumbers;
- (NSArray *)unusedButtonCodes;

#pragma mark -- PUBLIC: Expert APIs --
- (void)setEnableSecureEventInputWorkaround:(BOOL)newEnableSecureEventInputWorkaround;
- (BOOL)enableSecureEventInputWorkaround;

- (void)setExclusiveLockLendingEnabled:(BOOL)newExclusiveLockLendingEnabled;
- (BOOL)exclusiveLockLendingEnabled;

- (BOOL)isApplicationTerminating;
- (BOOL)isStopping;

#pragma mark -- PRIVATE: HID Event handling --
- (void)_handleButtonCode:(HIDRemoteButtonCode)buttonCode isPressed:(BOOL)isPressed hidAttribsDict:(NSMutableDictionary *)hidAttribsDict;
- (void)_sendButtonCode:(HIDRemoteButtonCode)buttonCode isPressed:(BOOL)isPressed hidAttribsDict:(NSMutableDictionary *)hidAttribsDict;
- (void)_hidEventFor:(io_service_t)hidDevice from:(IOHIDQueueInterface **)interface withResult:(IOReturn)result;

#pragma mark -- PRIVATE: Service setup and destruction --
- (BOOL)_prematchService:(io_object_t)service;
- (HIDRemoteButtonCode)buttonCodeForUsage:(unsigned int)usage usagePage:(unsigned int)usagePage;
- (BOOL)_setupService:(io_object_t)service;
- (void)_destructService:(io_object_t)service;

#pragma mark -- PRIVATE: Distributed notifiations handling --
- (void)_postStatusWithAction:(NSString *)action;
- (void)_handleNotifications:(NSNotification *)notification;
- (void)_setSendStatusNotifications:(BOOL)doSend;
- (BOOL)_sendStatusNotifications;

#pragma mark -- PRIVATE: Application becomes active / inactive handling for kHIDRemoteModeExclusiveAuto --
- (void)_appStatusChanged:(NSNotification *)notification;
- (void)_delayedAutoRecovery:(NSTimer *)aTimer;

#pragma mark -- PRIVATE: Notification handling --
- (void)_serviceMatching:(io_iterator_t)iterator;
- (void)_serviceNotificationFor:(io_service_t)service messageType:(natural_t)messageType messageArgument:(void *)messageArgument;
- (void)_updateSessionInformation;
- (void)_secureInputNotificationFor:(io_service_t)service messageType:(natural_t)messageType messageArgument:(void *)messageArgument;

@end

#pragma mark -- Information attribute keys --
extern NSString *kHIDRemoteManufacturer;
extern NSString *kHIDRemoteProduct;
extern NSString *kHIDRemoteTransport;

#pragma mark -- Internal/Expert attribute keys (AKA: don't touch these unless you really, really, REALLY know what you do) --
extern NSString *kHIDRemoteCFPluginInterface;
extern NSString *kHIDRemoteHIDDeviceInterface;
extern NSString *kHIDRemoteCookieButtonCodeLUT;
extern NSString *kHIDRemoteHIDQueueInterface;
extern NSString *kHIDRemoteServiceNotification;
extern NSString *kHIDRemoteCFRunLoopSource;
extern NSString *kHIDRemoteLastButtonPressed;
extern NSString *kHIDRemoteService;
extern NSString *kHIDRemoteSimulateHoldEventsTimer;
extern NSString *kHIDRemoteSimulateHoldEventsOriginButtonCode;
extern NSString *kHIDRemoteAluminumRemoteSupportLevel;
extern NSString *kHIDRemoteAluminumRemoteSupportOnDemand;

#pragma mark -- Distributed notifications --
extern NSString *kHIDRemoteDNHIDRemotePing;
extern NSString *kHIDRemoteDNHIDRemoteRetry;
extern NSString *kHIDRemoteDNHIDRemoteStatus;

extern NSString *kHIDRemoteDNHIDRemoteRetryGlobalObject;

#pragma mark -- Distributed notifications userInfo keys and values --
extern NSString *kHIDRemoteDNStatusHIDRemoteVersionKey;
extern NSString *kHIDRemoteDNStatusPIDKey;
extern NSString *kHIDRemoteDNStatusModeKey;
extern NSString *kHIDRemoteDNStatusUnusedButtonCodesKey;
extern NSString *kHIDRemoteDNStatusRemoteControlCountKey;
extern NSString *kHIDRemoteDNStatusReturnToPIDKey;
extern NSString *kHIDRemoteDNStatusActionKey;
extern NSString *kHIDRemoteDNStatusActionStart;
extern NSString *kHIDRemoteDNStatusActionStop;
extern NSString *kHIDRemoteDNStatusActionUpdate;
extern NSString *kHIDRemoteDNStatusActionNoNeed;

#pragma mark -- Driver compatibility flags --
typedef enum
{
        kHIDRemoteCompatibilityFlagsStandardHIDRemoteDevice = 1L,
} HIDRemoteCompatibilityFlags;
