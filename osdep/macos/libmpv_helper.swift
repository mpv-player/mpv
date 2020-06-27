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
    var mpvHandle: OpaquePointer?
    var mpvRenderContext: OpaquePointer?
    var macOptsPtr: UnsafeMutableRawPointer?
    var macOpts: macos_opts = macos_opts()
    var fbo: GLint = 1
    let deinitLock = NSLock()

    init(_ mpv: OpaquePointer, _ mpLog: OpaquePointer?) {
        mpvHandle = mpv
        log = LogHelper(mpLog)

        guard let app = NSApp as? Application,
              let ptr = mp_get_config_group(nil,
                                            mp_client_get_global(mpvHandle),
                                            app.getMacOSConf()) else
        {
            log.sendError("macOS config group couldn't be retrieved'")
            exit(1)
        }
        macOptsPtr = ptr
        macOpts = UnsafeMutablePointer<macos_opts>(OpaquePointer(ptr)).pointee
    }

    func initRender() {
        var advanced: CInt = 1
        let api = UnsafeMutableRawPointer(mutating: (MPV_RENDER_API_TYPE_OPENGL as NSString).utf8String)
        var pAddress = mpv_opengl_init_params(get_proc_address: getProcAddress,
                                              get_proc_address_ctx: nil,
                                              extra_exts: nil)
        var params: [mpv_render_param] = [
            mpv_render_param(type: MPV_RENDER_PARAM_API_TYPE, data: api),
            mpv_render_param(type: MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, data: &pAddress),
            mpv_render_param(type: MPV_RENDER_PARAM_ADVANCED_CONTROL, data: &advanced),
            mpv_render_param()
        ]

        if (mpv_render_context_create(&mpvRenderContext, mpvHandle, &params) < 0)
        {
            log.sendError("Render context init has failed.")
            exit(1)
        }
    }

    let getProcAddress: (@convention(c) (UnsafeMutableRawPointer?, UnsafePointer<Int8>?)
                        -> UnsafeMutableRawPointer?) =
    {
        (ctx: UnsafeMutableRawPointer?, name: UnsafePointer<Int8>?)
                        -> UnsafeMutableRawPointer? in
        let symbol: CFString = CFStringCreateWithCString(
                                kCFAllocatorDefault, name, kCFStringEncodingASCII)
        let indentifier = CFBundleGetBundleWithIdentifier("com.apple.opengl" as CFString)
        let addr = CFBundleGetFunctionPointerForName(indentifier, symbol)

        if symbol as String == "glFlush" {
            return unsafeBitCast(glDummy, to: UnsafeMutableRawPointer.self)
        }

        return addr
    }

    func setRenderUpdateCallback(_ callback: @escaping mpv_render_update_fn, context object: AnyObject) {
        if mpvRenderContext == nil {
            log.sendWarning("Init mpv render context first.")
        } else {
            mpv_render_context_set_update_callback(mpvRenderContext, callback, MPVHelper.bridge(obj: object))
        }
    }

    func setRenderControlCallback(_ callback: @escaping mp_render_cb_control_fn, context object: AnyObject) {
        if mpvRenderContext == nil {
            log.sendWarning("Init mpv render context first.")
        } else {
            mp_render_context_set_control_callback(mpvRenderContext, callback, MPVHelper.bridge(obj: object))
        }
    }

    func reportRenderFlip() {
        if mpvRenderContext == nil { return }
        mpv_render_context_report_swap(mpvRenderContext)
    }

    func isRenderUpdateFrame() -> Bool {
        deinitLock.lock()
        if mpvRenderContext == nil {
            deinitLock.unlock()
            return false
        }
        let flags: UInt64 = mpv_render_context_update(mpvRenderContext)
        deinitLock.unlock()
        return flags & UInt64(MPV_RENDER_UPDATE_FRAME.rawValue) > 0
    }

    func drawRender(_ surface: NSSize, _ depth: GLint, _ ctx: CGLContextObj, skip: Bool = false) {
        deinitLock.lock()
        if mpvRenderContext != nil {
            var i: GLint = 0
            var flip: CInt = 1
            var skip: CInt = skip ? 1 : 0
            var ditherDepth = depth
            glGetIntegerv(GLenum(GL_DRAW_FRAMEBUFFER_BINDING), &i)
            // CAOpenGLLayer has ownership of FBO zero yet can return it to us,
            // so only utilize a newly received FBO ID if it is nonzero.
            fbo = i != 0 ? i : fbo

            var data = mpv_opengl_fbo(fbo: Int32(fbo),
                                        w: Int32(surface.width),
                                        h: Int32(surface.height),
                          internal_format: 0)
            var params: [mpv_render_param] = [
                mpv_render_param(type: MPV_RENDER_PARAM_OPENGL_FBO, data: &data),
                mpv_render_param(type: MPV_RENDER_PARAM_FLIP_Y, data: &flip),
                mpv_render_param(type: MPV_RENDER_PARAM_DEPTH, data: &ditherDepth),
                mpv_render_param(type: MPV_RENDER_PARAM_SKIP_RENDERING, data: &skip),
                mpv_render_param()
            ]
            mpv_render_context_render(mpvRenderContext, &params);
        } else {
            glClearColor(0, 0, 0, 1)
            glClear(GLbitfield(GL_COLOR_BUFFER_BIT))
        }

        if !skip { CGLFlushDrawable(ctx) }

        deinitLock.unlock()
    }

    func setRenderICCProfile(_ profile: NSColorSpace) {
        if mpvRenderContext == nil { return }
        guard var iccData = profile.iccProfileData else {
            log.sendWarning("Invalid ICC profile data.")
            return
        }
        iccData.withUnsafeMutableBytes { (ptr: UnsafeMutableRawBufferPointer) in
            guard let baseAddress = ptr.baseAddress, ptr.count > 0 else { return }

            let u8Ptr = baseAddress.assumingMemoryBound(to: UInt8.self)
            let iccBstr = bstrdup(nil, bstr(start: u8Ptr, len: ptr.count))
            var icc = mpv_byte_array(data: iccBstr.start, size: iccBstr.len)
            let params = mpv_render_param(type: MPV_RENDER_PARAM_ICC_PROFILE, data: &icc)
            mpv_render_context_set_parameter(mpvRenderContext, params)
        }
    }

    func setRenderLux(_ lux: Int) {
        if mpvRenderContext == nil { return }
        var light = lux
        let params = mpv_render_param(type: MPV_RENDER_PARAM_AMBIENT_LIGHT, data: &light)
        mpv_render_context_set_parameter(mpvRenderContext, params)
    }

    func commandAsync(_ cmd: [String?], id: UInt64 = 1) {
        if mpvHandle == nil { return }
        var mCmd = cmd
        mCmd.append(nil)
        var cargs = mCmd.map { $0.flatMap { UnsafePointer<Int8>(strdup($0)) } }
        mpv_command_async(mpvHandle, id, &cargs)
        for ptr in cargs { free(UnsafeMutablePointer(mutating: ptr)) }
    }

    func observeString(_ property: String) {
        mpv_observe_property(mpvHandle, 0, property, MPV_FORMAT_STRING)
    }

    func observeFlag(_ property: String) {
        mpv_observe_property(mpvHandle, 0, property, MPV_FORMAT_FLAG)
    }

    // Unsafe function when called while using the render API
    func command(_ cmd: String) {
        if mpvHandle == nil { return }
        mpv_command_string(mpvHandle, cmd)
    }

    func getBoolProperty(_ name: String) -> Bool {
        if mpvHandle == nil { return false }
        var value = Int32()
        mpv_get_property(mpvHandle, name, MPV_FORMAT_FLAG, &value)
        return value > 0
    }

    func getIntProperty(_ name: String) -> Int {
        if mpvHandle == nil { return 0 }
        var value = Int64()
        mpv_get_property(mpvHandle, name, MPV_FORMAT_INT64, &value)
        return Int(value)
    }

    func getStringProperty(_ name: String) -> String? {
        guard let mpv = mpvHandle else { return nil }
        guard let value = mpv_get_property_string(mpv, name) else { return nil }
        let str = String(cString: value)
        mpv_free(value)
        return str
    }

    func deinitRender() {
        mpv_render_context_set_update_callback(mpvRenderContext, nil, nil)
        mp_render_context_set_control_callback(mpvRenderContext, nil, nil)
        deinitLock.lock()
        mpv_render_context_free(mpvRenderContext)
        mpvRenderContext = nil
        deinitLock.unlock()
    }

    func deinitMPV(_ destroy: Bool = false) {
        if destroy {
            mpv_destroy(mpvHandle)
        }
        ta_free(macOptsPtr)
        macOptsPtr = nil
        mpvHandle = nil
    }

    // *(char **) MPV_FORMAT_STRING on mpv_event_property
    class func mpvStringArrayToString(_ obj: UnsafeMutableRawPointer) -> String? {
        let cstr = UnsafeMutablePointer<UnsafeMutablePointer<Int8>>(OpaquePointer(obj))
        return String(cString: cstr[0])
    }

    // MPV_FORMAT_FLAG
    class func mpvFlagToBool(_ obj: UnsafeMutableRawPointer) -> Bool? {
        return UnsafePointer<Bool>(OpaquePointer(obj))?.pointee
    }
}
