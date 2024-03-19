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

class CocoaCB: Common {
    var libmpv: LibmpvHelper
    var layer: GLLayer?

    @objc var isShuttingDown: Bool = false

    enum State {
        case uninitialized
        case needsInit
        case initialized
    }
    var backendState: State = .uninitialized


    @objc init(_ mpvHandle: OpaquePointer) {
        let newlog = mp_log_new(UnsafeMutablePointer<MPContext>(mpvHandle), mp_client_get_log(mpvHandle), "cocoacb")
        libmpv = LibmpvHelper(mpvHandle, newlog)
        super.init(newlog)
        layer = GLLayer(cocoaCB: self)
    }

    func preinit(_ vo: UnsafeMutablePointer<vo>) {
        option = OptionHelper(vo, log)
        input = InputHelper(vo.pointee.input_ctx, option)

        if backendState == .uninitialized {
            backendState = .needsInit

            guard let layer = self.layer else {
                log.sendError("Something went wrong, no GLLayer was initialized")
                exit(1)
            }

            initView(vo, layer)
            initMisc(vo)
        }
    }

    func uninit() {
        window?.orderOut(nil)
        window?.close()
        option = nil
    }

    func reconfig(_ vo: UnsafeMutablePointer<vo>) {
        option?.vo = vo
        if backendState == .needsInit {
            DispatchQueue.main.sync { self.initBackend(vo) }
        } else if option?.opts.auto_window_resize ?? true {
            DispatchQueue.main.async {
                self.updateWindowSize(vo)
                self.layer?.update(force: true)
                if self.option?.opts.focus_on ?? 1 == 2 {
                    NSApp.activate(ignoringOtherApps: true)
                }
            }
        }
    }

    func initBackend(_ vo: UnsafeMutablePointer<vo>) {
        let previousActiveApp = getActiveApp()
        initApp()
        initWindow(vo, previousActiveApp)
        updateICCProfile()
        initWindowState()

        backendState = .initialized
    }

    func updateWindowSize(_ vo: UnsafeMutablePointer<vo>) {
        guard let targetScreen = getTargetScreen(forFullscreen: false) ?? NSScreen.main else
        {
            log.sendWarning("Couldn't update Window size, no Screen available")
            return
        }

        let wr = getWindowGeometry(forScreen: targetScreen, videoOut: vo)
        if !(window?.isVisible ?? false) &&
           !(window?.isMiniaturized ?? false) &&
           !NSApp.isHidden
        {
            window?.makeKeyAndOrderFront(nil)
        }
        layer?.atomicDrawingStart()
        window?.updateSize(wr.size)
    }

    override func displayLinkCallback(_ displayLink: CVDisplayLink,
                                            _ inNow: UnsafePointer<CVTimeStamp>,
                                     _ inOutputTime: UnsafePointer<CVTimeStamp>,
                                          _ flagsIn: CVOptionFlags,
                                         _ flagsOut: UnsafeMutablePointer<CVOptionFlags>) -> CVReturn
    {
        libmpv.reportRenderFlip()
        return kCVReturnSuccess
    }

    override func lightSensorUpdate() {
        libmpv.setRenderLux(lmuToLux(lastLmu))
    }

    override func updateICCProfile() {
        guard let colorSpace = window?.screen?.colorSpace else {
            log.sendWarning("Couldn't update ICC Profile, no color space available")
            return
        }

        libmpv.setRenderICCProfile(colorSpace)
        layer?.colorspace = colorSpace.cgColorSpace
    }

    override func windowDidEndAnimation() {
        layer?.update()
        checkShutdown()
    }

    override func windowSetToFullScreen() {
        layer?.update(force: true)
    }

    override func windowSetToWindow() {
        layer?.update(force: true)
    }

    override func windowDidUpdateFrame() {
        layer?.update(force: true)
    }

    override func windowDidChangeScreen() {
        layer?.update(force: true)
    }

    override func windowDidChangeScreenProfile() {
        layer?.needsICCUpdate = true
    }

    override func windowDidChangeBackingProperties() {
        layer?.contentsScale = window?.backingScaleFactor ?? 1
    }

    override func windowWillStartLiveResize() {
        layer?.inLiveResize = true
    }

    override func windowDidEndLiveResize() {
        layer?.inLiveResize = false
    }

    override func windowDidChangeOcclusionState() {
        layer?.update(force: true)
    }

    var controlCallback: mp_render_cb_control_fn = { ( v, ctx, e, request, data ) -> Int32 in
        let ccb = unsafeBitCast(ctx, to: CocoaCB.self)

        guard let vo = v, let events = e else {
            ccb.log.sendWarning("Unexpected nil value in Control Callback")
            return VO_FALSE
        }

        return ccb.control(vo, events: events, request: request, data: data)
    }

    override func control(_ vo: UnsafeMutablePointer<vo>,
                    events: UnsafeMutablePointer<Int32>,
                    request: UInt32,
                    data: UnsafeMutableRawPointer?) -> Int32
    {
        switch mp_voctrl(request) {
        case VOCTRL_PREINIT:
            DispatchQueue.main.sync { self.preinit(vo) }
            return VO_TRUE
        case VOCTRL_UNINIT:
            DispatchQueue.main.async { self.uninit() }
            return VO_TRUE
        case VOCTRL_RECONFIG:
            reconfig(vo)
            return VO_TRUE
        default:
            break
        }

        return super.control(vo, events: events, request: request, data: data)
    }

    func shutdown(_ destroy: Bool = false) {
        isShuttingDown = window?.isAnimating ?? false ||
                         window?.isInFullscreen ?? false && option?.opts.native_fs ?? true
        if window?.isInFullscreen ?? false && !(window?.isAnimating ?? false) {
            window?.close()
        }
        if isShuttingDown { return }

        uninit()
        uninitCommon()

        layer?.lockCglContext()
        libmpv.deinitRender()
        layer?.unlockCglContext()
        libmpv.deinitMPV(destroy)
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
        default:
            break
        }
    }
}
