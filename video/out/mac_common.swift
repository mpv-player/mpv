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

class MacCommon: Common {
    @objc var layer: MetalLayer?

    var timer: PreciseTimer?
    var swapTime: UInt64 = 0
    let swapLock: NSCondition = NSCondition()

    var needsICCUpdate: Bool = false

    @objc init(_ vo: UnsafeMutablePointer<vo>) {
        let newlog = mp_log_new(vo, vo.pointee.log, "mac")
        super.init(newlog)
        mpv = MPVHelper(vo, log)
        timer =  PreciseTimer(common: self)

        DispatchQueue.main.sync {
            layer = MetalLayer(common: self)
            initMisc(vo)
        }
    }

    @objc func config(_ vo: UnsafeMutablePointer<vo>) -> Bool {
        mpv?.vo = vo

        DispatchQueue.main.sync {
            let previousActiveApp = getActiveApp()
            initApp()

            let (_, _, wr) = getInitProperties(vo)

            guard let layer = self.layer else {
                log.sendError("Something went wrong, no MetalLayer was initialized")
                exit(1)
            }

            if window == nil {
                initView(vo, layer)
                initWindow(vo, previousActiveApp)
                initWindowState()
            }

            if !NSEqualSizes(window?.unfsContentFramePixel.size ?? NSZeroSize, wr.size) {
                window?.updateSize(wr.size)
            }

            windowDidResize()
            needsICCUpdate = true
        }

        return true
    }

    @objc func uninit(_ vo: UnsafeMutablePointer<vo>) {
        window?.waitForAnimation()

        DispatchQueue.main.sync {
            window?.delegate = nil
            window?.close()

            uninitCommon()
        }
    }

    @objc func swapBuffer() {
        swapLock.lock()
        //let oldSwapTime = swapTime
        //while(oldSwapTime == swapTime) {
        while(swapTime < 1) {
            swapLock.wait()
        }
        swapTime = 0
        swapLock.unlock()

        if needsICCUpdate {
            needsICCUpdate = false
            updateICCProfile()
        }
    }

    func updateRenderSize(_ size: NSSize) {
        mpv?.vo.pointee.dwidth = Int32(size.width)
        mpv?.vo.pointee.dheight = Int32(size.height)
        flagEvents(VO_EVENT_RESIZE | VO_EVENT_EXPOSE)
    }

    // TODO draw in background
    // TODO proper timing
    // TODO make time configurable
    override func displayLinkCallback(_ displayLink: CVDisplayLink,
                                            _ inNow: UnsafePointer<CVTimeStamp>,
                                     _ inOutputTime: UnsafePointer<CVTimeStamp>,
                                          _ flagsIn: CVOptionFlags,
                                         _ flagsOut: UnsafeMutablePointer<CVOptionFlags>) -> CVReturn
    {
        timer?.scheduleAt(time: inOutputTime.pointee.hostTime) {
            self.swapLock.lock()
            self.swapTime += 1
            /*if (self.swapTime > 1) {
                print("no swap for vysnc, behind: \(self.swapTime)")
            }*/
            self.swapLock.signal()
            self.swapLock.unlock()
        }

        return kCVReturnSuccess
    }

    override func startDisplayLink(_ vo: UnsafeMutablePointer<vo>) {
        super.startDisplayLink(vo)
        timer?.updatePolicy(refreshRate: currentFps())
    }

    override func updateDisplaylink() {
        super.updateDisplaylink()
        timer?.updatePolicy(refreshRate: currentFps())
    }

    override func lightSensorUpdate() {
        flagEvents(VO_EVENT_AMBIENT_LIGHTING_CHANGED)
    }

    @objc override func updateICCProfile() {
        guard let colorSpace = window?.screen?.colorSpace else {
            log.sendWarning("Couldn't update ICC Profile, no color space available")
            return
        }

        if #available(macOS 10.11, *) {
            layer?.colorspace = colorSpace.cgColorSpace
        }

        flagEvents(VO_EVENT_ICC_PROFILE_CHANGED)
    }

    override func windowDidResize() {
        guard let window = window else {
            log.sendWarning("No window available on window resize event")
            return
        }

        updateRenderSize(window.framePixel.size)
    }

    override func windowDidChangeScreenProfile() {
        needsICCUpdate = true
    }

    override func windowDidChangeBackingProperties() {
        layer?.contentsScale = window?.backingScaleFactor ?? 1
        windowDidResize()
    }
}
