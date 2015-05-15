//
//  HIDRemote.m
//  HIDRemote V1.2 (27th May 2011)
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

#import "HIDRemote.h"

// Callback Prototypes
static void HIDEventCallback(   void * target,
                                IOReturn result,
                                void * refcon,
                                void * sender);

static void ServiceMatchingCallback(    void *refCon,
                                        io_iterator_t iterator);

static void ServiceNotificationCallback(void *          refCon,
                                        io_service_t    service,
                                        natural_t       messageType,
                                        void *          messageArgument);

static void SecureInputNotificationCallback(    void *          refCon,
                                                io_service_t    service,
                                                natural_t       messageType,
                                                void *          messageArgument);

// Shared HIDRemote instance
static HIDRemote *sHIDRemote = nil;

@implementation HIDRemote

#pragma mark -- Init, dealloc & shared instance --

+ (HIDRemote *)sharedHIDRemote
{
        if (sHIDRemote==nil)
        {
                sHIDRemote = [[HIDRemote alloc] init];
        }

        return (sHIDRemote);
}

- (id)init
{
        if ((self = [super init]) != nil)
        {
                #ifdef HIDREMOTE_THREADSAFETY_HARDENED_NOTIFICATION_HANDLING
                _runOnThread = [[NSThread currentThread] retain];
                #endif

                // Detect application becoming active/inactive
                [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_appStatusChanged:)    name:NSApplicationDidBecomeActiveNotification  object:NSApp];
                [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_appStatusChanged:)    name:NSApplicationWillResignActiveNotification object:NSApp];
                [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_appStatusChanged:)    name:NSApplicationWillTerminateNotification    object:NSApp];

                // Handle distributed notifications
                _pidString = [[NSString alloc] initWithFormat:@"%d", getpid()];

                [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(_handleNotifications:) name:kHIDRemoteDNHIDRemotePing      object:nil];
                [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(_handleNotifications:) name:kHIDRemoteDNHIDRemoteRetry     object:kHIDRemoteDNHIDRemoteRetryGlobalObject];
                [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(_handleNotifications:) name:kHIDRemoteDNHIDRemoteRetry     object:_pidString];

                // Enabled by default: simulate hold events for plus/minus
                _simulateHoldEvents = YES;

                // Enabled by default: work around for a locking issue introduced with Security Update 2008-004 / 10.4.9 and beyond (credit for finding this workaround goes to Martin Kahr)
                _secureEventInputWorkAround = YES;
                _secureInputNotification = 0;

                // Initialize instance variables
                _lastSeenRemoteID = -1;
                _lastSeenModel = kHIDRemoteModelUndetermined;
                _unusedButtonCodes = [[NSMutableArray alloc] init];
                _exclusiveLockLending = NO;
                _sendExclusiveResourceReuseNotification = YES;
                _applicationIsTerminating = NO;

                // Send status notifications
                _sendStatusNotifications = YES;
        }

        return (self);
}

- (void)dealloc
{
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillTerminateNotification object:NSApp];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillResignActiveNotification object:NSApp];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationDidBecomeActiveNotification object:NSApp];

        [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kHIDRemoteDNHIDRemotePing  object:nil];
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kHIDRemoteDNHIDRemoteRetry object:kHIDRemoteDNHIDRemoteRetryGlobalObject];
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kHIDRemoteDNHIDRemoteRetry object:_pidString];
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:nil object:nil]; /* As demanded by the documentation for -[NSDistributedNotificationCenter removeObserver:name:object:] */

        [self stopRemoteControl];

        [self setExclusiveLockLendingEnabled:NO];

        [self setDelegate:nil];

        if (_unusedButtonCodes != nil)
        {
                [_unusedButtonCodes release];
                _unusedButtonCodes = nil;
        }

        #ifdef HIDREMOTE_THREADSAFETY_HARDENED_NOTIFICATION_HANDLING
        [_runOnThread release];
        _runOnThread = nil;
        #endif

        [_pidString release];
        _pidString = nil;

        [super dealloc];
}

#pragma mark -- PUBLIC: System Information --
+ (BOOL)isCandelairInstalled
{
        mach_port_t     masterPort = 0;
        kern_return_t   kernResult;
        io_service_t    matchingService = 0;
        BOOL isInstalled = NO;

        kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
        if ((kernResult!=kIOReturnSuccess) || (masterPort==0)) { return(NO); }

        if ((matchingService = IOServiceGetMatchingService(masterPort, IOServiceMatching("IOSPIRITIRController"))) != 0)
        {
                isInstalled = YES;
                IOObjectRelease((io_object_t) matchingService);
        }

        mach_port_deallocate(mach_task_self(), masterPort);

        return (isInstalled);
}

+ (BOOL)isCandelairInstallationRequiredForRemoteMode:(HIDRemoteMode)remoteMode
{
        return (NO);
}

- (HIDRemoteAluminumRemoteSupportLevel)aluminiumRemoteSystemSupportLevel
{
        HIDRemoteAluminumRemoteSupportLevel supportLevel = kHIDRemoteAluminumRemoteSupportLevelNone;
        NSEnumerator *attribDictsEnum;
        NSDictionary *hidAttribsDict;

        attribDictsEnum = [_serviceAttribMap objectEnumerator];

        while ((hidAttribsDict = [attribDictsEnum nextObject]) != nil)
        {
                NSNumber *deviceSupportLevel;

                if ((deviceSupportLevel = [hidAttribsDict objectForKey:kHIDRemoteAluminumRemoteSupportLevel]) != nil)
                {
                        if ([deviceSupportLevel intValue] > (int)supportLevel)
                        {
                                supportLevel = [deviceSupportLevel intValue];
                        }
                }
        }

        return (supportLevel);
}

#pragma mark -- PUBLIC: Interface / API --
- (BOOL)startRemoteControl:(HIDRemoteMode)hidRemoteMode
{
        if ((_mode == kHIDRemoteModeNone) && (hidRemoteMode != kHIDRemoteModeNone))
        {
                kern_return_t           kernReturn;
                CFMutableDictionaryRef  matchDict=NULL;
                io_service_t rootService;

                do
                {
                        // Get IOKit master port
                        kernReturn = IOMasterPort(bootstrap_port, &_masterPort);
                        if ((kernReturn!=kIOReturnSuccess) || (_masterPort==0)) { break; }

                        // Setup notification port
                        _notifyPort = IONotificationPortCreate(_masterPort);

                        if ((_notifyRLSource = IONotificationPortGetRunLoopSource(_notifyPort)) != NULL)
                        {
                                CFRunLoopAddSource(     CFRunLoopGetCurrent(),
                                                        _notifyRLSource,
                                                        kCFRunLoopCommonModes);
                        }
                        else
                        {
                                break;
                        }

                        // Setup SecureInput notification
                        if ((hidRemoteMode == kHIDRemoteModeExclusive) || (hidRemoteMode == kHIDRemoteModeExclusiveAuto))
                        {
                                if ((rootService = IORegistryEntryFromPath(_masterPort, kIOServicePlane ":/")) != 0)
                                {
                                        kernReturn = IOServiceAddInterestNotification(  _notifyPort,
                                                                                        rootService,
                                                                                        kIOBusyInterest,
                                                                                        SecureInputNotificationCallback,
                                                                                        (void *)self,
                                                                                        &_secureInputNotification);
                                        if (kernReturn != kIOReturnSuccess) { break; }

                                        [self _updateSessionInformation];
                                }
                                else
                                {
                                        break;
                                }
                        }

                        // Setup notification matching dict
                        matchDict = IOServiceMatching(kIOHIDDeviceKey);
                        CFRetain(matchDict);

                        // Actually add notification
                        kernReturn = IOServiceAddMatchingNotification(  _notifyPort,
                                                                        kIOFirstMatchNotification,
                                                                        matchDict,                      // one reference count consumed by this call
                                                                        ServiceMatchingCallback,
                                                                        (void *) self,
                                                                        &_matchingServicesIterator);
                        if (kernReturn != kIOReturnSuccess) { break; }

                        // Setup serviceAttribMap
                        _serviceAttribMap = [[NSMutableDictionary alloc] init];
                        if (_serviceAttribMap==nil) { break; }

                        // Phew .. everything went well!
                        _mode = hidRemoteMode;
                        CFRelease(matchDict);

                        [self _serviceMatching:_matchingServicesIterator];

                        [self _postStatusWithAction:kHIDRemoteDNStatusActionStart];

                        return (YES);

                }while(0);

                // An error occured. Do necessary clean up.
                if (matchDict!=NULL)
                {
                        CFRelease(matchDict);
                        matchDict = NULL;
                }

                [self stopRemoteControl];
        }

        return (NO);
}

- (void)stopRemoteControl
{
        UInt32 serviceCount = 0;

        _autoRecover = NO;
        _isStopping = YES;

        if (_autoRecoveryTimer!=nil)
        {
                [_autoRecoveryTimer invalidate];
                [_autoRecoveryTimer release];
                _autoRecoveryTimer = nil;
        }

        if (_serviceAttribMap!=nil)
        {
                NSDictionary *cloneDict = [[NSDictionary alloc] initWithDictionary:_serviceAttribMap];

                if (cloneDict!=nil)
                {
                        NSEnumerator *mapKeyEnum = [cloneDict keyEnumerator];
                        NSNumber *serviceValue;

                        while ((serviceValue = [mapKeyEnum nextObject]) != nil)
                        {
                                [self _destructService:(io_object_t)[serviceValue unsignedIntValue]];
                                serviceCount++;
                        };

                        [cloneDict release];
                        cloneDict = nil;
                }

                [_serviceAttribMap release];
                _serviceAttribMap = nil;
        }

        if (_matchingServicesIterator!=0)
        {
                IOObjectRelease((io_object_t) _matchingServicesIterator);
                _matchingServicesIterator = 0;
        }

        if (_secureInputNotification!=0)
        {
                IOObjectRelease((io_object_t) _secureInputNotification);
                _secureInputNotification = 0;
        }

        if (_notifyRLSource!=NULL)
        {
                CFRunLoopSourceInvalidate(_notifyRLSource);
                _notifyRLSource = NULL;
        }

        if (_notifyPort!=NULL)
        {
                IONotificationPortDestroy(_notifyPort);
                _notifyPort = NULL;
        }

        if (_masterPort!=0)
        {
                mach_port_deallocate(mach_task_self(), _masterPort);
                _masterPort = 0;
        }

        if (_returnToPID!=nil)
        {
                [_returnToPID release];
                _returnToPID = nil;
        }

        if (_mode!=kHIDRemoteModeNone)
        {
                // Post status
                [self _postStatusWithAction:kHIDRemoteDNStatusActionStop];

                if (_sendStatusNotifications)
                {
                        // In case we were not ready to lend it earlier, tell other HIDRemote apps that the resources (if any were used) are now again available for use by other applications
                        if (((_mode==kHIDRemoteModeExclusive) || (_mode==kHIDRemoteModeExclusiveAuto)) && (_sendExclusiveResourceReuseNotification==YES) && (_exclusiveLockLending==NO) && (serviceCount>0))
                        {
                                _mode = kHIDRemoteModeNone;

                                if (!_isRestarting)
                                {
                                        [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kHIDRemoteDNHIDRemoteRetry
                                                                                                       object:kHIDRemoteDNHIDRemoteRetryGlobalObject
                                                                                                     userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                                                                [NSNumber numberWithUnsignedInt:(unsigned int)getpid()], kHIDRemoteDNStatusPIDKey,
                                                                                                                [[NSBundle mainBundle] bundleIdentifier],                (NSString *)kCFBundleIdentifierKey,
                                                                                                               nil]
                                                                                           deliverImmediately:YES];
                                }
                        }
                }
        }

        _mode = kHIDRemoteModeNone;
        _isStopping = NO;
}

- (BOOL)isStarted
{
        return (_mode != kHIDRemoteModeNone);
}

- (HIDRemoteMode)startedInMode
{
        return (_mode);
}

- (unsigned)activeRemoteControlCount
{
        return ([_serviceAttribMap count]);
}

- (SInt32)lastSeenRemoteControlID
{
        return (_lastSeenRemoteID);
}

- (HIDRemoteModel)lastSeenModel
{
        return (_lastSeenModel);
}

- (void)setLastSeenModel:(HIDRemoteModel)aModel
{
        _lastSeenModel = aModel;
}

- (void)setSimulateHoldEvents:(BOOL)newSimulateHoldEvents
{
        _simulateHoldEvents = newSimulateHoldEvents;
}

- (BOOL)simulateHoldEvents
{
        return (_simulateHoldEvents);
}

- (NSArray *)unusedButtonCodes
{
        return (_unusedButtonCodes);
}

- (void)setUnusedButtonCodes:(NSArray *)newArrayWithUnusedButtonCodesAsNSNumbers
{
        [newArrayWithUnusedButtonCodesAsNSNumbers retain];
        [_unusedButtonCodes release];

        _unusedButtonCodes = newArrayWithUnusedButtonCodesAsNSNumbers;

        [self _postStatusWithAction:kHIDRemoteDNStatusActionUpdate];
}

- (void)setDelegate:(NSObject <HIDRemoteDelegate> *)newDelegate
{
        _delegate = newDelegate;
}

- (NSObject <HIDRemoteDelegate> *)delegate
{
        return (_delegate);
}

#pragma mark -- PUBLIC: Expert APIs --
- (void)setEnableSecureEventInputWorkaround:(BOOL)newEnableSecureEventInputWorkaround
{
        _secureEventInputWorkAround = newEnableSecureEventInputWorkaround;
}

- (BOOL)enableSecureEventInputWorkaround
{
        return (_secureEventInputWorkAround);
}

- (void)setExclusiveLockLendingEnabled:(BOOL)newExclusiveLockLendingEnabled
{
        if (newExclusiveLockLendingEnabled != _exclusiveLockLending)
        {
                _exclusiveLockLending = newExclusiveLockLendingEnabled;

                if (_exclusiveLockLending)
                {
                        [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(_handleNotifications:) name:kHIDRemoteDNHIDRemoteStatus object:nil];
                }
                else
                {
                        [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kHIDRemoteDNHIDRemoteStatus object:nil];

                        [_waitForReturnByPID release];
                        _waitForReturnByPID = nil;
                }
        }
}

- (BOOL)exclusiveLockLendingEnabled
{
        return (_exclusiveLockLending);
}

- (void)setSendExclusiveResourceReuseNotification:(BOOL)newSendExclusiveResourceReuseNotification
{
        _sendExclusiveResourceReuseNotification = newSendExclusiveResourceReuseNotification;
}

- (BOOL)sendExclusiveResourceReuseNotification
{
        return (_sendExclusiveResourceReuseNotification);
}

- (BOOL)isApplicationTerminating
{
        return (_applicationIsTerminating);
}

- (BOOL)isStopping
{
        return (_isStopping);
}

#pragma mark -- PRIVATE: Application becomes active / inactive handling for kHIDRemoteModeExclusiveAuto --
- (void)_appStatusChanged:(NSNotification *)notification
{
        #ifdef HIDREMOTE_THREADSAFETY_HARDENED_NOTIFICATION_HANDLING
        if ([self respondsToSelector:@selector(performSelector:onThread:withObject:waitUntilDone:)]) // OS X 10.5+ only
        {
                if ([NSThread currentThread] != _runOnThread)
                {
                        if ([[notification name] isEqual:NSApplicationDidBecomeActiveNotification])
                        {
                                if (!_autoRecover)
                                {
                                        return;
                                }
                        }

                        if ([[notification name] isEqual:NSApplicationWillResignActiveNotification])
                        {
                                if (_mode != kHIDRemoteModeExclusiveAuto)
                                {
                                        return;
                                }
                        }

                        [self performSelector:@selector(_appStatusChanged:) onThread:_runOnThread withObject:notification waitUntilDone:[[notification name] isEqual:NSApplicationWillTerminateNotification]];
                        return;
                }
        }
        #endif

        if (notification!=nil)
        {
                if (_autoRecoveryTimer!=nil)
                {
                        [_autoRecoveryTimer invalidate];
                        [_autoRecoveryTimer release];
                        _autoRecoveryTimer = nil;
                }

                if ([[notification name] isEqual:NSApplicationDidBecomeActiveNotification])
                {
                        if (_autoRecover)
                        {
                                // Delay autorecover by 0.1 to avoid race conditions
                                if ((_autoRecoveryTimer = [[NSTimer alloc] initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:0.1] interval:0.1 target:self selector:@selector(_delayedAutoRecovery:) userInfo:nil repeats:NO]) != nil)
                                {
                                        // Using CFRunLoopAddTimer instead of [[NSRunLoop currentRunLoop] addTimer:.. for consistency with run loop modes.
                                        // The kCFRunLoopCommonModes counterpart NSRunLoopCommonModes is only available in 10.5 and later, whereas this code
                                        // is designed to be also compatible with 10.4. CFRunLoopTimerRef is "toll-free-bridged" with NSTimer since 10.0.
                                        CFRunLoopAddTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)_autoRecoveryTimer, kCFRunLoopCommonModes);
                                }
                        }
                }

                if ([[notification name] isEqual:NSApplicationWillResignActiveNotification])
                {
                        if (_mode == kHIDRemoteModeExclusiveAuto)
                        {
                                [self stopRemoteControl];
                                _autoRecover = YES;
                        }
                }

                if ([[notification name] isEqual:NSApplicationWillTerminateNotification])
                {
                        _applicationIsTerminating = YES;

                        if ([self isStarted])
                        {
                                [self stopRemoteControl];
                        }
                }
        }
}

- (void)_delayedAutoRecovery:(NSTimer *)aTimer
{
        [_autoRecoveryTimer invalidate];
        [_autoRecoveryTimer release];
        _autoRecoveryTimer = nil;

        if (_autoRecover)
        {
                [self startRemoteControl:kHIDRemoteModeExclusiveAuto];
                _autoRecover = NO;
        }
}


#pragma mark -- PRIVATE: Distributed notifiations handling --
- (void)_postStatusWithAction:(NSString *)action
{
        if (_sendStatusNotifications)
        {
                [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kHIDRemoteDNHIDRemoteStatus
                                                                               object:((_pidString!=nil) ? _pidString : [NSString stringWithFormat:@"%d",getpid()])
                                                                             userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                                                [NSNumber numberWithInt:1],                                                     kHIDRemoteDNStatusHIDRemoteVersionKey,
                                                                                                [NSNumber numberWithUnsignedInt:(unsigned int)getpid()],                        kHIDRemoteDNStatusPIDKey,
                                                                                                [NSNumber numberWithInt:(int)_mode],                                            kHIDRemoteDNStatusModeKey,
                                                                                                [NSNumber numberWithUnsignedInt:(unsigned int)[self activeRemoteControlCount]], kHIDRemoteDNStatusRemoteControlCountKey,
                                                                                                ((_unusedButtonCodes!=nil) ? _unusedButtonCodes : [NSArray array]),             kHIDRemoteDNStatusUnusedButtonCodesKey,
                                                                                                action,                                                                         kHIDRemoteDNStatusActionKey,
                                                                                                [[NSBundle mainBundle] bundleIdentifier],                                       (NSString *)kCFBundleIdentifierKey,
                                                                                                _returnToPID,                                                                   kHIDRemoteDNStatusReturnToPIDKey,
                                                                                      nil]
                                                                   deliverImmediately:YES
                ];
        }
}

- (void)_handleNotifications:(NSNotification *)notification
{
        NSString *notificationName;

        #ifdef HIDREMOTE_THREADSAFETY_HARDENED_NOTIFICATION_HANDLING
        if ([self respondsToSelector:@selector(performSelector:onThread:withObject:waitUntilDone:)]) // OS X 10.5+ only
        {
                if ([NSThread currentThread] != _runOnThread)
                {
                        [self performSelector:@selector(_handleNotifications:) onThread:_runOnThread withObject:notification waitUntilDone:NO];
                        return;
                }
        }
        #endif

        if ((notification!=nil) && ((notificationName = [notification name]) != nil))
        {
                if ([notificationName isEqual:kHIDRemoteDNHIDRemotePing])
                {
                        [self _postStatusWithAction:kHIDRemoteDNStatusActionUpdate];
                }

                if ([notificationName isEqual:kHIDRemoteDNHIDRemoteRetry])
                {
                        if ([self isStarted])
                        {
                                BOOL retry = YES;

                                // Ignore our own global retry broadcasts
                                if ([[notification object] isEqual:kHIDRemoteDNHIDRemoteRetryGlobalObject])
                                {
                                        NSNumber *fromPID;

                                        if ((fromPID = [[notification userInfo] objectForKey:kHIDRemoteDNStatusPIDKey]) != nil)
                                        {
                                                if (getpid() == (int)[fromPID unsignedIntValue])
                                                {
                                                        retry = NO;
                                                }
                                        }
                                }

                                if (retry)
                                {
                                        if (([self delegate] != nil) &&
                                            ([[self delegate] respondsToSelector:@selector(hidRemote:shouldRetryExclusiveLockWithInfo:)]))
                                        {
                                                retry = [[self delegate] hidRemote:self shouldRetryExclusiveLockWithInfo:[notification userInfo]];
                                        }
                                }

                                if (retry)
                                {
                                        HIDRemoteMode restartInMode = _mode;

                                        if (restartInMode != kHIDRemoteModeNone)
                                        {
                                                _isRestarting = YES;
                                                [self stopRemoteControl];

                                                [_returnToPID release];
                                                _returnToPID = nil;

                                                [self startRemoteControl:restartInMode];
                                                _isRestarting = NO;

                                                if (restartInMode != kHIDRemoteModeShared)
                                                {
                                                        _returnToPID = [[[notification userInfo] objectForKey:kHIDRemoteDNStatusPIDKey] retain];
                                                }
                                        }
                                }
                                else
                                {
                                        NSNumber *cacheReturnPID = _returnToPID;

                                        _returnToPID = [[[notification userInfo] objectForKey:kHIDRemoteDNStatusPIDKey] retain];
                                        [self _postStatusWithAction:kHIDRemoteDNStatusActionNoNeed];
                                        [_returnToPID release];

                                        _returnToPID = cacheReturnPID;
                                }
                        }
                }

                if (_exclusiveLockLending)
                {
                        if ([notificationName isEqual:kHIDRemoteDNHIDRemoteStatus])
                        {
                                NSString *action;

                                if ((action = [[notification userInfo] objectForKey:kHIDRemoteDNStatusActionKey]) != nil)
                                {
                                        if ((_mode == kHIDRemoteModeNone) && (_waitForReturnByPID!=nil))
                                        {
                                                NSNumber *pidNumber, *returnToPIDNumber;

                                                if ((pidNumber          = [[notification userInfo] objectForKey:kHIDRemoteDNStatusPIDKey]) != nil)
                                                {
                                                        returnToPIDNumber = [[notification userInfo] objectForKey:kHIDRemoteDNStatusReturnToPIDKey];

                                                        if ([action isEqual:kHIDRemoteDNStatusActionStart])
                                                        {
                                                                if ([pidNumber isEqual:_waitForReturnByPID])
                                                                {
                                                                        NSNumber *startMode;

                                                                         if ((startMode = [[notification userInfo] objectForKey:kHIDRemoteDNStatusModeKey]) != nil)
                                                                         {
                                                                                if ([startMode intValue] == kHIDRemoteModeShared)
                                                                                {
                                                                                        returnToPIDNumber = [NSNumber numberWithInt:getpid()];
                                                                                        action = kHIDRemoteDNStatusActionNoNeed;
                                                                                }
                                                                         }
                                                                }
                                                        }

                                                        if (returnToPIDNumber != nil)
                                                        {
                                                                if ([action isEqual:kHIDRemoteDNStatusActionStop] || [action isEqual:kHIDRemoteDNStatusActionNoNeed])
                                                                {
                                                                        if ([pidNumber isEqual:_waitForReturnByPID] && ([returnToPIDNumber intValue] == getpid()))
                                                                        {
                                                                                [_waitForReturnByPID release];
                                                                                _waitForReturnByPID = nil;

                                                                                if (([self delegate] != nil) &&
                                                                                    ([[self delegate] respondsToSelector:@selector(hidRemote:exclusiveLockReleasedByApplicationWithInfo:)]))
                                                                                {
                                                                                        [[self delegate] hidRemote:self exclusiveLockReleasedByApplicationWithInfo:[notification userInfo]];
                                                                                }
                                                                                else
                                                                                {
                                                                                        [self startRemoteControl:kHIDRemoteModeExclusive];
                                                                                }
                                                                        }
                                                                }
                                                        }
                                                }
                                        }

                                        if (_mode==kHIDRemoteModeExclusive)
                                        {
                                                if ([action isEqual:kHIDRemoteDNStatusActionStart])
                                                {
                                                        NSNumber *originPID = [[notification userInfo] objectForKey:kHIDRemoteDNStatusPIDKey];
                                                        BOOL lendLock = YES;

                                                        if ([originPID intValue] != getpid())
                                                        {
                                                                if (([self delegate] != nil) &&
                                                                    ([[self delegate] respondsToSelector:@selector(hidRemote:lendExclusiveLockToApplicationWithInfo:)]))
                                                                {
                                                                        lendLock = [[self delegate] hidRemote:self lendExclusiveLockToApplicationWithInfo:[notification userInfo]];
                                                                }

                                                                if (lendLock)
                                                                {
                                                                        [_waitForReturnByPID release];
                                                                        _waitForReturnByPID = [originPID retain];

                                                                        if (_waitForReturnByPID != nil)
                                                                        {
                                                                                [self stopRemoteControl];

                                                                                [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kHIDRemoteDNHIDRemoteRetry
                                                                                                                                               object:[NSString stringWithFormat:@"%d", [_waitForReturnByPID intValue]]
                                                                                                                                             userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                                                                                                                [NSNumber numberWithUnsignedInt:(unsigned int)getpid()], kHIDRemoteDNStatusPIDKey,
                                                                                                                                                                [[NSBundle mainBundle] bundleIdentifier],                (NSString *)kCFBundleIdentifierKey,
                                                                                                                                                      nil]
                                                                                                                                   deliverImmediately:YES];
                                                                        }
                                                                }
                                                        }
                                                }
                                        }
                                }
                        }
                }
        }
}

- (void)_setSendStatusNotifications:(BOOL)doSend
{
        _sendStatusNotifications = doSend;
}

- (BOOL)_sendStatusNotifications
{
        return (_sendStatusNotifications);
}

#pragma mark -- PRIVATE: Service setup and destruction --
- (BOOL)_prematchService:(io_object_t)service
{
        BOOL serviceMatches = NO;
        NSString *ioClass;
        NSNumber *candelairHIDRemoteCompatibilityMask;

        if (service != 0)
        {
                // IOClass matching
                if ((ioClass = (NSString *)IORegistryEntryCreateCFProperty((io_registry_entry_t)service,
                                                                           CFSTR(kIOClassKey),
                                                                           kCFAllocatorDefault,
                                                                           0)) != nil)
                {
                        // Match on Apple's AppleIRController and old versions of the Remote Buddy IR Controller
                        if ([ioClass isEqual:@"AppleIRController"] || [ioClass isEqual:@"RBIOKitAIREmu"])
                        {
                                CFTypeRef candelairHIDRemoteCompatibilityDevice;

                                serviceMatches = YES;

                                if ((candelairHIDRemoteCompatibilityDevice = IORegistryEntryCreateCFProperty((io_registry_entry_t)service, CFSTR("CandelairHIDRemoteCompatibilityDevice"), kCFAllocatorDefault, 0)) != NULL)
                                {
                                        if (CFEqual(kCFBooleanTrue, candelairHIDRemoteCompatibilityDevice))
                                        {
                                                serviceMatches = NO;
                                        }

                                        CFRelease (candelairHIDRemoteCompatibilityDevice);
                                }
                        }

                        // Match on the virtual IOSPIRIT IR Controller
                        if ([ioClass isEqual:@"IOSPIRITIRController"])
                        {
                                serviceMatches = YES;
                        }

                        CFRelease((CFTypeRef)ioClass);
                }

                // Match on services that claim compatibility with the HID Remote class (Candelair or third-party) by having a property of CandelairHIDRemoteCompatibilityMask = 1 <Type: Number>
                if ((candelairHIDRemoteCompatibilityMask = (NSNumber *)IORegistryEntryCreateCFProperty((io_registry_entry_t)service, CFSTR("CandelairHIDRemoteCompatibilityMask"), kCFAllocatorDefault, 0)) != nil)
                {
                        if ([candelairHIDRemoteCompatibilityMask isKindOfClass:[NSNumber class]])
                        {
                                if ([candelairHIDRemoteCompatibilityMask unsignedIntValue] & kHIDRemoteCompatibilityFlagsStandardHIDRemoteDevice)
                                {
                                        serviceMatches = YES;
                                }
                                else
                                {
                                        serviceMatches = NO;
                                }
                        }

                        CFRelease((CFTypeRef)candelairHIDRemoteCompatibilityMask);
                }
        }

        if (([self delegate]!=nil) &&
            ([[self delegate] respondsToSelector:@selector(hidRemote:inspectNewHardwareWithService:prematchResult:)]))
        {
                serviceMatches = [((NSObject <HIDRemoteDelegate> *)[self delegate]) hidRemote:self inspectNewHardwareWithService:service prematchResult:serviceMatches];
        }

        return (serviceMatches);
}

- (HIDRemoteButtonCode)buttonCodeForUsage:(unsigned int)usage usagePage:(unsigned int)usagePage
{
        HIDRemoteButtonCode buttonCode = kHIDRemoteButtonCodeNone;

        switch (usagePage)
        {
                case kHIDPage_Consumer:
                        switch (usage)
                        {
                                case kHIDUsage_Csmr_MenuPick:
                                        // Aluminum Remote: Center
                                        buttonCode = (kHIDRemoteButtonCodeCenter|kHIDRemoteButtonCodeAluminumMask);
                                break;

                                case kHIDUsage_Csmr_ModeStep:
                                        // Aluminium Remote: Center Hold
                                        buttonCode = (kHIDRemoteButtonCodeCenterHold|kHIDRemoteButtonCodeAluminumMask);
                                break;

                                case kHIDUsage_Csmr_PlayOrPause:
                                        // Aluminum Remote: Play/Pause
                                        buttonCode = (kHIDRemoteButtonCodePlay|kHIDRemoteButtonCodeAluminumMask);
                                break;

                                case kHIDUsage_Csmr_Rewind:
                                        buttonCode = kHIDRemoteButtonCodeLeftHold;
                                break;

                                case kHIDUsage_Csmr_FastForward:
                                        buttonCode = kHIDRemoteButtonCodeRightHold;
                                break;

                                case kHIDUsage_Csmr_Menu:
                                        buttonCode = kHIDRemoteButtonCodeMenuHold;
                                break;
                        }
                break;

                case kHIDPage_GenericDesktop:
                        switch (usage)
                        {
                                case kHIDUsage_GD_SystemAppMenu:
                                        buttonCode = kHIDRemoteButtonCodeMenu;
                                break;

                                case kHIDUsage_GD_SystemMenu:
                                        buttonCode = kHIDRemoteButtonCodeCenter;
                                break;

                                case kHIDUsage_GD_SystemMenuRight:
                                        buttonCode = kHIDRemoteButtonCodeRight;
                                break;

                                case kHIDUsage_GD_SystemMenuLeft:
                                        buttonCode = kHIDRemoteButtonCodeLeft;
                                break;

                                case kHIDUsage_GD_SystemMenuUp:
                                        buttonCode = kHIDRemoteButtonCodeUp;
                                break;

                                case kHIDUsage_GD_SystemMenuDown:
                                        buttonCode = kHIDRemoteButtonCodeDown;
                                break;
                        }
                break;

                case 0x06: /* Reserved */
                        switch (usage)
                        {
                                case 0x22:
                                        buttonCode = kHIDRemoteButtonCodeIDChanged;
                                break;
                        }
                break;

                case 0xFF01: /* Vendor specific */
                        switch (usage)
                        {
                                case 0x23:
                                        buttonCode = kHIDRemoteButtonCodeCenterHold;
                                break;

                                #ifdef _HIDREMOTE_EXTENSIONS
                                        #define _HIDREMOTE_EXTENSIONS_SECTION 2
                                        #include "HIDRemoteAdditions.h"
                                        #undef _HIDREMOTE_EXTENSIONS_SECTION
                                #endif /* _HIDREMOTE_EXTENSIONS */
                        }
                break;
        }

        return (buttonCode);
}

- (BOOL)_setupService:(io_object_t)service
{
        kern_return_t            kernResult;
        IOReturn                 returnCode;
        HRESULT                  hResult;
        SInt32                   score;
        BOOL                     opened = NO, queueStarted = NO;
        IOHIDDeviceInterface122  **hidDeviceInterface   = NULL;
        IOCFPlugInInterface      **cfPluginInterface    = NULL;
        IOHIDQueueInterface      **hidQueueInterface    = NULL;
        io_object_t              serviceNotification    = 0;
        CFRunLoopSourceRef       queueEventSource       = NULL;
        NSMutableDictionary      *hidAttribsDict        = nil;
        CFArrayRef               hidElements            = NULL;
        NSError                  *error                 = nil;
        UInt32                   errorCode              = 0;

        if (![self _prematchService:service])
        {
                return (NO);
        }

        do
        {
                // Create a plugin interface ..
                kernResult = IOCreatePlugInInterfaceForService( service,
                                                                kIOHIDDeviceUserClientTypeID,
                                                                kIOCFPlugInInterfaceID,
                                                                &cfPluginInterface,
                                                                &score);

                if (kernResult != kIOReturnSuccess)
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:kernResult userInfo:nil];
                        errorCode = 1;
                        break;
                }


                // .. use it to get the HID interface ..
                hResult = (*cfPluginInterface)->QueryInterface( cfPluginInterface,
                                                                CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID122),
                                                                (LPVOID)&hidDeviceInterface);

                if ((hResult!=S_OK) || (hidDeviceInterface==NULL))
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:hResult userInfo:nil];
                        errorCode = 2;
                        break;
                }


                // .. then open it ..
                switch (_mode)
                {
                        case kHIDRemoteModeShared:
                                hResult = (*hidDeviceInterface)->open(hidDeviceInterface, kIOHIDOptionsTypeNone);
                        break;

                        case kHIDRemoteModeExclusive:
                        case kHIDRemoteModeExclusiveAuto:
                                hResult = (*hidDeviceInterface)->open(hidDeviceInterface, kIOHIDOptionsTypeSeizeDevice);
                        break;

                        default:
                                goto cleanUp; // Ugh! But there are no "double breaks" available in C AFAIK ..
                        break;
                }

                if (hResult!=S_OK)
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:hResult userInfo:nil];
                        errorCode = 3;
                        break;
                }

                opened = YES;

                // .. query the HID elements ..
                returnCode = (*hidDeviceInterface)->copyMatchingElements(hidDeviceInterface,
                                                                         NULL,
                                                                         &hidElements);
                if ((returnCode != kIOReturnSuccess) || (hidElements==NULL))
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:returnCode userInfo:nil];
                        errorCode = 4;

                        break;
                }

                // Setup an event queue for HID events!
                hidQueueInterface = (*hidDeviceInterface)->allocQueue(hidDeviceInterface);
                if (hidQueueInterface == NULL)
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:kIOReturnError userInfo:nil];
                        errorCode = 5;

                        break;
                }

                returnCode = (*hidQueueInterface)->create(hidQueueInterface, 0, 32);
                if (returnCode != kIOReturnSuccess)
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:returnCode userInfo:nil];
                        errorCode = 6;

                        break;
                }


                // Setup of attributes stored for this HID device
                hidAttribsDict = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
                                        [NSValue valueWithPointer:(const void *)cfPluginInterface],     kHIDRemoteCFPluginInterface,
                                        [NSValue valueWithPointer:(const void *)hidDeviceInterface],    kHIDRemoteHIDDeviceInterface,
                                        [NSValue valueWithPointer:(const void *)hidQueueInterface],     kHIDRemoteHIDQueueInterface,
                                 nil];

                {
                        UInt32 i, hidElementCnt = CFArrayGetCount(hidElements);
                        NSMutableDictionary *cookieButtonCodeLUT = [[NSMutableDictionary alloc] init];
                        NSMutableDictionary *cookieCount        = [[NSMutableDictionary alloc] init];

                        if ((cookieButtonCodeLUT==nil) || (cookieCount==nil))
                        {
                                [cookieButtonCodeLUT  release];
                                cookieButtonCodeLUT = nil;

                                [cookieCount    release];
                                cookieCount = nil;

                                error = [NSError errorWithDomain:NSMachErrorDomain code:kIOReturnError userInfo:nil];
                                errorCode = 7;

                                break;
                        }

                        // Analyze the HID elements and find matching elements
                        for (i=0;i<hidElementCnt;i++)
                        {
                                CFDictionaryRef         hidDict;
                                NSNumber                *usage, *usagePage, *cookie;
                                HIDRemoteButtonCode     buttonCode = kHIDRemoteButtonCodeNone;

                                hidDict = CFArrayGetValueAtIndex(hidElements, i);

                                usage     = (NSNumber *) CFDictionaryGetValue(hidDict, CFSTR(kIOHIDElementUsageKey));
                                usagePage = (NSNumber *) CFDictionaryGetValue(hidDict, CFSTR(kIOHIDElementUsagePageKey));
                                cookie    = (NSNumber *) CFDictionaryGetValue(hidDict, CFSTR(kIOHIDElementCookieKey));

                                if ((usage!=nil) && (usagePage!=nil) && (cookie!=nil))
                                {
                                        // Find the button codes for the ID combos
                                        buttonCode = [self buttonCodeForUsage:[usage unsignedIntValue] usagePage:[usagePage unsignedIntValue]];

                                        #ifdef _HIDREMOTE_EXTENSIONS
                                                // Debug logging code
                                                #define _HIDREMOTE_EXTENSIONS_SECTION 3
                                                #include "HIDRemoteAdditions.h"
                                                #undef _HIDREMOTE_EXTENSIONS_SECTION
                                        #endif /* _HIDREMOTE_EXTENSIONS */

                                        // Did record match?
                                        if (buttonCode != kHIDRemoteButtonCodeNone)
                                        {
                                                NSString *pairString        = [[NSString alloc] initWithFormat:@"%u_%u", [usagePage unsignedIntValue], [usage unsignedIntValue]];
                                                NSNumber *buttonCodeNumber  = [[NSNumber alloc] initWithUnsignedInt:(unsigned int)buttonCode];

                                                #ifdef _HIDREMOTE_EXTENSIONS
                                                        // Debug logging code
                                                        #define _HIDREMOTE_EXTENSIONS_SECTION 4
                                                        #include "HIDRemoteAdditions.h"
                                                        #undef _HIDREMOTE_EXTENSIONS_SECTION
                                                #endif /* _HIDREMOTE_EXTENSIONS */

                                                [cookieCount            setObject:buttonCodeNumber forKey:pairString];
                                                [cookieButtonCodeLUT    setObject:buttonCodeNumber forKey:cookie];

                                                (*hidQueueInterface)->addElement(hidQueueInterface,
                                                                                 (IOHIDElementCookie) [cookie unsignedIntValue],
                                                                                 0);

                                                #ifdef _HIDREMOTE_EXTENSIONS
                                                        // Get current Apple Remote ID value
                                                        #define _HIDREMOTE_EXTENSIONS_SECTION 7
                                                        #include "HIDRemoteAdditions.h"
                                                        #undef _HIDREMOTE_EXTENSIONS_SECTION
                                                #endif /* _HIDREMOTE_EXTENSIONS */

                                                [buttonCodeNumber release];
                                                [pairString release];
                                        }
                                }
                        }

                        // Compare number of *unique* matches (thus the cookieCount dictionary) with required minimum
                        if ([cookieCount count] < 10)
                        {
                                [cookieButtonCodeLUT  release];
                                cookieButtonCodeLUT = nil;

                                [cookieCount    release];
                                cookieCount = nil;

                                error = [NSError errorWithDomain:NSMachErrorDomain code:kIOReturnError userInfo:nil];
                                errorCode = 8;

                                break;
                        }

                        [hidAttribsDict setObject:cookieButtonCodeLUT forKey:kHIDRemoteCookieButtonCodeLUT];

                        [cookieButtonCodeLUT  release];
                        cookieButtonCodeLUT = nil;

                        [cookieCount    release];
                        cookieCount = nil;
                }

                // Finish setup of IOHIDQueueInterface with CFRunLoop
                returnCode = (*hidQueueInterface)->createAsyncEventSource(hidQueueInterface, &queueEventSource);
                if ((returnCode != kIOReturnSuccess) || (queueEventSource == NULL))
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:returnCode userInfo:nil];
                        errorCode = 9;
                        break;
                }

                returnCode = (*hidQueueInterface)->setEventCallout(hidQueueInterface, HIDEventCallback, (void *)((intptr_t)service), (void *)self);
                if (returnCode != kIOReturnSuccess)
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:returnCode userInfo:nil];
                        errorCode = 10;
                        break;
                }

                CFRunLoopAddSource(     CFRunLoopGetCurrent(),
                                        queueEventSource,
                                        kCFRunLoopCommonModes);
                [hidAttribsDict setObject:[NSValue valueWithPointer:(const void *)queueEventSource] forKey:kHIDRemoteCFRunLoopSource];

                returnCode = (*hidQueueInterface)->start(hidQueueInterface);
                if (returnCode != kIOReturnSuccess)
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:returnCode userInfo:nil];
                        errorCode = 11;
                        break;
                }

                queueStarted = YES;

                // Setup device notifications
                returnCode = IOServiceAddInterestNotification(  _notifyPort,
                                                                service,
                                                                kIOGeneralInterest,
                                                                ServiceNotificationCallback,
                                                                self,
                                                                &serviceNotification);
                if ((returnCode != kIOReturnSuccess) || (serviceNotification==0))
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:returnCode userInfo:nil];
                        errorCode = 12;
                        break;
                }

                [hidAttribsDict setObject:[NSNumber numberWithUnsignedInt:(unsigned int)serviceNotification] forKey:kHIDRemoteServiceNotification];

                // Retain service
                if (IOObjectRetain(service) != kIOReturnSuccess)
                {
                        error = [NSError errorWithDomain:NSMachErrorDomain code:kIOReturnError userInfo:nil];
                        errorCode = 13;
                        break;
                }

                [hidAttribsDict setObject:[NSNumber numberWithUnsignedInt:(unsigned int)service] forKey:kHIDRemoteService];

                // Get some (somewhat optional) infos on the device
                {
                        CFStringRef product, manufacturer, transport;

                        if ((product = IORegistryEntryCreateCFProperty( (io_registry_entry_t)service,
                                                                        (CFStringRef) @"Product",
                                                                        kCFAllocatorDefault,
                                                                        0)) != NULL)
                        {
                                if (CFGetTypeID(product) == CFStringGetTypeID())
                                {
                                        [hidAttribsDict setObject:(NSString *)product forKey:kHIDRemoteProduct];
                                }

                                CFRelease(product);
                        }

                        if ((manufacturer = IORegistryEntryCreateCFProperty(    (io_registry_entry_t)service,
                                                                                (CFStringRef) @"Manufacturer",
                                                                                kCFAllocatorDefault,
                                                                                0)) != NULL)
                        {
                                if (CFGetTypeID(manufacturer) == CFStringGetTypeID())
                                {
                                        [hidAttribsDict setObject:(NSString *)manufacturer forKey:kHIDRemoteManufacturer];
                                }

                                CFRelease(manufacturer);
                        }

                        if ((transport = IORegistryEntryCreateCFProperty(       (io_registry_entry_t)service,
                                                                                (CFStringRef) @"Transport",
                                                                                kCFAllocatorDefault,
                                                                                0)) != NULL)
                        {
                                if (CFGetTypeID(transport) == CFStringGetTypeID())
                                {
                                        [hidAttribsDict setObject:(NSString *)transport forKey:kHIDRemoteTransport];
                                }

                                CFRelease(transport);
                        }
                }

                // Determine Aluminum Remote support
                {
                        CFNumberRef aluSupport;
                        HIDRemoteAluminumRemoteSupportLevel supportLevel = kHIDRemoteAluminumRemoteSupportLevelNone;

                        if ((_mode == kHIDRemoteModeExclusive) || (_mode == kHIDRemoteModeExclusiveAuto))
                        {
                                // Determine if this driver offers on-demand support for the Aluminum Remote (only relevant under OS versions < 10.6.2)
                                if ((aluSupport = IORegistryEntryCreateCFProperty((io_registry_entry_t)service,
                                                                                  (CFStringRef) @"AluminumRemoteSupportLevelOnDemand",
                                                                                  kCFAllocatorDefault,
                                                                                  0)) != nil)
                                {
                                        // There is => request the driver to enable it for us
                                        if (IORegistryEntrySetCFProperty((io_registry_entry_t)service,
                                                                         CFSTR("EnableAluminumRemoteSupportForMe"),
                                                                         [NSDictionary dictionaryWithObjectsAndKeys:
                                                                                [NSNumber numberWithLongLong:(long long)getpid()],      @"pid",
                                                                                [NSNumber numberWithLongLong:(long long)getuid()],      @"uid",
                                                                         nil]) == kIOReturnSuccess)
                                        {
                                                if (CFGetTypeID(aluSupport) == CFNumberGetTypeID())
                                                {
                                                        supportLevel = (HIDRemoteAluminumRemoteSupportLevel) [(NSNumber *)aluSupport intValue];
                                                }

                                                [hidAttribsDict setObject:[NSNumber numberWithBool:YES] forKey:kHIDRemoteAluminumRemoteSupportOnDemand];
                                        }

                                        CFRelease(aluSupport);
                                }
                        }

                        if (supportLevel == kHIDRemoteAluminumRemoteSupportLevelNone)
                        {
                                if ((aluSupport = IORegistryEntryCreateCFProperty((io_registry_entry_t)service,
                                                                                  (CFStringRef) @"AluminumRemoteSupportLevel",
                                                                                  kCFAllocatorDefault,
                                                                                  0)) != nil)
                                {
                                        if (CFGetTypeID(aluSupport) == CFNumberGetTypeID())
                                        {
                                                supportLevel = (HIDRemoteAluminumRemoteSupportLevel) [(NSNumber *)aluSupport intValue];
                                        }

                                        CFRelease(aluSupport);
                                }
                                else
                                {
                                        CFStringRef ioKitClassName;

                                        if ((ioKitClassName = IORegistryEntryCreateCFProperty(  (io_registry_entry_t)service,
                                                                                                CFSTR(kIOClassKey),
                                                                                                kCFAllocatorDefault,
                                                                                                0)) != nil)
                                        {
                                                if ([(NSString *)ioKitClassName isEqual:@"AppleIRController"])
                                                {
                                                        supportLevel = kHIDRemoteAluminumRemoteSupportLevelNative;
                                                }

                                                CFRelease(ioKitClassName);
                                        }
                                }
                        }

                        [hidAttribsDict setObject:(NSNumber *)[NSNumber numberWithInt:(int)supportLevel] forKey:kHIDRemoteAluminumRemoteSupportLevel];
                }

                // Add it to the serviceAttribMap
                [_serviceAttribMap setObject:hidAttribsDict forKey:[NSNumber numberWithUnsignedInt:(unsigned int)service]];

                // And we're done with setup ..
                if (([self delegate]!=nil) &&
                    ([[self delegate] respondsToSelector:@selector(hidRemote:foundNewHardwareWithAttributes:)]))
                {
                        [((NSObject <HIDRemoteDelegate> *)[self delegate]) hidRemote:self foundNewHardwareWithAttributes:hidAttribsDict];
                }

                [hidAttribsDict release];
                hidAttribsDict = nil;

                return(YES);

        }while(0);

        cleanUp:

        if (([self delegate]!=nil) &&
            ([[self delegate] respondsToSelector:@selector(hidRemote:failedNewHardwareWithError:)]))
        {
                if (error!=nil)
                {
                        error = [NSError errorWithDomain:[error domain]
                                                    code:[error code]
                                                userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:errorCode] forKey:@"InternalErrorCode"]
                                ];
                }

                [((NSObject <HIDRemoteDelegate> *)[self delegate]) hidRemote:self failedNewHardwareWithError:error];
        }

        // An error occured or this device is not of interest .. cleanup ..
        if (serviceNotification!=0)
        {
                IOObjectRelease(serviceNotification);
                serviceNotification = 0;
        }

        if (queueEventSource!=NULL)
        {
                CFRunLoopSourceInvalidate(queueEventSource);
                queueEventSource=NULL;
        }

        if (hidQueueInterface!=NULL)
        {
                if (queueStarted)
                {
                        (*hidQueueInterface)->stop(hidQueueInterface);
                }
                (*hidQueueInterface)->dispose(hidQueueInterface);
                (*hidQueueInterface)->Release(hidQueueInterface);
                hidQueueInterface = NULL;
        }

        if (hidAttribsDict!=nil)
        {
                [hidAttribsDict release];
                hidAttribsDict = nil;
        }

        if (hidElements!=NULL)
        {
                CFRelease(hidElements);
                hidElements = NULL;
        }

        if (hidDeviceInterface!=NULL)
        {
                if (opened)
                {
                        (*hidDeviceInterface)->close(hidDeviceInterface);
                }
                (*hidDeviceInterface)->Release(hidDeviceInterface);
                // opened = NO;
                hidDeviceInterface = NULL;
        }

        if (cfPluginInterface!=NULL)
        {
                IODestroyPlugInInterface(cfPluginInterface);
                cfPluginInterface = NULL;
        }

        return (NO);
}

- (void)_destructService:(io_object_t)service
{
        NSNumber            *serviceValue;
        NSMutableDictionary *serviceDict = NULL;

        if ((serviceValue = [NSNumber numberWithUnsignedInt:(unsigned int)service]) == nil)
        {
                return;
        }

        serviceDict  = [_serviceAttribMap objectForKey:serviceValue];

        if (serviceDict!=nil)
        {
                IOHIDDeviceInterface122  **hidDeviceInterface   = NULL;
                IOCFPlugInInterface      **cfPluginInterface    = NULL;
                IOHIDQueueInterface      **hidQueueInterface    = NULL;
                io_object_t              serviceNotification    = 0;
                CFRunLoopSourceRef       queueEventSource       = NULL;
                io_object_t              theService             = 0;
                NSMutableDictionary      *cookieButtonMap       = nil;
                NSTimer                  *simulateHoldTimer     = nil;

                serviceNotification = (io_object_t)                     ([serviceDict objectForKey:kHIDRemoteServiceNotification]       ? [[serviceDict objectForKey:kHIDRemoteServiceNotification] unsignedIntValue] :   0);
                theService          = (io_object_t)                     ([serviceDict objectForKey:kHIDRemoteService]                   ? [[serviceDict objectForKey:kHIDRemoteService]             unsignedIntValue] :   0);
                queueEventSource    = (CFRunLoopSourceRef)              ([serviceDict objectForKey:kHIDRemoteCFRunLoopSource]           ? [[serviceDict objectForKey:kHIDRemoteCFRunLoopSource]     pointerValue]     : NULL);
                hidQueueInterface   = (IOHIDQueueInterface **)          ([serviceDict objectForKey:kHIDRemoteHIDQueueInterface]         ? [[serviceDict objectForKey:kHIDRemoteHIDQueueInterface]   pointerValue]     : NULL);
                hidDeviceInterface  = (IOHIDDeviceInterface122 **)      ([serviceDict objectForKey:kHIDRemoteHIDDeviceInterface]        ? [[serviceDict objectForKey:kHIDRemoteHIDDeviceInterface]  pointerValue]     : NULL);
                cfPluginInterface   = (IOCFPlugInInterface **)          ([serviceDict objectForKey:kHIDRemoteCFPluginInterface]         ? [[serviceDict objectForKey:kHIDRemoteCFPluginInterface]   pointerValue]     : NULL);
                cookieButtonMap     = (NSMutableDictionary *)            [serviceDict objectForKey:kHIDRemoteCookieButtonCodeLUT];
                simulateHoldTimer   = (NSTimer *)                        [serviceDict objectForKey:kHIDRemoteSimulateHoldEventsTimer];

                [serviceDict  retain];
                [_serviceAttribMap removeObjectForKey:serviceValue];

                if (([serviceDict objectForKey:kHIDRemoteAluminumRemoteSupportOnDemand]!=nil) && [[serviceDict objectForKey:kHIDRemoteAluminumRemoteSupportOnDemand] boolValue] && (theService != 0))
                {
                        // We previously requested the driver to enable Aluminum Remote support for us. Tell it to turn it off again - now that we no longer need it
                        IORegistryEntrySetCFProperty(   (io_registry_entry_t)theService,
                                                        CFSTR("DisableAluminumRemoteSupportForMe"),
                                                        [NSDictionary dictionaryWithObjectsAndKeys:
                                                                [NSNumber numberWithLongLong:(long long)getpid()],      @"pid",
                                                                [NSNumber numberWithLongLong:(long long)getuid()],      @"uid",
                                                        nil]);
                }

                if (([self delegate]!=nil) &&
                    ([[self delegate] respondsToSelector:@selector(hidRemote:releasedHardwareWithAttributes:)]))
                {
                        [((NSObject <HIDRemoteDelegate> *)[self delegate]) hidRemote:self releasedHardwareWithAttributes:serviceDict];
                }

                if (simulateHoldTimer!=nil)
                {
                        [simulateHoldTimer invalidate];
                }

                if (serviceNotification!=0)
                {
                        IOObjectRelease(serviceNotification);
                }

                if (queueEventSource!=NULL)
                {
                        CFRunLoopRemoveSource(  CFRunLoopGetCurrent(),
                                                queueEventSource,
                                                kCFRunLoopCommonModes);
                }

                if ((hidQueueInterface!=NULL) && (cookieButtonMap!=nil))
                {
                        NSEnumerator *cookieEnum = [cookieButtonMap keyEnumerator];
                        NSNumber *cookie;

                        while ((cookie = [cookieEnum nextObject]) != nil)
                        {
                                if ((*hidQueueInterface)->hasElement(hidQueueInterface, (IOHIDElementCookie) [cookie unsignedIntValue]))
                                {
                                        (*hidQueueInterface)->removeElement(hidQueueInterface,
                                                                            (IOHIDElementCookie) [cookie unsignedIntValue]);
                                }
                        };
                }

                if (hidQueueInterface!=NULL)
                {
                        (*hidQueueInterface)->stop(hidQueueInterface);
                        (*hidQueueInterface)->dispose(hidQueueInterface);
                        (*hidQueueInterface)->Release(hidQueueInterface);
                }

                if (hidDeviceInterface!=NULL)
                {
                        (*hidDeviceInterface)->close(hidDeviceInterface);
                        (*hidDeviceInterface)->Release(hidDeviceInterface);
                }

                if (cfPluginInterface!=NULL)
                {
                        IODestroyPlugInInterface(cfPluginInterface);
                }

                if (theService!=0)
                {
                        IOObjectRelease(theService);
                }

                [serviceDict release];
        }
}


#pragma mark -- PRIVATE: HID Event handling --
- (void)_simulateHoldEvent:(NSTimer *)aTimer
{
        NSMutableDictionary *hidAttribsDict;
        NSTimer  *shTimer;
        NSNumber *shButtonCode;

        if ((hidAttribsDict = (NSMutableDictionary *)[aTimer userInfo]) != nil)
        {
                if (((shTimer      = [hidAttribsDict objectForKey:kHIDRemoteSimulateHoldEventsTimer]) != nil) &&
                    ((shButtonCode = [hidAttribsDict objectForKey:kHIDRemoteSimulateHoldEventsOriginButtonCode]) != nil))
                {
                        [shTimer invalidate];
                        [hidAttribsDict removeObjectForKey:kHIDRemoteSimulateHoldEventsTimer];

                        [self _sendButtonCode:(((HIDRemoteButtonCode)[shButtonCode unsignedIntValue])|kHIDRemoteButtonCodeHoldMask) isPressed:YES hidAttribsDict:hidAttribsDict];
                }
        }
}

- (void)_handleButtonCode:(HIDRemoteButtonCode)buttonCode isPressed:(BOOL)isPressed hidAttribsDict:(NSMutableDictionary *)hidAttribsDict
{
        switch (buttonCode)
        {
                case kHIDRemoteButtonCodeIDChanged:
                        // Do nothing, this is handled seperately
                break;

                case kHIDRemoteButtonCodeUp:
                case kHIDRemoteButtonCodeDown:
                        if (_simulateHoldEvents)
                        {
                                NSTimer  *shTimer = nil;
                                NSNumber *shButtonCode = nil;

                                [[hidAttribsDict objectForKey:kHIDRemoteSimulateHoldEventsTimer] invalidate];

                                if (isPressed)
                                {
                                        [hidAttribsDict setObject:[NSNumber numberWithUnsignedInt:buttonCode] forKey:kHIDRemoteSimulateHoldEventsOriginButtonCode];

                                        if ((shTimer = [[NSTimer alloc] initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:0.7] interval:0.1 target:self selector:@selector(_simulateHoldEvent:) userInfo:hidAttribsDict repeats:NO]) != nil)
                                        {
                                                [hidAttribsDict setObject:shTimer forKey:kHIDRemoteSimulateHoldEventsTimer];

                                                // Using CFRunLoopAddTimer instead of [[NSRunLoop currentRunLoop] addTimer:.. for consistency with run loop modes.
                                                // The kCFRunLoopCommonModes counterpart NSRunLoopCommonModes is only available in 10.5 and later, whereas this code
                                                // is designed to be also compatible with 10.4. CFRunLoopTimerRef is "toll-free-bridged" with NSTimer since 10.0.
                                                CFRunLoopAddTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)shTimer, kCFRunLoopCommonModes);

                                                [shTimer release];

                                                break;
                                        }
                                }
                                else
                                {
                                        shTimer      = [hidAttribsDict objectForKey:kHIDRemoteSimulateHoldEventsTimer];
                                        shButtonCode = [hidAttribsDict objectForKey:kHIDRemoteSimulateHoldEventsOriginButtonCode];

                                        if ((shTimer!=nil) && (shButtonCode!=nil))
                                        {
                                                [self _sendButtonCode:(HIDRemoteButtonCode)[shButtonCode unsignedIntValue] isPressed:YES hidAttribsDict:hidAttribsDict];
                                                [self _sendButtonCode:(HIDRemoteButtonCode)[shButtonCode unsignedIntValue] isPressed:NO hidAttribsDict:hidAttribsDict];
                                        }
                                        else
                                        {
                                                if (shButtonCode!=nil)
                                                {
                                                        [self _sendButtonCode:(((HIDRemoteButtonCode)[shButtonCode unsignedIntValue])|kHIDRemoteButtonCodeHoldMask) isPressed:NO hidAttribsDict:hidAttribsDict];
                                                }
                                        }
                                }

                                [hidAttribsDict removeObjectForKey:kHIDRemoteSimulateHoldEventsTimer];
                                [hidAttribsDict removeObjectForKey:kHIDRemoteSimulateHoldEventsOriginButtonCode];

                                break;
                        }

                default:
                        [self _sendButtonCode:buttonCode isPressed:isPressed hidAttribsDict:hidAttribsDict];
                break;
        }
}

- (void)_sendButtonCode:(HIDRemoteButtonCode)buttonCode isPressed:(BOOL)isPressed hidAttribsDict:(NSMutableDictionary *)hidAttribsDict
{
        if (([self delegate]!=nil) &&
            ([[self delegate] respondsToSelector:@selector(hidRemote:eventWithButton:isPressed:fromHardwareWithAttributes:)]))
        {
                switch (buttonCode & (~kHIDRemoteButtonCodeAluminumMask))
                {
                        case kHIDRemoteButtonCodePlay:
                        case kHIDRemoteButtonCodeCenter:
                                if (buttonCode & kHIDRemoteButtonCodeAluminumMask)
                                {
                                        _lastSeenModel         = kHIDRemoteModelAluminum;
                                        _lastSeenModelRemoteID = _lastSeenRemoteID;
                                }
                                else
                                {
                                        switch ((HIDRemoteAluminumRemoteSupportLevel)[[hidAttribsDict objectForKey:kHIDRemoteAluminumRemoteSupportLevel] intValue])
                                        {
                                                case kHIDRemoteAluminumRemoteSupportLevelNone:
                                                case kHIDRemoteAluminumRemoteSupportLevelEmulation:
                                                        // Remote type can't be determined by just the Center button press
                                                break;

                                                case kHIDRemoteAluminumRemoteSupportLevelNative:
                                                        // Remote type can be safely determined by just the Center button press
                                                        if (((_lastSeenModel == kHIDRemoteModelAluminum) && (_lastSeenModelRemoteID != _lastSeenRemoteID)) ||
                                                             (_lastSeenModel == kHIDRemoteModelUndetermined))
                                                        {
                                                                _lastSeenModel = kHIDRemoteModelWhitePlastic;
                                                        }
                                                break;
                                        }
                                }
                        break;
                }

                // As soon as we have received a code that's unique to the Aluminum Remote, we can tell kHIDRemoteButtonCodePlayHold and kHIDRemoteButtonCodeCenterHold apart.
                // Prior to that, a long press of the new "Play" button will be submitted as a "kHIDRemoteButtonCodeCenterHold", not a "kHIDRemoteButtonCodePlayHold" code.
                if ((buttonCode == kHIDRemoteButtonCodeCenterHold) && (_lastSeenModel == kHIDRemoteModelAluminum))
                {
                        buttonCode = kHIDRemoteButtonCodePlayHold;
                }

                [((NSObject <HIDRemoteDelegate> *)[self delegate]) hidRemote:self eventWithButton:(buttonCode & (~kHIDRemoteButtonCodeAluminumMask)) isPressed:isPressed fromHardwareWithAttributes:hidAttribsDict];
        }
}

- (void)_hidEventFor:(io_service_t)hidDevice from:(IOHIDQueueInterface **)interface withResult:(IOReturn)result
{
        NSMutableDictionary *hidAttribsDict = [[[_serviceAttribMap objectForKey:[NSNumber numberWithUnsignedInt:(unsigned int)hidDevice]] retain] autorelease];

        if (hidAttribsDict!=nil)
        {
                IOHIDQueueInterface **queueInterface  = NULL;

                queueInterface  = [[hidAttribsDict objectForKey:kHIDRemoteHIDQueueInterface] pointerValue];

                if (interface == queueInterface)
                {
                        NSNumber            *lastButtonPressedNumber = nil;
                        HIDRemoteButtonCode  lastButtonPressed = kHIDRemoteButtonCodeNone;
                        NSMutableDictionary *cookieButtonMap = nil;

                        cookieButtonMap  = [hidAttribsDict objectForKey:kHIDRemoteCookieButtonCodeLUT];

                        if ((lastButtonPressedNumber = [hidAttribsDict objectForKey:kHIDRemoteLastButtonPressed]) != nil)
                        {
                                lastButtonPressed = [lastButtonPressedNumber unsignedIntValue];
                        }

                        while (result == kIOReturnSuccess)
                        {
                                IOHIDEventStruct hidEvent;
                                AbsoluteTime supportedTime = { 0,0 };

                                result = (*queueInterface)->getNextEvent(       queueInterface,
                                                                                &hidEvent,
                                                                                supportedTime,
                                                                                0);

                                if (result == kIOReturnSuccess)
                                {
                                        NSNumber *buttonCodeNumber = [cookieButtonMap objectForKey:[NSNumber numberWithUnsignedInt:(unsigned int) hidEvent.elementCookie]];

                                        #ifdef _HIDREMOTE_EXTENSIONS
                                                // Debug logging code
                                                #define _HIDREMOTE_EXTENSIONS_SECTION 5
                                                #include "HIDRemoteAdditions.h"
                                                #undef _HIDREMOTE_EXTENSIONS_SECTION
                                        #endif /* _HIDREMOTE_EXTENSIONS */

                                        if (buttonCodeNumber!=nil)
                                        {
                                                HIDRemoteButtonCode buttonCode = [buttonCodeNumber unsignedIntValue];

                                                if (hidEvent.value == 0)
                                                {
                                                        if (buttonCode == lastButtonPressed)
                                                        {
                                                                [self _handleButtonCode:lastButtonPressed isPressed:NO hidAttribsDict:hidAttribsDict];
                                                                lastButtonPressed = kHIDRemoteButtonCodeNone;
                                                        }
                                                }

                                                if (hidEvent.value != 0)
                                                {
                                                        if (lastButtonPressed != kHIDRemoteButtonCodeNone)
                                                        {
                                                                [self _handleButtonCode:lastButtonPressed isPressed:NO hidAttribsDict:hidAttribsDict];
                                                                // lastButtonPressed = kHIDRemoteButtonCodeNone;
                                                        }

                                                        if (buttonCode == kHIDRemoteButtonCodeIDChanged)
                                                        {
                                                                if (([self delegate]!=nil) &&
                                                                    ([[self delegate] respondsToSelector:@selector(hidRemote:remoteIDChangedOldID:newID:forHardwareWithAttributes:)]))
                                                                {
                                                                        [((NSObject <HIDRemoteDelegate> *)[self delegate]) hidRemote:self remoteIDChangedOldID:_lastSeenRemoteID newID:hidEvent.value forHardwareWithAttributes:hidAttribsDict];
                                                                }

                                                                _lastSeenRemoteID = hidEvent.value;
                                                                _lastSeenModel    = kHIDRemoteModelUndetermined;
                                                        }

                                                        [self _handleButtonCode:buttonCode isPressed:YES hidAttribsDict:hidAttribsDict];
                                                        lastButtonPressed = buttonCode;
                                                }
                                        }
                                }
                        };

                        [hidAttribsDict setObject:[NSNumber numberWithUnsignedInt:lastButtonPressed] forKey:kHIDRemoteLastButtonPressed];
                }

                #ifdef _HIDREMOTE_EXTENSIONS
                        // Debug logging code
                        #define _HIDREMOTE_EXTENSIONS_SECTION 6
                        #include "HIDRemoteAdditions.h"
                        #undef _HIDREMOTE_EXTENSIONS_SECTION
                #endif /* _HIDREMOTE_EXTENSIONS */
        }
}

#pragma mark -- PRIVATE: Notification handling --
- (void)_serviceMatching:(io_iterator_t)iterator
{
        io_object_t matchingService = 0;

        while ((matchingService = IOIteratorNext(iterator)) != 0)
        {
                [self _setupService:matchingService];

                IOObjectRelease(matchingService);
        };
}

- (void)_serviceNotificationFor:(io_service_t)service messageType:(natural_t)messageType messageArgument:(void *)messageArgument
{
        if (messageType == kIOMessageServiceIsTerminated)
        {
                [self _destructService:service];
        }
}

- (void)_updateSessionInformation
{
        NSArray *consoleUsersArray;
        io_service_t rootService;

        if (_masterPort==0) { return; }

        if ((rootService = IORegistryGetRootEntry(_masterPort)) != 0)
        {
                if ((consoleUsersArray = (NSArray *)IORegistryEntryCreateCFProperty((io_registry_entry_t)rootService, CFSTR("IOConsoleUsers"), kCFAllocatorDefault, 0)) != nil)
                {
                        if ([consoleUsersArray isKindOfClass:[NSArray class]])  // Be careful - ensure this really is an array
                        {
                                NSEnumerator *consoleUsersEnum; // I *love* Obj-C2's fast enumerators, but we need to stay compatible with 10.4 :-/

                                if ((consoleUsersEnum = [consoleUsersArray objectEnumerator]) != nil)
                                {
                                        UInt64 secureEventInputPIDSum = 0;
                                        uid_t frontUserSession = 0;
                                        NSDictionary *consoleUserDict;

                                        while ((consoleUserDict = [consoleUsersEnum nextObject]) != nil)
                                        {
                                                if ([consoleUserDict isKindOfClass:[NSDictionary class]]) // Be careful - ensure this really is a dictionary
                                                {
                                                        NSNumber *secureInputPID;
                                                        NSNumber *onConsole;
                                                        NSNumber *userID;

                                                        if ((secureInputPID = [consoleUserDict objectForKey:@"kCGSSessionSecureInputPID"]) != nil)
                                                        {
                                                                if ([secureInputPID isKindOfClass:[NSNumber class]])
                                                                {
                                                                        secureEventInputPIDSum += ((UInt64) [secureInputPID intValue]);
                                                                }
                                                        }

                                                        if (((onConsole = [consoleUserDict objectForKey:@"kCGSSessionOnConsoleKey"]) != nil) &&
                                                            ((userID    = [consoleUserDict objectForKey:@"kCGSSessionUserIDKey"]) != nil))
                                                        {
                                                                if ([onConsole isKindOfClass:[NSNumber class]] && [userID isKindOfClass:[NSNumber class]])
                                                                {
                                                                        if ([onConsole boolValue])
                                                                        {
                                                                                frontUserSession = (uid_t) [userID intValue];
                                                                        }
                                                                }
                                                        }
                                                }
                                        }

                                        _lastSecureEventInputPIDSum = secureEventInputPIDSum;
                                        _lastFrontUserSession       = frontUserSession;
                                }
                        }

                        CFRelease((CFTypeRef)consoleUsersArray);
                }

                IOObjectRelease((io_object_t) rootService);
        }
}

- (void)_secureInputNotificationFor:(io_service_t)service messageType:(natural_t)messageType messageArgument:(void *)messageArgument
{
        if (messageType == kIOMessageServiceBusyStateChange)
        {
                UInt64 old_lastSecureEventInputPIDSum = _lastSecureEventInputPIDSum;
                uid_t  old_lastFrontUserSession = _lastFrontUserSession;

                [self _updateSessionInformation];

                if (((old_lastSecureEventInputPIDSum != _lastSecureEventInputPIDSum) || (old_lastFrontUserSession != _lastFrontUserSession)) && _secureEventInputWorkAround)
                {
                        if ((_mode == kHIDRemoteModeExclusive) || (_mode == kHIDRemoteModeExclusiveAuto))
                        {
                                HIDRemoteMode restartInMode = _mode;

                                _isRestarting = YES;
                                [self stopRemoteControl];
                                [self startRemoteControl:restartInMode];
                                _isRestarting = NO;
                        }
                }
        }
}

@end

#pragma mark -- PRIVATE: IOKitLib Callbacks --

static void HIDEventCallback(   void * target,
                                IOReturn result,
                                void * refCon,
                                void * sender)
{
        HIDRemote               *hidRemote = (HIDRemote *)refCon;
        NSAutoreleasePool       *pool      = [[NSAutoreleasePool alloc] init];

        [hidRemote _hidEventFor:(io_service_t)((intptr_t)target) from:(IOHIDQueueInterface**)sender withResult:(IOReturn)result];

        [pool release];
}


static void ServiceMatchingCallback(    void *refCon,
                                        io_iterator_t iterator)
{
        HIDRemote               *hidRemote = (HIDRemote *)refCon;
        NSAutoreleasePool       *pool      = [[NSAutoreleasePool alloc] init];

        [hidRemote _serviceMatching:iterator];

        [pool release];
}

static void ServiceNotificationCallback(void *          refCon,
                                        io_service_t    service,
                                        natural_t       messageType,
                                        void *          messageArgument)
{
        HIDRemote               *hidRemote = (HIDRemote *)refCon;
        NSAutoreleasePool       *pool     = [[NSAutoreleasePool alloc] init];

        [hidRemote _serviceNotificationFor:service
                               messageType:messageType
                           messageArgument:messageArgument];

        [pool release];
}

static void SecureInputNotificationCallback(    void *          refCon,
                                                io_service_t    service,
                                                natural_t       messageType,
                                                void *          messageArgument)
{
        HIDRemote               *hidRemote = (HIDRemote *)refCon;
        NSAutoreleasePool       *pool     = [[NSAutoreleasePool alloc] init];

        [hidRemote _secureInputNotificationFor:service
                                   messageType:messageType
                               messageArgument:messageArgument];

        [pool release];
}

// Attribute dictionary keys
NSString *kHIDRemoteCFPluginInterface                   = @"CFPluginInterface";
NSString *kHIDRemoteHIDDeviceInterface                  = @"HIDDeviceInterface";
NSString *kHIDRemoteCookieButtonCodeLUT                 = @"CookieButtonCodeLUT";
NSString *kHIDRemoteHIDQueueInterface                   = @"HIDQueueInterface";
NSString *kHIDRemoteServiceNotification                 = @"ServiceNotification";
NSString *kHIDRemoteCFRunLoopSource                     = @"CFRunLoopSource";
NSString *kHIDRemoteLastButtonPressed                   = @"LastButtonPressed";
NSString *kHIDRemoteService                             = @"Service";
NSString *kHIDRemoteSimulateHoldEventsTimer             = @"SimulateHoldEventsTimer";
NSString *kHIDRemoteSimulateHoldEventsOriginButtonCode  = @"SimulateHoldEventsOriginButtonCode";
NSString *kHIDRemoteAluminumRemoteSupportLevel          = @"AluminumRemoteSupportLevel";
NSString *kHIDRemoteAluminumRemoteSupportOnDemand       = @"AluminumRemoteSupportLevelOnDemand";

NSString *kHIDRemoteManufacturer                        = @"Manufacturer";
NSString *kHIDRemoteProduct                             = @"Product";
NSString *kHIDRemoteTransport                           = @"Transport";

// Distributed notifications
NSString *kHIDRemoteDNHIDRemotePing                     = @"com.candelair.ping";
NSString *kHIDRemoteDNHIDRemoteRetry                    = @"com.candelair.retry";
NSString *kHIDRemoteDNHIDRemoteStatus                   = @"com.candelair.status";

NSString *kHIDRemoteDNHIDRemoteRetryGlobalObject        = @"global";

// Distributed notifications userInfo keys and values
NSString *kHIDRemoteDNStatusHIDRemoteVersionKey         = @"HIDRemoteVersion";
NSString *kHIDRemoteDNStatusPIDKey                      = @"PID";
NSString *kHIDRemoteDNStatusModeKey                     = @"Mode";
NSString *kHIDRemoteDNStatusUnusedButtonCodesKey        = @"UnusedButtonCodes";
NSString *kHIDRemoteDNStatusActionKey                   = @"Action";
NSString *kHIDRemoteDNStatusRemoteControlCountKey       = @"RemoteControlCount";
NSString *kHIDRemoteDNStatusReturnToPIDKey              = @"ReturnToPID";
NSString *kHIDRemoteDNStatusActionStart                 = @"start";
NSString *kHIDRemoteDNStatusActionStop                  = @"stop";
NSString *kHIDRemoteDNStatusActionUpdate                = @"update";
NSString *kHIDRemoteDNStatusActionNoNeed                = @"noneed";
