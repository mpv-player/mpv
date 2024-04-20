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

import Cocoa
import IOKit.pwr_mgt

class Common: NSObject {
    var option: OptionHelper
    var input: InputHelper?
    var log: LogHelper
    var vo: UnsafeMutablePointer<vo>?
    let queue: DispatchQueue = DispatchQueue(label: "io.mpv.queue")

    @objc var window: Window?
    var view: View?
    var titleBar: TitleBar?

    var link: CVDisplayLink?

    let eventsLock = NSLock()
    var events: Int = 0

    var lightSensor: io_connect_t = 0
    var lastLmu: UInt64 = 0
    var lightSensorIOPort: IONotificationPortRef?

    var displaySleepAssertion: IOPMAssertionID = IOPMAssertionID(0)

    var appNotificationObservers: [NSObjectProtocol] = []

    var cursorVisibilityWanted: Bool = true

    var title: String = "mpv" {
        didSet { if let window = window { window.title = title } }
    }

    init(_ option: OptionHelper, _ log: LogHelper) {
        self.option = option
        self.log = log
    }

    func initMisc(_ vo: UnsafeMutablePointer<vo>) {
        startDisplayLink(vo)
        initLightSensor()
        addDisplayReconfigureObserver()
        addAppNotifications()
        option.setMacOptionCallback(macOptsWakeupCallback, context: self)
    }

    func initApp() {
        var policy: NSApplication.ActivationPolicy = .regular
        switch option.mac.macos_app_activation_policy {
        case 0: policy = .regular
        case 1: policy = .accessory
        case 2: policy = .prohibited
        default: break
        }

        NSApp.setActivationPolicy(policy)
        setAppIcon()
    }

    func initWindow(_ vo: UnsafeMutablePointer<vo>, _ previousActiveApp: NSRunningApplication?) {
        let (targetScreen, wr) = getInitProperties(vo)

        guard let view = self.view else {
            log.error("Something went wrong, no View was initialized")
            exit(1)
        }

        window = Window(contentRect: wr, screen: targetScreen, view: view, common: self)
        guard let window = self.window else {
            log.error("Something went wrong, no Window was initialized")
            exit(1)
        }

        window.setOnTop(Bool(option.vo.ontop), Int(option.vo.ontop_level))
        window.setOnAllWorkspaces(Bool(option.vo.all_workspaces))
        window.keepAspect = Bool(option.vo.keepaspect_window)
        window.title = title
        window.border = Bool(option.vo.border)

        titleBar = TitleBar(frame: wr, window: window, common: self)

        let maximized = Bool(option.vo.window_maximized)
        let minimized = Bool(option.vo.window_minimized)
        window.isRestorable = false
        window.isReleasedWhenClosed = false
        window.setMaximized((minimized || !maximized) ? window.isZoomed : maximized)
        window.setMinimized(minimized)
        window.makeMain()
        window.makeKey()

        view.layer?.contentsScale = window.backingScaleFactor

        if !minimized {
            window.orderFront(nil)
        }

        NSApp.activate(ignoringOtherApps: option.vo.focus_on >= 1)

        // workaround for macOS 10.15 to refocus the previous App
        if option.vo.focus_on == 0 {
            previousActiveApp?.activate()
        }
    }

    func initView(_ vo: UnsafeMutablePointer<vo>, _ layer: CALayer) {
        let (_, wr) = getInitProperties(vo)

        view = View(frame: wr, common: self)
        guard let view = self.view else {
            log.error("Something went wrong, no View was initialized")
            exit(1)
        }

        view.layer = layer
        view.wantsLayer = true
        view.layerContentsPlacement = .scaleProportionallyToFit
        layer.delegate = view
    }

    func initWindowState() {
        if option.vo.fullscreen {
            DispatchQueue.main.async {
                self.window?.toggleFullScreen(nil)
            }
        } else {
            window?.isMovableByWindowBackground = true
        }
    }

    func uninitCommon() {
        setCursorVisibility(true)
        stopDisplaylink()
        uninitLightSensor()
        removeDisplayReconfigureObserver()
        removeAppNotifications()
        enableDisplaySleep()
        window?.orderOut(nil)

        titleBar?.removeFromSuperview()
        view?.removeFromSuperview()
    }

    func displayLinkCallback(_ displayLink: CVDisplayLink,
                                   _ inNow: UnsafePointer<CVTimeStamp>,
                            _ inOutputTime: UnsafePointer<CVTimeStamp>,
                                 _ flagsIn: CVOptionFlags,
                                _ flagsOut: UnsafeMutablePointer<CVOptionFlags>) -> CVReturn
    {
        return kCVReturnSuccess
    }

    func startDisplayLink(_ vo: UnsafeMutablePointer<vo>) {
        CVDisplayLinkCreateWithActiveCGDisplays(&link)

        guard let screen = getTargetScreen(forFullscreen: false) ?? NSScreen.main,
              let link = self.link else
        {
            log.warning("Couldn't start DisplayLink, no Screen or DisplayLink available")
            return
        }

        CVDisplayLinkSetCurrentCGDisplay(link, screen.displayID)
        CVDisplayLinkSetOutputHandler(link) { link, now, out, inFlags, outFlags -> CVReturn in
            return self.displayLinkCallback(link, now, out, inFlags, outFlags)
        }
        CVDisplayLinkStart(link)
    }

    func stopDisplaylink() {
        if let link = self.link, CVDisplayLinkIsRunning(link) {
            CVDisplayLinkStop(link)
        }
    }

    func updateDisplaylink() {
        guard let screen = window?.screen, let link = self.link else {
            log.warning("Couldn't update DisplayLink, no Screen or DisplayLink available")
            return
        }

        CVDisplayLinkSetCurrentCGDisplay(link, screen.displayID)
        queue.asyncAfter(deadline: DispatchTime.now() + 0.1) {
            self.flagEvents(VO_EVENT_WIN_STATE)
        }
    }

    func currentFps() -> Double {
        if let link = self.link {
            var actualFps = CVDisplayLinkGetActualOutputVideoRefreshPeriod(link)
            let nominalData = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link)

            if (nominalData.flags & Int32(CVTimeFlags.isIndefinite.rawValue)) < 1 {
                let nominalFps = Double(nominalData.timeScale) / Double(nominalData.timeValue)

                if actualFps > 0 {
                    actualFps = 1/actualFps
                }

                if fabs(actualFps - nominalFps) > 0.1 {
                    log.verbose("Falling back to nominal display refresh rate: \(nominalFps)")
                    return nominalFps
                } else {
                    return actualFps
                }
            }
        } else {
            log.warning("No DisplayLink available")
        }

        log.warning("Falling back to standard display refresh rate: 60Hz")
        return 60.0
    }

    func enableDisplaySleep() {
        IOPMAssertionRelease(displaySleepAssertion)
        displaySleepAssertion = IOPMAssertionID(0)
    }

    func disableDisplaySleep() {
        if displaySleepAssertion != IOPMAssertionID(0) { return }
        IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleDisplaySleep as CFString,
            IOPMAssertionLevel(kIOPMAssertionLevelOn),
            "io.mpv.video_playing_back" as CFString,
            &displaySleepAssertion)
    }

    func lmuToLux(_ v: UInt64) -> Int {
        // the polinomial approximation for apple lmu value -> lux was empirically
        // derived by firefox developers (Apple provides no documentation).
        // https://bugzilla.mozilla.org/show_bug.cgi?id=793728
        let power_c4: Double = 1 / pow(10, 27)
        let power_c3: Double = 1 / pow(10, 19)
        let power_c2: Double = 1 / pow(10, 12)
        let power_c1: Double = 1 / pow(10, 5)

        let lum = Double(v)
        let term4: Double = -3.0 * power_c4 * pow(lum, 4.0)
        let term3: Double = 2.6 * power_c3 * pow(lum, 3.0)
        let term2: Double = -3.4 * power_c2 * pow(lum, 2.0)
        let term1: Double = 3.9 * power_c1 * lum

        let lux = Int(ceil(term4 + term3 + term2 + term1 - 0.19))
        return lux > 0 ? lux : 0
    }

    var lightSensorCallback: IOServiceInterestCallback = { (ctx, service, messageType, messageArgument) -> Void in
        let com = unsafeBitCast(ctx, to: Common.self)

        var outputs: UInt32 = 2
        var values: [UInt64] = [0, 0]

        var kr = IOConnectCallMethod(com.lightSensor, 0, nil, 0, nil, 0, &values, &outputs, nil, nil)
        if kr == KERN_SUCCESS {
            var mean = (values[0] + values[1]) / 2
            if com.lastLmu != mean {
                com.lastLmu = mean
                com.lightSensorUpdate()
            }
        }
    }

    func lightSensorUpdate() {
        log.warning("lightSensorUpdate not implemented")
    }

    func initLightSensor() {
        let srv = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("AppleLMUController"))
        if srv == IO_OBJECT_NULL {
            log.verbose("Can't find an ambient light sensor")
            return
        }

        lightSensorIOPort = IONotificationPortCreate(kIOMasterPortDefault)
        IONotificationPortSetDispatchQueue(lightSensorIOPort, queue)
        var n = io_object_t()
        IOServiceAddInterestNotification(lightSensorIOPort, srv, kIOGeneralInterest, lightSensorCallback, TypeHelper.bridge(obj: self), &n)
        let kr = IOServiceOpen(srv, mach_task_self_, 0, &lightSensor)
        IOObjectRelease(srv)

        if kr != KERN_SUCCESS {
            log.verbose("Can't start ambient light sensor connection")
            return
        }
        lightSensorCallback(TypeHelper.bridge(obj: self), 0, 0, nil)
    }

    func uninitLightSensor() {
        if lightSensorIOPort != nil {
            IONotificationPortDestroy(lightSensorIOPort)
            IOObjectRelease(lightSensor)
        }
    }

    var reconfigureCallback: CGDisplayReconfigurationCallBack = { (display, flags, userInfo) in
        if flags.contains(.setModeFlag) {
            let com = unsafeBitCast(userInfo, to: Common.self)
            let displayID = com.window?.screen?.displayID ?? display

            if displayID == display {
                com.log.verbose("Detected display mode change, updating screen refresh rate")
                com.flagEvents(VO_EVENT_WIN_STATE)
            }
        }
    }

    func addDisplayReconfigureObserver() {
        CGDisplayRegisterReconfigurationCallback(reconfigureCallback, TypeHelper.bridge(obj: self))
    }

    func removeDisplayReconfigureObserver() {
        CGDisplayRemoveReconfigurationCallback(reconfigureCallback, TypeHelper.bridge(obj: self))
    }

    func addAppNotifications() {
        appNotificationObservers.append(NotificationCenter.default.addObserver(
            forName: NSApplication.didBecomeActiveNotification,
            object: nil,
            queue: .main,
            using: { [weak self] (_) in self?.appDidBecomeActive() }
        ))
        appNotificationObservers.append(NotificationCenter.default.addObserver(
            forName: NSApplication.didResignActiveNotification,
            object: nil,
            queue: .main,
            using: { [weak self] (_) in self?.appDidResignActive() }
        ))
    }

    func removeAppNotifications() {
        appNotificationObservers.forEach { NotificationCenter.default.removeObserver($0) }
        appNotificationObservers.removeAll()
    }

    func appDidBecomeActive() {
        flagEvents(VO_EVENT_FOCUS)
    }

    func appDidResignActive() {
        flagEvents(VO_EVENT_FOCUS)
    }

    func setAppIcon() {
        if ProcessInfo.processInfo.environment["MPVBUNDLE"] != "true" {
            NSApp.applicationIconImage = AppHub.shared.getIcon()
        }
    }

    func updateCursorVisibility() {
        setCursorVisibility(cursorVisibilityWanted)
    }

    func setCursorVisibility(_ visible: Bool) {
        NSCursor.setHiddenUntilMouseMoves(!visible && (view?.canHideCursor() ?? false))
    }

    func updateICCProfile() {
        log.warning("updateICCProfile not implemented")
    }

    func getScreenBy(id screenID: Int) -> NSScreen? {
        if screenID >= NSScreen.screens.count {
            log.info("Screen ID \(screenID) does not exist, falling back to current device")
            return nil
        } else if screenID < 0 {
            return nil
        }
        return NSScreen.screens[screenID]
    }

    func getScreenBy(name screenName: String?) -> NSScreen? {
        for screen in NSScreen.screens {
            if [screen.localizedName, screen.name, screen.uniqueName, screen.serialNumber].contains(screenName) {
                return screen
            }
        }
        return nil
    }

    func getTargetScreen(forFullscreen fs: Bool) -> NSScreen? {
        let screenID = fs ? option.vo.fsscreen_id : option.vo.screen_id
        var name: String?
        if let screenName = fs ? option.vo.fsscreen_name : option.vo.screen_name {
            name = String(cString: screenName)
        }
        return getScreenBy(id: Int(screenID)) ?? getScreenBy(name: name)
    }

    func getCurrentScreen() -> NSScreen? {
         return window != nil ? window?.screen :
                                    getTargetScreen(forFullscreen: false) ??
                                    NSScreen.main
    }

    func getWindowGeometry(forScreen screen: NSScreen,
                           videoOut vo: UnsafeMutablePointer<vo>) -> NSRect {
        let r = screen.convertRectToBacking(screen.frame)
        let targetFrame = option.mac.macos_geometry_calculation == FRAME_VISIBLE
            ? screen.visibleFrame : screen.frame
        let rv = screen.convertRectToBacking(targetFrame)

        // convert origin to be relative to target screen
        var originY = rv.origin.y - r.origin.y
        let originX = rv.origin.x - r.origin.x
        // flip the y origin, mp_rect expects the origin at the top-left
        // macOS' windowing system operates from the bottom-left
        originY = -(originY + rv.size.height)
        var screenRC: mp_rect = mp_rect(x0: Int32(originX),
                                        y0: Int32(originY),
                                        x1: Int32(originX + rv.size.width),
                                        y1: Int32(originY + rv.size.height))

        var geo: vo_win_geometry = vo_win_geometry()
        vo_calc_window_geometry2(vo, &screenRC, Double(screen.backingScaleFactor), &geo)
        vo_apply_window_geometry(vo, &geo)

        let height = CGFloat(geo.win.y1 - geo.win.y0)
        let width = CGFloat(geo.win.x1 - geo.win.x0)
        // flip the y origin again
        let y = CGFloat(-geo.win.y1)
        let x = CGFloat(geo.win.x0)
        return screen.convertRectFromBacking(NSMakeRect(x, y, width, height))
    }

    func getInitProperties(_ vo: UnsafeMutablePointer<vo>) -> (NSScreen, NSRect) {
        guard let targetScreen = getTargetScreen(forFullscreen: false) ?? NSScreen.main else {
            log.error("Something went wrong, no Screen was found")
            exit(1)
        }

        let wr = getWindowGeometry(forScreen: targetScreen, videoOut: vo)

        return (targetScreen, wr)
    }

    // call before initApp, because on macOS +10.15 it changes the active App
    func getActiveApp() -> NSRunningApplication? {
        return NSWorkspace.shared.runningApplications.first(where: {$0.isActive})
    }

    func flagEvents(_ ev: Int) {
        eventsLock.lock()
        events |= ev
        eventsLock.unlock()

        guard let vo = vo else {
            log.warning("vo nil in flagEvents")
            return
        }
        vo_wakeup(vo)
    }

    func checkEvents() -> Int {
        eventsLock.lock()
        let ev = events
        events = 0
        eventsLock.unlock()
        return ev
    }

    func windowDidEndAnimation() {}
    func windowSetToFullScreen() {}
    func windowSetToWindow() {}
    func windowDidUpdateFrame() {}
    func windowDidChangeScreen() {}
    func windowDidChangeScreenProfile() {}
    func windowDidChangeBackingProperties() {}
    func windowWillStartLiveResize() {}
    func windowDidEndLiveResize() {}
    func windowDidResize() {}
    func windowDidChangeOcclusionState() {}

    @objc func control(_ vo: UnsafeMutablePointer<vo>,
                         events: UnsafeMutablePointer<Int32>,
                         request: UInt32,
                         data: UnsafeMutableRawPointer?) -> Int32
    {
        switch mp_voctrl(request) {
        case VOCTRL_CHECK_EVENTS:
            events.pointee |= Int32(checkEvents())
            return VO_TRUE
        case VOCTRL_VO_OPTS_CHANGED:
            var opt: UnsafeMutableRawPointer?
            while option.nextChangedOption(property: &opt) {
                switch opt {
                case TypeHelper.toPointer(&option.voPtr.pointee.border):
                    DispatchQueue.main.async {
                        self.window?.border = Bool(self.option.vo.border)
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.fullscreen):
                    DispatchQueue.main.async {
                        self.window?.toggleFullScreen(nil)
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.ontop): fallthrough
                case TypeHelper.toPointer(&option.voPtr.pointee.ontop_level):
                    DispatchQueue.main.async {
                        self.window?.setOnTop(Bool(self.option.vo.ontop), Int(self.option.vo.ontop_level))
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.all_workspaces):
                    DispatchQueue.main.async {
                        self.window?.setOnAllWorkspaces(Bool(self.option.vo.all_workspaces))
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.keepaspect_window):
                    DispatchQueue.main.async {
                        self.window?.keepAspect = Bool(self.option.vo.keepaspect_window)
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.window_minimized):
                    DispatchQueue.main.async {
                        self.window?.setMinimized(Bool(self.option.vo.window_minimized))
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.window_maximized):
                    DispatchQueue.main.async {
                        self.window?.setMaximized(Bool(self.option.vo.window_maximized))
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.cursor_passthrough):
                    DispatchQueue.main.async {
                        self.window?.ignoresMouseEvents = self.option.vo.cursor_passthrough
                    }
                case TypeHelper.toPointer(&option.voPtr.pointee.geometry): fallthrough
                case TypeHelper.toPointer(&option.voPtr.pointee.autofit): fallthrough
                case TypeHelper.toPointer(&option.voPtr.pointee.autofit_smaller): fallthrough
                case TypeHelper.toPointer(&option.voPtr.pointee.autofit_larger):
                    DispatchQueue.main.async {
                        let (_, wr) = self.getInitProperties(vo)
                        self.window?.updateFrame(wr)
                    }
                default:
                    break
                }
            }
            return VO_TRUE
        case VOCTRL_GET_DISPLAY_FPS:
            let fps = data!.assumingMemoryBound(to: CDouble.self)
            fps.pointee = currentFps()
            return VO_TRUE
        case VOCTRL_GET_WINDOW_ID:
            guard let window = window else {
                return VO_NOTAVAIL
            }
            let wid = data!.assumingMemoryBound(to: Int64.self)
            wid.pointee = unsafeBitCast(window, to: Int64.self)
            return VO_TRUE
        case VOCTRL_GET_HIDPI_SCALE:
            let scaleFactor = data!.assumingMemoryBound(to: CDouble.self)
            let screen = getCurrentScreen()
            let factor = window?.backingScaleFactor ??
                         screen?.backingScaleFactor ?? 1.0
            scaleFactor.pointee = Double(factor)
            return VO_TRUE
        case VOCTRL_RESTORE_SCREENSAVER:
            enableDisplaySleep()
            return VO_TRUE
        case VOCTRL_KILL_SCREENSAVER:
            disableDisplaySleep()
            return VO_TRUE
        case VOCTRL_SET_CURSOR_VISIBILITY:
            let cursorVisibility = data!.assumingMemoryBound(to: CBool.self)
            cursorVisibilityWanted = cursorVisibility.pointee
            DispatchQueue.main.async {
                self.setCursorVisibility(self.cursorVisibilityWanted)
            }
            return VO_TRUE
        case VOCTRL_GET_ICC_PROFILE:
            let screen = getCurrentScreen()
            guard var iccData = screen?.colorSpace?.iccProfileData else {
                log.warning("No Screen available to retrieve ICC profile")
                return VO_TRUE
            }

            let icc = data!.assumingMemoryBound(to: bstr.self)
            iccData.withUnsafeMutableBytes { (ptr: UnsafeMutableRawBufferPointer) in
                guard let baseAddress = ptr.baseAddress, ptr.count > 0 else { return }
                let u8Ptr = baseAddress.assumingMemoryBound(to: UInt8.self)
                icc.pointee = bstrdup(nil, bstr(start: u8Ptr, len: ptr.count))
            }
            return VO_TRUE
        case VOCTRL_GET_AMBIENT_LUX:
            if lightSensor != 0 {
                let lux = data!.assumingMemoryBound(to: Int32.self)
                lux.pointee = Int32(lmuToLux(lastLmu))
                return VO_TRUE;
            }
            return VO_NOTIMPL
        case VOCTRL_GET_UNFS_WINDOW_SIZE:
            let sizeData = data!.assumingMemoryBound(to: Int32.self)
            let size = UnsafeMutableBufferPointer(start: sizeData, count: 2)
            let rect = (Bool(option.vo.hidpi_window_scale) ? window?.unfsContentFrame
                : window?.unfsContentFramePixel) ?? NSRect(x: 0, y: 0, width: 1280, height: 720)

            size[0] = Int32(rect.size.width)
            size[1] = Int32(rect.size.height)
            return VO_TRUE
        case VOCTRL_SET_UNFS_WINDOW_SIZE:
            let sizeData = data!.assumingMemoryBound(to: Int32.self)
            let size = UnsafeBufferPointer(start: sizeData, count: 2)
            var rect = NSMakeRect(0, 0, CGFloat(size[0]), CGFloat(size[1]))
            DispatchQueue.main.async {
                if let screen = self.window?.currentScreen, !Bool(self.option.vo.hidpi_window_scale) {
                    rect = screen.convertRectFromBacking(rect)
                }
                self.window?.updateSize(rect.size)
            }
            return VO_TRUE
        case VOCTRL_GET_DISPLAY_NAMES:
            let dnames = data!.assumingMemoryBound(to: UnsafeMutablePointer<UnsafeMutablePointer<Int8>?>?.self)
            var array: UnsafeMutablePointer<UnsafeMutablePointer<Int8>?>? = nil
            var count: Int32 = 0
            let displayName = getCurrentScreen()?.uniqueName ?? "Unknown"

            app_bridge_tarray_append(nil, &array, &count, ta_xstrdup(nil, displayName))
            app_bridge_tarray_append(nil, &array, &count, nil)
            dnames.pointee = array
            return VO_TRUE
        case VOCTRL_GET_DISPLAY_RES:
            guard let screen = getCurrentScreen() else {
                log.warning("No Screen available to retrieve frame")
                return VO_NOTAVAIL
            }
            let sizeData = data!.assumingMemoryBound(to: Int32.self)
            let size = UnsafeMutableBufferPointer(start: sizeData, count: 2)
            let frame = screen.convertRectToBacking(screen.frame)
            size[0] = Int32(frame.size.width)
            size[1] = Int32(frame.size.height)
            return VO_TRUE
        case VOCTRL_GET_FOCUSED:
            let focus = data!.assumingMemoryBound(to: CBool.self)
            focus.pointee = NSApp.isActive
            return VO_TRUE
        case VOCTRL_UPDATE_WINDOW_TITLE:
            let title = String(cString: data!.assumingMemoryBound(to: CChar.self))
            DispatchQueue.main.async {
                self.title = title
            }
            return VO_TRUE
        default:
            return VO_NOTIMPL
        }
    }

    let macOptsWakeupCallback: swift_wakeup_cb_fn = { ( ctx ) in
        let com = unsafeBitCast(ctx, to: Common.self)
        DispatchQueue.main.async {
            com.macOptsUpdate()
        }
    }

    func macOptsUpdate() {
        var opt: UnsafeMutableRawPointer?
        while option.nextChangedMacOption(property: &opt) {
            switch opt {
            case TypeHelper.toPointer(&option.macPtr.pointee.macos_title_bar_appearance):
                titleBar?.set(appearance: Int(option.mac.macos_title_bar_appearance))
            case TypeHelper.toPointer(&option.macPtr.pointee.macos_title_bar_material):
                titleBar?.set(material: Int(option.mac.macos_title_bar_material))
            case TypeHelper.toPointer(&option.macPtr.pointee.macos_title_bar_color):
                titleBar?.set(color: option.mac.macos_title_bar_color)
            default:
                break
            }
        }
    }
}
