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

class CocoaCB: NSObject {

    var mpv: MPVHelper
    var window: Window?
    var titleBar: TitleBar?
    var view: EventsView?
    var layer: VideoLayer?
    var link: CVDisplayLink?

    var cursorHidden: Bool = false
    var cursorVisibilityWanted: Bool = true
    @objc var isShuttingDown: Bool = false

    var title: String = "mpv" {
        didSet { if let window = window { window.title = title } }
    }

    enum State {
        case uninitialized
        case needsInit
        case initialized
    }
    var backendState: State = .uninitialized

    let eventsLock = NSLock()
    var events: Int = 0

    var lightSensor: io_connect_t = 0
    var lastLmu: UInt64 = 0
    var lightSensorIOPort: IONotificationPortRef?
    var displaySleepAssertion: IOPMAssertionID = IOPMAssertionID(0)

    let queue: DispatchQueue = DispatchQueue(label: "io.mpv.queue")

    @objc init(_ mpvHandle: OpaquePointer) {
        mpv = MPVHelper(mpvHandle)
        super.init()
        layer = VideoLayer(cocoaCB: self)
    }

    func preinit(_ vo: UnsafeMutablePointer<vo>) {
        if backendState == .uninitialized {
            backendState = .needsInit

            view = EventsView(cocoaCB: self)
            view?.layer = layer
            view?.wantsLayer = true
            view?.layerContentsPlacement = .scaleProportionallyToFit
            startDisplayLink(vo)
            initLightSensor()
            addDisplayReconfigureObserver()
        }
    }

    func uninit() {
        window?.orderOut(nil)
    }

    func reconfig(_ vo: UnsafeMutablePointer<vo>) {
        mpv.vo = vo
        if backendState == .needsInit {
            DispatchQueue.main.sync { self.initBackend(vo) }
        } else {
            DispatchQueue.main.async {
                self.updateWindowSize(vo)
                self.layer?.update()
            }
        }
    }

    func initBackend(_ vo: UnsafeMutablePointer<vo>) {
        let opts: mp_vo_opts = vo.pointee.opts.pointee
        NSApp.setActivationPolicy(.regular)
        setAppIcon()

        guard let view = self.view else {
            mpv.sendError("Something went wrong, no View was initialized")
            exit(1)
        }
        guard let targetScreen = getScreenBy(id: Int(opts.screen_id)) ?? NSScreen.main else {
            mpv.sendError("Something went wrong, no Screen was found")
            exit(1)
        }

        let wr = getWindowGeometry(forScreen: targetScreen, videoOut: vo)
        window = Window(contentRect: wr, screen: targetScreen, view: view, cocoaCB: self)
        guard let window = self.window else {
            mpv.sendError("Something went wrong, no Window was initialized")
            exit(1)
        }

        updateICCProfile()
        window.setOnTop(Bool(opts.ontop), Int(opts.ontop_level))
        window.keepAspect = Bool(opts.keepaspect_window)
        window.title = title
        window.border = Bool(opts.border)

        titleBar = TitleBar(frame: wr, window: window, cocoaCB: self)

        window.isRestorable = false
        window.makeMain()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)

        if Bool(opts.fullscreen) {
            DispatchQueue.main.async {
                self.window?.toggleFullScreen(nil)
            }
        } else {
            window.isMovableByWindowBackground = true
        }

        backendState = .initialized
    }

    func updateWindowSize(_ vo: UnsafeMutablePointer<vo>) {
        let opts: mp_vo_opts = vo.pointee.opts.pointee
        guard let targetScreen = getScreenBy(id: Int(opts.screen_id)) ?? NSScreen.main else {
            mpv.sendWarning("Couldn't update Window size, no Screen available")
            return
        }

        let wr = getWindowGeometry(forScreen: targetScreen, videoOut: vo)
        if !(window?.isVisible ?? false) {
            window?.makeKeyAndOrderFront(nil)
        }
        layer?.atomicDrawingStart()
        window?.updateSize(wr.size)
    }

    func setAppIcon() {
        if let app = NSApp as? Application {
            NSApp.applicationIconImage = app.getMPVIcon()
        }
    }

    let linkCallback: CVDisplayLinkOutputCallback = {
                    (displayLink: CVDisplayLink,
                           inNow: UnsafePointer<CVTimeStamp>,
                    inOutputTime: UnsafePointer<CVTimeStamp>,
                         flagsIn: CVOptionFlags,
                        flagsOut: UnsafeMutablePointer<CVOptionFlags>,
              displayLinkContext: UnsafeMutableRawPointer?) -> CVReturn in
        let ccb = unsafeBitCast(displayLinkContext, to: CocoaCB.self)
        ccb.mpv.reportRenderFlip()
        return kCVReturnSuccess
    }

    func startDisplayLink(_ vo: UnsafeMutablePointer<vo>) {
        let opts: mp_vo_opts = vo.pointee.opts.pointee
        CVDisplayLinkCreateWithActiveCGDisplays(&link)

        guard let screen = getScreenBy(id: Int(opts.screen_id)) ?? NSScreen.main,
              let link = self.link else
        {
            mpv.sendWarning("Couldn't start DisplayLink, no Screen or DisplayLink available")
            return
        }

        CVDisplayLinkSetCurrentCGDisplay(link, screen.displayID)
        if #available(macOS 10.12, *) {
            CVDisplayLinkSetOutputHandler(link) { link, now, out, inFlags, outFlags -> CVReturn in
                self.mpv.reportRenderFlip()
                return kCVReturnSuccess
            }
        } else {
            CVDisplayLinkSetOutputCallback(link, linkCallback, MPVHelper.bridge(obj: self))
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
            mpv.sendWarning("Couldn't update DisplayLink, no Screen or DisplayLink available")
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
                    mpv.sendVerbose("Falling back to nominal display refresh rate: \(nominalFps)")
                    return nominalFps
                } else {
                    return actualFps
                }
            }
        } else {
            mpv.sendWarning("No DisplayLink available")
        }

        mpv.sendWarning("Falling back to standard display refresh rate: 60Hz")
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

    func updateCusorVisibility() {
        setCursorVisiblility(cursorVisibilityWanted)
    }

    func setCursorVisiblility(_ visible: Bool) {
        let visibility = visible ? true : !(view?.canHideCursor() ?? false)
        if visibility && cursorHidden {
            NSCursor.unhide()
            cursorHidden = false;
        } else if !visibility && !cursorHidden {
            NSCursor.hide()
            cursorHidden = true
        }
    }

    func updateICCProfile() {
        guard let colorSpace = window?.screen?.colorSpace else {
            mpv.sendWarning("Couldn't update ICC Profile, no color space available")
            return
        }

        mpv.setRenderICCProfile(colorSpace)
        if #available(macOS 10.11, *) {
            layer?.colorspace = colorSpace.cgColorSpace
        }
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
        let ccb = unsafeBitCast(ctx, to: CocoaCB.self)

        var outputs: UInt32 = 2
        var values: [UInt64] = [0, 0]

        var kr = IOConnectCallMethod(ccb.lightSensor, 0, nil, 0, nil, 0, &values, &outputs, nil, nil)
        if kr == KERN_SUCCESS {
            var mean = (values[0] + values[1]) / 2
            if ccb.lastLmu != mean {
                ccb.lastLmu = mean
                ccb.mpv.setRenderLux(ccb.lmuToLux(ccb.lastLmu))
            }
        }
    }

    func initLightSensor() {
        let srv = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("AppleLMUController"))
        if srv == IO_OBJECT_NULL {
            mpv.sendVerbose("Can't find an ambient light sensor")
            return
        }

        lightSensorIOPort = IONotificationPortCreate(kIOMasterPortDefault)
        IONotificationPortSetDispatchQueue(lightSensorIOPort, queue)
        var n = io_object_t()
        IOServiceAddInterestNotification(lightSensorIOPort, srv, kIOGeneralInterest, lightSensorCallback, MPVHelper.bridge(obj: self), &n)
        let kr = IOServiceOpen(srv, mach_task_self_, 0, &lightSensor)
        IOObjectRelease(srv)

        if kr != KERN_SUCCESS {
            mpv.sendVerbose("Can't start ambient light sensor connection")
            return
        }
        lightSensorCallback(MPVHelper.bridge(obj: self), 0, 0, nil)
    }

    func uninitLightSensor() {
        if lightSensorIOPort != nil {
            IONotificationPortDestroy(lightSensorIOPort)
            IOObjectRelease(lightSensor)
        }
    }

    var reconfigureCallback: CGDisplayReconfigurationCallBack = { (display, flags, userInfo) in
        if flags.contains(.setModeFlag) {
            let ccb = unsafeBitCast(userInfo, to: CocoaCB.self)
            let displayID = ccb.window?.screen?.displayID ?? display

            if displayID == display {
                ccb.mpv.sendVerbose("Detected display mode change, updating screen refresh rate");
                ccb.flagEvents(VO_EVENT_WIN_STATE)
            }
        }
    }

    func addDisplayReconfigureObserver() {
        CGDisplayRegisterReconfigurationCallback(reconfigureCallback, MPVHelper.bridge(obj: self))
    }

    func removeDisplayReconfigureObserver() {
        CGDisplayRemoveReconfigurationCallback(reconfigureCallback, MPVHelper.bridge(obj: self))
    }

    func getTargetScreen(forFullscreen fs: Bool) -> NSScreen? {
        let screenType = fs ? "fs-screen" : "screen"
        let screenID = mpv.getStringProperty(screenType) ?? "current"

        switch screenID {
        case "current", "default", "all":
            return getScreenBy(id: -1)
        default:
            return getScreenBy(id: Int(screenID) ?? 0)
        }
    }

    func getScreenBy(id screenID: Int) -> NSScreen? {
        if screenID >= NSScreen.screens.count {
            mpv.sendInfo("Screen ID \(screenID) does not exist, falling back to current device")
            return nil
        } else if screenID < 0 {
            return nil
        }
        return NSScreen.screens[screenID]
    }

    func getWindowGeometry(forScreen targetScreen: NSScreen,
                           videoOut vo: UnsafeMutablePointer<vo>) -> NSRect {
        let r = targetScreen.convertRectToBacking(targetScreen.frame)
        var screenRC: mp_rect = mp_rect(x0: Int32(0),
                                        y0: Int32(0),
                                        x1: Int32(r.size.width),
                                        y1: Int32(r.size.height))

        var geo: vo_win_geometry = vo_win_geometry()
        vo_calc_window_geometry2(vo, &screenRC, Double(targetScreen.backingScaleFactor), &geo)

        // flip y coordinates
        geo.win.y1 = Int32(r.size.height) - geo.win.y1
        geo.win.y0 = Int32(r.size.height) - geo.win.y0

        let wr = NSMakeRect(CGFloat(geo.win.x0), CGFloat(geo.win.y1),
                            CGFloat(geo.win.x1 - geo.win.x0),
                            CGFloat(geo.win.y0 - geo.win.y1))
        return targetScreen.convertRectFromBacking(wr)
    }

    func flagEvents(_ ev: Int) {
        eventsLock.lock()
        events |= ev
        eventsLock.unlock()
        vo_wakeup(mpv.vo)
    }

    func checkEvents() -> Int {
        eventsLock.lock()
        let ev = events
        events = 0
        eventsLock.unlock()
        return ev
    }

    var controlCallback: mp_render_cb_control_fn = { ( vo, ctx, events, request, data ) -> Int32 in
        let ccb = unsafeBitCast(ctx, to: CocoaCB.self)

        switch mp_voctrl(request) {
        case VOCTRL_CHECK_EVENTS:
            if let ev = events {
                ev.pointee = Int32(ccb.checkEvents())
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_FULLSCREEN:
            DispatchQueue.main.async {
                ccb.window?.toggleFullScreen(nil)
            }
            return VO_TRUE
        case VOCTRL_GET_FULLSCREEN:
            if let fsData = data?.assumingMemoryBound(to: Int32.self) {
                fsData.pointee = (ccb.window?.isInFullscreen ?? false) ? 1 : 0
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_GET_DISPLAY_FPS:
            if let fps = data?.assumingMemoryBound(to: CDouble.self) {
                fps.pointee = ccb.currentFps()
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_RESTORE_SCREENSAVER:
            ccb.enableDisplaySleep()
            return VO_TRUE
        case VOCTRL_KILL_SCREENSAVER:
            ccb.disableDisplaySleep()
            return VO_TRUE
        case VOCTRL_SET_CURSOR_VISIBILITY:
            if let cursorVisibility = data?.assumingMemoryBound(to: CBool.self) {
                ccb.cursorVisibilityWanted = cursorVisibility.pointee
                DispatchQueue.main.async {
                    ccb.setCursorVisiblility(ccb.cursorVisibilityWanted)
                }
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_SET_UNFS_WINDOW_SIZE:
            if let sizeData = data?.assumingMemoryBound(to: Int32.self) {
                let size = UnsafeBufferPointer(start: sizeData, count: 2)
                var rect = NSMakeRect(0, 0, CGFloat(size[0]), CGFloat(size[1]))
                DispatchQueue.main.async {
                    if let screen = ccb.window?.currentScreen, !ccb.mpv.getBoolProperty("hidpi-window-scale") {
                        rect = screen.convertRectFromBacking(rect)
                    }
                    ccb.window?.updateSize(rect.size)
                }
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_GET_WIN_STATE:
            if let minimized = data?.assumingMemoryBound(to: Int32.self) {
                minimized.pointee = ccb.window?.isMiniaturized ?? false ?
                    VO_WIN_STATE_MINIMIZED : Int32(0)
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_GET_DISPLAY_NAMES:
            if let opts: mp_vo_opts = vo?.pointee.opts?.pointee,
               let dnames = data?.assumingMemoryBound(to: UnsafeMutablePointer<UnsafeMutablePointer<Int8>?>?.self)
            {
                var array: UnsafeMutablePointer<UnsafeMutablePointer<Int8>?>? = nil
                var count: Int32 = 0
                let screen = ccb.window != nil ? ccb.window?.screen :
                                                 ccb.getScreenBy(id: Int(opts.screen_id)) ??
                                                 NSScreen.main
                let displayName = screen?.displayName ?? "Unknown"

                SWIFT_TARRAY_STRING_APPEND(nil, &array, &count, ta_xstrdup(nil, displayName))
                SWIFT_TARRAY_STRING_APPEND(nil, &array, &count, nil)
                dnames.pointee = array
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_UPDATE_WINDOW_TITLE:
            if let titleData = data?.assumingMemoryBound(to: Int8.self) {
                DispatchQueue.main.async {
                    ccb.title = String(cString: titleData)
                }
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_PREINIT:
            if let vout = vo {
                DispatchQueue.main.sync { ccb.preinit(vout) }
                return VO_TRUE
            }
            return VO_FALSE
        case VOCTRL_UNINIT:
            DispatchQueue.main.async { ccb.uninit() }
            return VO_TRUE
        case VOCTRL_RECONFIG:
            if let vout = vo {
                ccb.reconfig(vout)
                return VO_TRUE
            }
            return VO_FALSE
        default:
            return VO_NOTIMPL
        }
    }

    func shutdown(_ destroy: Bool = false) {
        isShuttingDown = window?.isAnimating ?? false ||
                         window?.isInFullscreen ?? false && mpv.getBoolProperty("native-fs")
        if window?.isInFullscreen ?? false && !(window?.isAnimating ?? false) {
            window?.close()
        }
        if isShuttingDown { return }

        setCursorVisiblility(true)
        stopDisplaylink()
        uninitLightSensor()
        removeDisplayReconfigureObserver()
        mpv.deinitRender()
        mpv.deinitMPV(destroy)
    }

    func checkShutdown() {
        if isShuttingDown {
            shutdown(true)
        }
    }

    @objc func processEvent(_ event: UnsafePointer<mpv_event>) {
        switch event.pointee.event_id {
        case MPV_EVENT_SHUTDOWN:
            shutdown()
        case MPV_EVENT_PROPERTY_CHANGE:
            if backendState == .initialized {
                handlePropertyChange(event)
            }
        default:
            break
        }
    }

    func handlePropertyChange(_ event: UnsafePointer<mpv_event>) {
        let pData = OpaquePointer(event.pointee.data)
        guard let property = UnsafePointer<mpv_event_property>(pData)?.pointee else {
            return
        }

        switch String(cString: property.name) {
        case "border":
            if let data = MPVHelper.mpvFlagToBool(property.data) {
                window?.border = data
            }
        case "ontop":
            if let data = MPVHelper.mpvFlagToBool(property.data) {
                window?.setOnTop(data, mpv.getStringProperty("ontop-level") ?? "window")
            }
        case "keepaspect-window":
            if let data = MPVHelper.mpvFlagToBool(property.data) {
                window?.keepAspect = data
            }
        case "macos-title-bar-appearance":
            if let data = MPVHelper.mpvStringArrayToString(property.data) {
                titleBar?.set(appearance: data)
            }
        case "macos-title-bar-material":
            if let data = MPVHelper.mpvStringArrayToString(property.data) {
                titleBar?.set(material: data)
            }
        case "macos-title-bar-color":
            if let data = MPVHelper.mpvStringArrayToString(property.data) {
                titleBar?.set(color: data)
            }
        default:
            break
        }
    }
}
