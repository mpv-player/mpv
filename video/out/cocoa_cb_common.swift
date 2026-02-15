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

class CocoaCB: Common, EventSubscriber {
    var libmpv: LibmpvHelper
    var layer: GLLayer?

    var isShuttingDown: Bool = false

    // NSView ref for embedded mode (wid property)
    var externalView: NSView?
    var isEmbedded: Bool { return externalView != nil }

    enum State {
        case uninitialized
        case needsInit
        case initialized
    }
    var backendState: State = .uninitialized

    init(_ mpv: OpaquePointer) {
        let log = LogHelper(mp_log_new(UnsafeMutablePointer(mpv), mp_client_get_log(mpv), "cocoacb"))
        let option = OptionHelper(UnsafeMutablePointer(mpv), mp_client_get_global(mpv))
        libmpv = LibmpvHelper(mpv, log)
        super.init(option, log)
        layer = GLLayer(cocoaCB: self)
        AppHub.shared.event?.subscribe(self, event: .init(name: "MPV_EVENT_SHUTDOWN"))
    }

    // Init for embedded mode (wid property)
    convenience init(_ mpv: OpaquePointer, _ view: NSView) {
        self.init(mpv)
        self.externalView = view
        log.verbose("Initialized in embedded mode, NSView: \(view)")
    }

    func preinit(_ vo: UnsafeMutablePointer<vo>) {
        eventsLock.withLock { self.vo = vo }
        input = InputHelper(vo.pointee.input_ctx, option)

        if backendState == .uninitialized {
            backendState = .needsInit

            guard let layer = self.layer else {
                log.error("Something went wrong, no GLLayer was initialized")
                exit(1)
            }

            if isEmbedded {
                initEmbeddedView(vo, layer)
            } else {
                initView(vo, layer)
            }
            initMisc(vo)
        }
    }

    // Init embedded mode, attach layer to external NSView
    func initEmbeddedView(_ vo: UnsafeMutablePointer<vo>, _ layer: CALayer) {
        guard let extView = externalView else {
            log.error("No NSView provided for embedded mode")
            return
        }

        log.verbose("Attaching GLLayer to external view")

        // Create internal sub-view, hosting the GL layer
        view = View(frame: extView.bounds, common: self)
        guard let sub = view else {
            log.error("Unable to allocate sub-view for embedded mode")
            return
        }

        sub.autoresizingMask = [.width, .height]
        sub.wantsLayer = true
        sub.layer = layer
        sub.layerContentsPlacement = .scaleProportionallyToFit
        layer.delegate = sub
        extView.addSubview(sub)

        // Track external-view size changes
        extView.postsFrameChangedNotifications = true
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(hostViewFrameDidChange(_:)),
            name: NSView.frameDidChangeNotification,
            object: extView)

        let scale = extView.window?.backingScaleFactor ?? NSScreen.main?.backingScaleFactor ?? 1.0
        layer.contentsScale = scale

        log.verbose("Embedded view initialized with scale factor: \(scale)")
    }

    func uninit() {

        log.verbose("Uninit")

        if isEmbedded {
            NotificationCenter.default.removeObserver(self,
                                                      name: NSView.frameDidChangeNotification,
                                                      object: externalView)
            view?.removeFromSuperview()
            externalView?.layer = nil
        } else {
            eventsLock.withLock { self.vo = nil }
            window?.orderOut(nil)
            window?.close()
        }
    }

    func reconfig(_ vo: UnsafeMutablePointer<vo>) {
        eventsLock.withLock { self.vo = vo }
        if backendState == .needsInit {
            DispatchQueue.main.sync { self.initBackend(vo) }
        } else if option.vo.auto_window_resize {
            DispatchQueue.main.async {
                if self.isEmbedded {
                    self.layer?.update(force: true)
                } else {
                    self.updateWindowSize(vo)
                    self.layer?.update(force: true)
                    if self.option.vo.focus_on == 2 {
                        NSApp.activate(ignoringOtherApps: true)
                    }
                }
            }
        }
    }

    func initBackend(_ vo: UnsafeMutablePointer<vo>) {
        log.verbose("InitBackend")

        if isEmbedded {
            updateICCProfile()
        } else {
            let previousActiveApp = getActiveApp()
            initApp()
            initWindow(vo, previousActiveApp)
            updateICCProfile()
            initWindowState()
        }

        backendState = .initialized
    }

    func updateWindowSize(_ vo: UnsafeMutablePointer<vo>) {
        guard let targetScreen = getTargetScreen(forFullscreen: false) ?? NSScreen.main else {
            log.warning("Couldn't update Window size, no Screen available")
            return
        }

        let (wr, _) = getWindowGeometry(forScreen: targetScreen, videoOut: vo)
        if !(window?.isVisible ?? false) && !(window?.isMiniaturized ?? false) && !NSApp.isHidden {
            window?.makeKeyAndOrderFront(nil)
        }
        layer?.atomicDrawingStart()
        window?.updateSize(wr.size)
    }

    override func displayLinkCallback(_ displayLink: CVDisplayLink,
                                      _ inNow: UnsafePointer<CVTimeStamp>,
                                      _ inOutputTime: UnsafePointer<CVTimeStamp>,
                                      _ flagsIn: CVOptionFlags,
                                      _ flagsOut: UnsafeMutablePointer<CVOptionFlags>) -> CVReturn {
        libmpv.reportRenderFlip()
        return kCVReturnSuccess
    }

    func getEmbeddedScreen() -> NSScreen? {
        if isEmbedded {
            return externalView?.window?.screen ?? NSScreen.main
        }
        return window?.screen
    }

    override func getCurrentScreen() -> NSScreen? {
        if isEmbedded {
            return externalView?.window?.screen ??
                   getTargetScreen(forFullscreen: false) ??
                   NSScreen.main
        }
        return window != nil ? window?.screen :
                               getTargetScreen(forFullscreen: false) ??
                               NSScreen.main
    }

    override func updateICCProfile() {
        guard let colorSpace = getEmbeddedScreen()?.colorSpace else {
            log.warning("Couldn't update ICC Profile, no color space available")
            return
        }

        libmpv.setRenderICCProfile(colorSpace)
        let (isEdr, colorspace) = getColorSpace()
        layer?.colorspace = colorspace
        layer?.wantsExtendedDynamicRangeContent = isEdr
    }

    func getColorSpace() -> (Bool, CGColorSpace?) {
        guard let colorSpace = getEmbeddedScreen()?.colorSpace?.cgColorSpace else {
            log.warning("Couldn't retrieve ICC Profile, no color space available")
            return (false, nil)
        }

        let outputCsp = Int(option.mac.cocoa_cb_output_csp)

        switch outputCsp {
        case MAC_CSP_AUTO: return (false, colorSpace)
        case MAC_CSP_DISPLAY_P3: return (true, CGColorSpace(name: CGColorSpace.displayP3))
        case MAC_CSP_DISPLAY_P3_HLG: return (true, CGColorSpace(name: CGColorSpace.displayP3_HLG))
        case MAC_CSP_DCI_P3: return (true, CGColorSpace(name: CGColorSpace.dcip3))
        case MAC_CSP_BT_2020: return (true, CGColorSpace(name: CGColorSpace.itur_2020))
        case MAC_CSP_BT_709: return (false, CGColorSpace(name: CGColorSpace.itur_709))
        case MAC_CSP_SRGB: return (false, CGColorSpace(name: CGColorSpace.sRGB))
        case MAC_CSP_SRGB_LINEAR: return (false, CGColorSpace(name: CGColorSpace.linearSRGB))
        case MAC_CSP_RGB_LINEAR: return (false, CGColorSpace(name: CGColorSpace.genericRGBLinear))
        case MAC_CSP_ADOBE: return (false, CGColorSpace(name: CGColorSpace.adobeRGB1998))
        default: break
        }

#if HAVE_MACOS_10_15_4_FEATURES
        if #available(macOS 10.15.4, *) {
            switch outputCsp {
            case MAC_CSP_DISPLAY_P3_PQ: return (true, CGColorSpace(name: CGColorSpace.displayP3_PQ))
            default: break
            }
        }
#endif

#if HAVE_MACOS_11_FEATURES
        if #available(macOS 11.0, *) {
            switch outputCsp {
            case MAC_CSP_BT_2100_HLG: return (true, CGColorSpace(name: CGColorSpace.itur_2100_HLG))
            case MAC_CSP_BT_2100_PQ: return (true, CGColorSpace(name: CGColorSpace.itur_2100_PQ))
            default: break
            }
        }
#endif

#if HAVE_MACOS_12_FEATURES
        if #available(macOS 12.0, *) {
            switch outputCsp {
            case MAC_CSP_DISPLAY_P3_LINEAR: return (true, CGColorSpace(name: CGColorSpace.linearDisplayP3))
            case MAC_CSP_BT_2020_LINEAR: return (true, CGColorSpace(name: CGColorSpace.linearITUR_2020))
            default: break
            }
        }
#endif

        log.warning("Couldn't retrieve configured color space, falling back to auto")

        return (false, colorSpace)
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
        if isEmbedded {
            layer?.contentsScale = externalView?.window?.backingScaleFactor ?? 1
        } else {
            layer?.contentsScale = window?.backingScaleFactor ?? 1
        }
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

    @objc func hostViewFrameDidChange(_ note: Notification) {
        layer?.update(force: true)
    }

    var controlCallback: mp_render_cb_control_fn = { ( v, ctx, e, request, data ) -> Int32 in
        let ccb = unsafeBitCast(ctx, to: CocoaCB.self)

        guard let vo = v, let events = e else {
            ccb.log.warning("Unexpected nil value in Control Callback")
            return VO_FALSE
        }

        return ccb.control(vo, events: events, request: request, data: data)
    }

    override func control(_ vo: UnsafeMutablePointer<vo>,
                          events: UnsafeMutablePointer<Int32>,
                          request: UInt32,
                          data: UnsafeMutableRawPointer?) -> Int32 {
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
        case VOCTRL_GET_WINDOW_ID:
            if isEmbedded {
                guard let extView = externalView else {
                    return VO_NOTAVAIL
                }
                let wid = data!.assumingMemoryBound(to: Int64.self)
                wid.pointee = unsafeBitCast(extView, to: Int64.self)
                return VO_TRUE
            }
        case VOCTRL_GET_HIDPI_SCALE:
            if isEmbedded {
                let scaleFactor = data!.assumingMemoryBound(to: CDouble.self)
                let factor = externalView?.window?.backingScaleFactor ??
                             getCurrentScreen()?.backingScaleFactor ?? 1.0
                scaleFactor.pointee = Double(factor)
                return VO_TRUE
            }
        case VOCTRL_GET_UNFS_WINDOW_SIZE:
            if isEmbedded {
                let sizeData = data!.assumingMemoryBound(to: Int32.self)
                let size = UnsafeMutableBufferPointer(start: sizeData, count: 2)
                let rect = externalView?.bounds ?? NSRect(x: 0, y: 0, width: 1280, height: 720)

                if Bool(option.vo.hidpi_window_scale) {
                    size[0] = Int32(rect.size.width)
                    size[1] = Int32(rect.size.height)
                } else {
                    let scale = externalView?.window?.backingScaleFactor ?? 1.0
                    size[0] = Int32(rect.size.width * scale)
                    size[1] = Int32(rect.size.height * scale)
                }
                return VO_TRUE
            }
        case VOCTRL_SET_UNFS_WINDOW_SIZE:
            if isEmbedded {
                return VO_NOTIMPL
            }
        case VOCTRL_VO_OPTS_CHANGED:
            if isEmbedded {
                var opt: UnsafeMutableRawPointer?
                while option.nextChangedOption(property: &opt) {
                    // Skip window-specific options in embedded mode
                }
                return VO_TRUE
            }
        default:
            break
        }

        return super.control(vo, events: events, request: request, data: data)
    }

    func shutdown() {
        log.verbose("Shutdown")

        if isEmbedded {
            uninit()
            uninitCommon()

            layer?.lockCglContext()
            libmpv.uninit()
            layer?.unlockCglContext()
        } else {
            isShuttingDown = window?.isAnimating ?? false ||
                             window?.isInFullscreen ?? false && option.vo.native_fs
            if window?.isInFullscreen ?? false && !(window?.isAnimating ?? false) {
                window?.close()
            }
            if isShuttingDown { return }

            uninit()
            uninitCommon()
            window = nil

            layer?.lockCglContext()
            libmpv.uninit()
            layer?.unlockCglContext()
        }
    }

    func checkShutdown() {
        if isShuttingDown {
            shutdown()
        }
    }

    func handle(event: EventHelper.Event) {
        if event.name == String(describing: MPV_EVENT_SHUTDOWN) { shutdown() }
    }
}
