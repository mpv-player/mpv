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

class CocoaCBEmbedded: CocoaCB {
    let externalView: NSView

    init(_ mpv: OpaquePointer, _ view: NSView) {
        externalView = view
        super.init(mpv)
        log.verbose("Initialized in embedded mode, NSView: \(view)")
    }

    override func getEmbeddedBackingScaleFactor() -> CGFloat {
        return externalView.window?.backingScaleFactor ??
               getCurrentScreen()?.backingScaleFactor ?? 1.0
    }

    override func preinit(_ vo: UnsafeMutablePointer<vo>) {
        eventsLock.withLock { self.vo = vo }
        input = InputHelper(vo.pointee.input_ctx, option)

        if backendState == .uninitialized {
            backendState = .needsInit

            guard let layer = self.layer else {
                log.error("Something went wrong, no GLLayer was initialized")
                exit(1)
            }

            initEmbeddedView(layer)
            initMisc(vo)
        }
    }

    func initEmbeddedView(_ layer: CALayer) {
        log.verbose("Attaching GLLayer to external view")

        view = View(frame: externalView.bounds, common: self)
        guard let sub = view else {
            log.error("Unable to allocate sub-view for embedded mode")
            return
        }

        sub.autoresizingMask = [.width, .height]
        sub.wantsLayer = true
        sub.layer = layer
        sub.layerContentsPlacement = .scaleProportionallyToFit
        layer.delegate = sub
        externalView.addSubview(sub)

        externalView.postsFrameChangedNotifications = true
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(hostViewFrameDidChange(_:)),
            name: NSView.frameDidChangeNotification,
            object: externalView)

        let scale = getEmbeddedBackingScaleFactor()
        layer.contentsScale = scale

        log.verbose("Embedded view initialized with scale factor: \(scale)")
    }

    override func uninit() {
        eventsLock.withLock { self.vo = nil }
        NotificationCenter.default.removeObserver(self,
                                                  name: NSView.frameDidChangeNotification,
                                                  object: externalView)
        view?.removeFromSuperview()
    }

    override func reconfig(_ vo: UnsafeMutablePointer<vo>) {
        eventsLock.withLock { self.vo = vo }
        if backendState == .needsInit {
            DispatchQueue.main.sync { self.initBackend(vo) }
        } else if option.vo.auto_window_resize {
            DispatchQueue.main.async {
                self.layer?.update(force: true)
            }
        }
    }

    override func initBackend(_ vo: UnsafeMutablePointer<vo>) {
        updateICCProfile()
        backendState = .initialized
    }

    override func updateICCProfile() {
        guard let colorSpace = getCurrentScreen()?.colorSpace else {
            log.warning("Couldn't update ICC Profile, no color space available")
            return
        }

        libmpv.setRenderICCProfile(colorSpace)
        let (isEdr, colorspace) = getColorSpace()
        layer?.colorspace = colorspace
        layer?.wantsExtendedDynamicRangeContent = isEdr
    }

    override func getColorSpace() -> (Bool, CGColorSpace?) {
        guard let colorSpace = getCurrentScreen()?.colorSpace?.cgColorSpace else {
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

    override func getCurrentScreen() -> NSScreen? {
        return externalView.window?.screen ??
               getTargetScreen(forFullscreen: false) ??
               NSScreen.main
    }

    override func windowDidChangeBackingProperties() {
        layer?.contentsScale = getEmbeddedBackingScaleFactor()
    }

    @objc func hostViewFrameDidChange(_ note: Notification) {
        layer?.update(force: true)
    }

    override func control(_ vo: UnsafeMutablePointer<vo>,
                          events: UnsafeMutablePointer<Int32>,
                          request: UInt32,
                          data: UnsafeMutableRawPointer?) -> Int32 {
        switch mp_voctrl(request) {
        case VOCTRL_GET_WINDOW_ID:
            let wid = data!.assumingMemoryBound(to: Int64.self)
            wid.pointee = unsafeBitCast(externalView, to: Int64.self)
            return VO_TRUE
        case VOCTRL_GET_HIDPI_SCALE:
            let scaleFactor = data!.assumingMemoryBound(to: CDouble.self)
            scaleFactor.pointee = Double(getEmbeddedBackingScaleFactor())
            return VO_TRUE
        case VOCTRL_GET_UNFS_WINDOW_SIZE:
            let sizeData = data!.assumingMemoryBound(to: Int32.self)
            let size = UnsafeMutableBufferPointer(start: sizeData, count: 2)
            let rect = externalView.bounds

            if Bool(option.vo.hidpi_window_scale) {
                size[0] = Int32(rect.size.width)
                size[1] = Int32(rect.size.height)
            } else {
                let scale = getEmbeddedBackingScaleFactor()
                size[0] = Int32(rect.size.width * scale)
                size[1] = Int32(rect.size.height * scale)
            }
            return VO_TRUE
        case VOCTRL_SET_UNFS_WINDOW_SIZE:
            return VO_NOTIMPL
        case VOCTRL_VO_OPTS_CHANGED:
            var opt: UnsafeMutableRawPointer?
            while option.nextChangedOption(property: &opt) {
                // Skip window-specific options in embedded mode.
            }
            return VO_TRUE
        default:
            return super.control(vo, events: events, request: request, data: data)
        }
    }

    override func shutdown() {
        log.verbose("Shutdown")
        uninit()
        uninitCommon()
        layer?.lockCglContext()
        libmpv.uninit()
        layer?.unlockCglContext()
    }
}
