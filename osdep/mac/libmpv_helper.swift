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

let glDummy: @convention(c) () -> Void = {}

class LibmpvHelper {
    var log: LogHelper
    var mpv: OpaquePointer?
    var mpvRenderContext: OpaquePointer?
    var fbo: GLint = 1
    let uninitLock = NSLock()

    init(_ mpv: OpaquePointer, _ log: LogHelper) {
        self.mpv = mpv
        self.log = log
    }

    func initRender() {
        let advanced: CInt = 1
        let api = UnsafeMutableRawPointer(mutating: (MPV_RENDER_API_TYPE_OPENGL as NSString).utf8String)
        let pAddress = mpv_opengl_init_params(get_proc_address: getProcAddress,
                                              get_proc_address_ctx: nil)

        TypeHelper.withUnsafeMutableRawPointers([pAddress, advanced]) { (pointers: [UnsafeMutableRawPointer?]) in
            var params: [mpv_render_param] = [
                mpv_render_param(type: MPV_RENDER_PARAM_API_TYPE, data: api),
                mpv_render_param(type: MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, data: pointers[0]),
                mpv_render_param(type: MPV_RENDER_PARAM_ADVANCED_CONTROL, data: pointers[1]),
                mpv_render_param()
            ]

            if mpv_render_context_create(&mpvRenderContext, mpv, &params) < 0 {
                log.error("Render context init has failed.")
                exit(1)
            }
        }
    }

    let getProcAddress: (@convention(c) (UnsafeMutableRawPointer?, UnsafePointer<Int8>?)
        -> UnsafeMutableRawPointer?) = { (_ ctx: UnsafeMutableRawPointer?, name: UnsafePointer<Int8>?) -> UnsafeMutableRawPointer? in
        let symbol: CFString = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII)
        let identifier = CFBundleGetBundleWithIdentifier("com.apple.opengl" as CFString)
        let addr = CFBundleGetFunctionPointerForName(identifier, symbol)

        if symbol as String == "glFlush" {
            return unsafeBitCast(glDummy, to: UnsafeMutableRawPointer.self)
        }

        return addr
    }

    func setRenderUpdateCallback(_ callback: @escaping mpv_render_update_fn, context object: AnyObject) {
        if mpvRenderContext == nil {
            log.warning("Init mpv render context first.")
        } else {
            mpv_render_context_set_update_callback(mpvRenderContext, callback, TypeHelper.bridge(obj: object))
        }
    }

    func setRenderControlCallback(_ callback: @escaping mp_render_cb_control_fn, context object: AnyObject) {
        if mpvRenderContext == nil {
            log.warning("Init mpv render context first.")
        } else {
            mp_render_context_set_control_callback(mpvRenderContext, callback, TypeHelper.bridge(obj: object))
        }
    }

    func reportRenderFlip() {
        if mpvRenderContext == nil { return }
        mpv_render_context_report_swap(mpvRenderContext)
    }

    func isRenderUpdateFrame() -> Bool {
        uninitLock.lock()
        if mpvRenderContext == nil {
            uninitLock.unlock()
            return false
        }
        let flags: UInt64 = mpv_render_context_update(mpvRenderContext)
        uninitLock.unlock()
        return flags & UInt64(MPV_RENDER_UPDATE_FRAME.rawValue) > 0
    }

    func drawRender(_ surface: NSSize, _ depth: GLint, _ ctx: CGLContextObj, skip: Bool = false) {
        uninitLock.lock()
        if mpvRenderContext != nil {
            var i: GLint = 0
            let flip: CInt = 1
            let skip: CInt = skip ? 1 : 0
            let ditherDepth = depth
            glGetIntegerv(GLenum(GL_DRAW_FRAMEBUFFER_BINDING), &i)
            // CAOpenGLLayer has ownership of FBO zero yet can return it to us,
            // so only utilize a newly received FBO ID if it is nonzero.
            fbo = i != 0 ? i : fbo

            let data = mpv_opengl_fbo(fbo: Int32(fbo),
                                        w: Int32(surface.width),
                                        h: Int32(surface.height),
                          internal_format: 0)

            TypeHelper.withUnsafeMutableRawPointers([data, flip, ditherDepth, skip]) { (pointers: [UnsafeMutableRawPointer?]) in
                var params: [mpv_render_param] = [
                    mpv_render_param(type: MPV_RENDER_PARAM_OPENGL_FBO, data: pointers[0]),
                    mpv_render_param(type: MPV_RENDER_PARAM_FLIP_Y, data: pointers[1]),
                    mpv_render_param(type: MPV_RENDER_PARAM_DEPTH, data: pointers[2]),
                    mpv_render_param(type: MPV_RENDER_PARAM_SKIP_RENDERING, data: pointers[3]),
                    mpv_render_param()
                ]
                mpv_render_context_render(mpvRenderContext, &params)
            }
        } else {
            glClearColor(0, 0, 0, 1)
            glClear(GLbitfield(GL_COLOR_BUFFER_BIT))
        }

        if !skip { CGLFlushDrawable(ctx) }

        uninitLock.unlock()
    }

    func setRenderICCProfile(_ profile: NSColorSpace) {
        if mpvRenderContext == nil { return }
        guard var iccData = profile.iccProfileData else {
            log.warning("Invalid ICC profile data.")
            return
        }
        iccData.withUnsafeMutableBytes { (ptr: UnsafeMutableRawBufferPointer) in
            guard let baseAddress = ptr.baseAddress, ptr.count > 0 else { return }

            let u8Ptr = baseAddress.assumingMemoryBound(to: UInt8.self)
            let iccBstr = bstrdup(nil, bstr(start: u8Ptr, len: ptr.count))
            var icc = mpv_byte_array(data: iccBstr.start, size: iccBstr.len)
            withUnsafeMutableBytes(of: &icc) { (ptr: UnsafeMutableRawBufferPointer) in
                let params = mpv_render_param(type: MPV_RENDER_PARAM_ICC_PROFILE, data: ptr.baseAddress)
                mpv_render_context_set_parameter(mpvRenderContext, params)
            }
        }
    }

    func setRenderLux(_ lux: Int) {
        if mpvRenderContext == nil { return }
        var light = lux
        withUnsafeMutableBytes(of: &light) { (ptr: UnsafeMutableRawBufferPointer) in
            let params = mpv_render_param(type: MPV_RENDER_PARAM_AMBIENT_LIGHT, data: ptr.baseAddress)
            mpv_render_context_set_parameter(mpvRenderContext, params)
        }
    }

    func uninit() {
        mpv_render_context_set_update_callback(mpvRenderContext, nil, nil)
        mp_render_context_set_control_callback(mpvRenderContext, nil, nil)
        uninitLock.lock()
        mpv_render_context_free(mpvRenderContext)
        mpvRenderContext = nil
        mpv_destroy(mpv)
        mpv = nil
        uninitLock.unlock()
    }
}
