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
import OpenGL.GL
import OpenGL.GL3

class VideoLayer: CAOpenGLLayer {

    weak var cocoaCB: CocoaCB!
    var mpv: MPVHelper! {
        get { return cocoaCB == nil ? nil : cocoaCB.mpv }
    }

    let videoLock = NSLock()
    let displayLock = NSLock()
    var needsFlip: Bool = false
    var forceDraw: Bool = false
    var cglContext: CGLContextObj? = nil
    var cglPixelFormat: CGLPixelFormatObj? = nil
    var surfaceSize: NSSize?

    enum Draw: Int { case normal = 1, atomic, atomicEnd }
    var draw: Draw = .normal

    let queue: DispatchQueue = DispatchQueue(label: "io.mpv.queue.draw")

    let glVersions: [CGLOpenGLProfile] = [
        kCGLOGLPVersion_3_2_Core,
        kCGLOGLPVersion_Legacy
    ]

    let glFormatBase: [CGLPixelFormatAttribute] = [
        kCGLPFAOpenGLProfile,
        kCGLPFAAccelerated,
        kCGLPFADoubleBuffer
    ]

    let glFormatSoftwareBase: [CGLPixelFormatAttribute] = [
        kCGLPFAOpenGLProfile,
        kCGLPFARendererID,
        CGLPixelFormatAttribute(UInt32(kCGLRendererGenericFloatID)),
        kCGLPFADoubleBuffer
    ]

    let glFormatOptional: [CGLPixelFormatAttribute] = [
        kCGLPFABackingStore,
        kCGLPFAAllowOfflineRenderers,
        kCGLPFASupportsAutomaticGraphicsSwitching
    ]

    let attributeLookUp: [UInt32:String] = [
        kCGLOGLPVersion_3_2_Core.rawValue:     "kCGLOGLPVersion_3_2_Core",
        kCGLOGLPVersion_Legacy.rawValue:       "kCGLOGLPVersion_Legacy",
        kCGLPFAOpenGLProfile.rawValue:         "kCGLPFAOpenGLProfile",
        UInt32(kCGLRendererGenericFloatID):    "kCGLRendererGenericFloatID",
        kCGLPFARendererID.rawValue:            "kCGLPFARendererID",
        kCGLPFAAccelerated.rawValue:           "kCGLPFAAccelerated",
        kCGLPFADoubleBuffer.rawValue:          "kCGLPFADoubleBuffer",
        kCGLPFABackingStore.rawValue:          "kCGLPFABackingStore",
        kCGLPFAAllowOfflineRenderers.rawValue: "kCGLPFAAllowOfflineRenderers",
        kCGLPFASupportsAutomaticGraphicsSwitching.rawValue: "kCGLPFASupportsAutomaticGraphicsSwitching",
    ]

    var needsICCUpdate: Bool = false {
        didSet {
            if needsICCUpdate == true {
                update()
            }
        }
    }

    var inLiveResize: Bool = false {
        didSet {
            if inLiveResize {
                isAsynchronous = true
            }
            update(force: true)
        }
    }

    init(cocoaCB ccb: CocoaCB) {
        cocoaCB = ccb
        super.init()
        autoresizingMask = [.layerWidthSizable, .layerHeightSizable]
        backgroundColor = NSColor.black.cgColor

        cglPixelFormat = copyCGLPixelFormat(forDisplayMask: 0)
        CGLCreateContext(cglPixelFormat!, nil, &cglContext)
        var i: GLint = 1
        CGLSetParameter(cglContext!, kCGLCPSwapInterval, &i)
        CGLSetCurrentContext(cglContext!)

        mpv.initRender()
        mpv.setRenderUpdateCallback(updateCallback, context: self)
        mpv.setRenderControlCallback(cocoaCB.controlCallback, context: cocoaCB)
    }

    override init(layer: Any) {
        let oldLayer = layer as! VideoLayer
        cocoaCB = oldLayer.cocoaCB
        super.init()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func canDraw(inCGLContext ctx: CGLContextObj,
                          pixelFormat pf: CGLPixelFormatObj,
                          forLayerTime t: CFTimeInterval,
                          displayTime ts: UnsafePointer<CVTimeStamp>?) -> Bool {
        if inLiveResize == false {
            isAsynchronous = false
        }
        return mpv != nil && cocoaCB.backendState == .initialized &&
               (forceDraw || mpv.isRenderUpdateFrame())
    }

    override func draw(inCGLContext ctx: CGLContextObj,
                       pixelFormat pf: CGLPixelFormatObj,
                       forLayerTime t: CFTimeInterval,
                       displayTime ts: UnsafePointer<CVTimeStamp>?) {
        needsFlip = false
        forceDraw = false

        if draw.rawValue >= Draw.atomic.rawValue {
             if draw == .atomic {
                draw = .atomicEnd
             } else {
                atomicDrawingEnd()
             }
        }

        updateSurfaceSize()
        mpv.drawRender(surfaceSize!, ctx)

        if needsICCUpdate {
            needsICCUpdate = false
            cocoaCB.updateICCProfile()
        }
    }

    func updateSurfaceSize() {
        var dims: [GLint] = [0, 0, 0, 0]
        glGetIntegerv(GLenum(GL_VIEWPORT), &dims)
        surfaceSize = NSMakeSize(CGFloat(dims[2]), CGFloat(dims[3]))

        if NSEqualSizes(surfaceSize!, NSZeroSize) {
            surfaceSize = bounds.size
            surfaceSize!.width *= contentsScale
            surfaceSize!.height *= contentsScale
        }
    }

    func atomicDrawingStart() {
        if draw == .normal {
            NSDisableScreenUpdates()
            draw = .atomic
        }
    }

    func atomicDrawingEnd() {
        if draw.rawValue >= Draw.atomic.rawValue {
            NSEnableScreenUpdates()
            draw = .normal
        }
    }

    override func copyCGLPixelFormat(forDisplayMask mask: UInt32) -> CGLPixelFormatObj {
        if cglPixelFormat != nil { return cglPixelFormat! }

        var pix: CGLPixelFormatObj?
        var err: CGLError = CGLError(rawValue: 0)

        if mpv.macOpts!.cocoa_cb_sw_renderer != 1 {
            (pix, err) = createPixelFormat()
        }

        if (err != kCGLNoError || pix == nil) && mpv.macOpts!.cocoa_cb_sw_renderer != 0 {
            (pix, err) = createPixelFormat(software: true)
        }

        if err != kCGLNoError || pix == nil {
            mpv.sendError("Couldn't create any CGL pixel format")
            exit(1)
        }

        return pix!
    }

    func createPixelFormat(software: Bool = false) -> (CGLPixelFormatObj?, CGLError) {
        var pix: CGLPixelFormatObj?
        var err: CGLError = CGLError(rawValue: 0)
        var npix: GLint = 0

        for ver in glVersions {
            var glBase = software ? glFormatSoftwareBase : glFormatBase
            glBase.insert(CGLPixelFormatAttribute(ver.rawValue), at: 1)
            var glFormat = glBase + glFormatOptional

            for index in stride(from: glFormat.count-1, through: glBase.count-1, by: -1) {
                let format = glFormat + [_CGLPixelFormatAttribute(rawValue: 0)]
                err = CGLChoosePixelFormat(format, &pix, &npix)

                if err == kCGLBadAttribute || err == kCGLBadPixelFormat || pix == nil {
                    glFormat.remove(at: index)
                } else {
                    let attArray = glFormat.map({ (value: _CGLPixelFormatAttribute) -> String in
                        return attributeLookUp[value.rawValue]!
                    })

                    mpv.sendVerbose("Created CGL pixel format with attributes: " +
                                    "\(attArray.joined(separator: ", "))")
                    return (pix, err)
                }
            }
        }

        let errS = String(cString: CGLErrorString(err))
        mpv.sendWarning("Couldn't create a " +
                        "\(software ? "software" : "hardware accelerated") " +
                        "CGL pixel format: \(errS) (\(err.rawValue))")

        if software == false && mpv.macOpts!.cocoa_cb_sw_renderer == -1 {
            mpv.sendWarning("Falling back to software renderer")
        }

        return (pix, err)
    }

    override func copyCGLContext(forPixelFormat pf: CGLPixelFormatObj) -> CGLContextObj {
        contentsScale = cocoaCB.window.backingScaleFactor
        return cglContext!
    }

    let updateCallback: mpv_render_update_fn = { (ctx) in
        let layer: VideoLayer = MPVHelper.bridge(ptr: ctx!)
        layer.update()
    }

    override func display() {
        displayLock.lock()
        let isUpdate = needsFlip
        super.display()
        CATransaction.flush()
        if isUpdate && needsFlip {
            CGLSetCurrentContext(cglContext!)
            if mpv.isRenderUpdateFrame() {
                mpv.drawRender(NSZeroSize, cglContext!, skip: true)
            }
        }
        displayLock.unlock()
    }

    func update(force: Bool = false) {
        if force { forceDraw = true }
        queue.async {
            if self.forceDraw || !self.inLiveResize {
                self.needsFlip = true
                self.display()
            }
        }
    }

}
