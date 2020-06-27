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

let glFormatOptional: [[CGLPixelFormatAttribute]] = [
    [kCGLPFABackingStore],
    [kCGLPFAAllowOfflineRenderers]
]

let glFormat10Bit: [CGLPixelFormatAttribute] = [
    kCGLPFAColorSize,
    _CGLPixelFormatAttribute(rawValue: 64),
    kCGLPFAColorFloat
]

let glFormatAutoGPU: [CGLPixelFormatAttribute] = [
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
    kCGLPFAColorSize.rawValue:             "kCGLPFAColorSize",
    kCGLPFAColorFloat.rawValue:            "kCGLPFAColorFloat",
    kCGLPFAAllowOfflineRenderers.rawValue: "kCGLPFAAllowOfflineRenderers",
    kCGLPFASupportsAutomaticGraphicsSwitching.rawValue: "kCGLPFASupportsAutomaticGraphicsSwitching",
]

class GLLayer: CAOpenGLLayer {
    unowned var cocoaCB: CocoaCB
    var libmpv: LibmpvHelper { get { return cocoaCB.libmpv } }

    let displayLock = NSLock()
    let cglContext: CGLContextObj
    let cglPixelFormat: CGLPixelFormatObj
    var needsFlip: Bool = false
    var forceDraw: Bool = false
    var surfaceSize: NSSize = NSSize(width: 0, height: 0)
    var bufferDepth: GLint = 8

    enum Draw: Int { case normal = 1, atomic, atomicEnd }
    var draw: Draw = .normal

    let queue: DispatchQueue = DispatchQueue(label: "io.mpv.queue.draw")

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
        (cglPixelFormat, bufferDepth) = GLLayer.createPixelFormat(ccb)
        cglContext = GLLayer.createContext(ccb, cglPixelFormat)
        super.init()
        autoresizingMask = [.layerWidthSizable, .layerHeightSizable]
        backgroundColor = NSColor.black.cgColor

        if #available(macOS 10.12, *), bufferDepth > 8 {
            contentsFormat = .RGBA16Float
        }

        var i: GLint = 1
        CGLSetParameter(cglContext, kCGLCPSwapInterval, &i)
        CGLSetCurrentContext(cglContext)

        libmpv.initRender()
        libmpv.setRenderUpdateCallback(updateCallback, context: self)
        libmpv.setRenderControlCallback(cocoaCB.controlCallback, context: cocoaCB)
    }

    // necessary for when the layer containing window changes the screen
    override init(layer: Any) {
        guard let oldLayer = layer as? GLLayer else {
            fatalError("init(layer: Any) passed an invalid layer")
        }
        cocoaCB = oldLayer.cocoaCB
        surfaceSize = oldLayer.surfaceSize
        cglPixelFormat = oldLayer.cglPixelFormat
        cglContext = oldLayer.cglContext
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
        return cocoaCB.backendState == .initialized &&
               (forceDraw || libmpv.isRenderUpdateFrame())
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
        libmpv.drawRender(surfaceSize, bufferDepth, ctx)

        if needsICCUpdate {
            needsICCUpdate = false
            cocoaCB.updateICCProfile()
        }
    }

    func updateSurfaceSize() {
        var dims: [GLint] = [0, 0, 0, 0]
        glGetIntegerv(GLenum(GL_VIEWPORT), &dims)
        surfaceSize = NSSize(width: CGFloat(dims[2]), height: CGFloat(dims[3]))

        if NSEqualSizes(surfaceSize, NSZeroSize) {
            surfaceSize = bounds.size
            surfaceSize.width *= contentsScale
            surfaceSize.height *= contentsScale
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
        return cglPixelFormat
    }

    override func copyCGLContext(forPixelFormat pf: CGLPixelFormatObj) -> CGLContextObj {
        contentsScale = cocoaCB.window?.backingScaleFactor ?? 1.0
        return cglContext
    }

    let updateCallback: mpv_render_update_fn = { (ctx) in
        let layer: GLLayer = unsafeBitCast(ctx, to: GLLayer.self)
        layer.update()
    }

    override func display() {
        displayLock.lock()
        let isUpdate = needsFlip
        super.display()
        CATransaction.flush()
        if isUpdate && needsFlip {
            CGLSetCurrentContext(cglContext)
            if libmpv.isRenderUpdateFrame() {
                libmpv.drawRender(NSZeroSize, bufferDepth, cglContext, skip: true)
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

    class func createPixelFormat(_ ccb: CocoaCB) -> (CGLPixelFormatObj, GLint) {
        var pix: CGLPixelFormatObj?
        var depth: GLint = 8
        var err: CGLError = CGLError(rawValue: 0)
        let swRender = ccb.libmpv.macOpts.cocoa_cb_sw_renderer

        if swRender != 1 {
            (pix, depth, err) = GLLayer.findPixelFormat(ccb)
        }

        if (err != kCGLNoError || pix == nil) && swRender != 0 {
            (pix, depth, err) = GLLayer.findPixelFormat(ccb, software: true)
        }

        guard let pixelFormat = pix, err == kCGLNoError else {
            ccb.log.sendError("Couldn't create any CGL pixel format")
            exit(1)
        }

        return (pixelFormat, depth)
    }

    class func findPixelFormat(_ ccb: CocoaCB, software: Bool = false) -> (CGLPixelFormatObj?, GLint, CGLError) {
        var pix: CGLPixelFormatObj?
        var err: CGLError = CGLError(rawValue: 0)
        var npix: GLint = 0

        for ver in glVersions {
            var glBase = software ? glFormatSoftwareBase : glFormatBase
            glBase.insert(CGLPixelFormatAttribute(ver.rawValue), at: 1)

            var glFormat = [glBase]
            if (ccb.libmpv.macOpts.cocoa_cb_10bit_context == 1) {
                glFormat += [glFormat10Bit]
            }
            glFormat += glFormatOptional

            if (ccb.libmpv.macOpts.macos_force_dedicated_gpu == 0) {
                glFormat += [glFormatAutoGPU]
            }

            for index in stride(from: glFormat.count-1, through: 0, by: -1) {
                let format = glFormat.flatMap { $0 } + [_CGLPixelFormatAttribute(rawValue: 0)]
                err = CGLChoosePixelFormat(format, &pix, &npix)

                if err == kCGLBadAttribute || err == kCGLBadPixelFormat || pix == nil {
                    glFormat.remove(at: index)
                } else {
                    let attArray = format.map({ (value: _CGLPixelFormatAttribute) -> String in
                        return attributeLookUp[value.rawValue] ?? String(value.rawValue)
                    })

                    ccb.log.sendVerbose("Created CGL pixel format with attributes: " +
                                    "\(attArray.joined(separator: ", "))")
                    return (pix, glFormat.contains(glFormat10Bit) ? 16 : 8, err)
                }
            }
        }

        let errS = String(cString: CGLErrorString(err))
        ccb.log.sendWarning("Couldn't create a " +
                           "\(software ? "software" : "hardware accelerated") " +
                           "CGL pixel format: \(errS) (\(err.rawValue))")
        if software == false && ccb.libmpv.macOpts.cocoa_cb_sw_renderer == -1 {
            ccb.log.sendWarning("Falling back to software renderer")
        }

        return (pix, 8, err)
    }

    class func createContext(_ ccb: CocoaCB, _ pixelFormat: CGLPixelFormatObj) -> CGLContextObj {
        var context: CGLContextObj?
        let error = CGLCreateContext(pixelFormat, nil, &context)

        guard let cglContext = context, error == kCGLNoError else {
            let errS = String(cString: CGLErrorString(error))
            ccb.log.sendError("Couldn't create a CGLContext: " + errS)
            exit(1)
        }

        return cglContext
    }
}
