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

extension Bool {
    init(_ num: Int32) {
        self.init(num > 0)
    }
}

class MPVHelper: NSObject {

    var mpvHandle: OpaquePointer?
    var mpvRenderContext: OpaquePointer?
    var mpvLog: OpaquePointer?
    var inputContext: OpaquePointer?
    var mpctx: UnsafeMutablePointer<MPContext>?
    var macOpts: macos_opts?
    var fbo: GLint = 1

    init(_ mpv: OpaquePointer) {
        super.init()
        mpvHandle = mpv
        mpvLog = mp_log_new(UnsafeMutablePointer<MPContext>(mpvHandle),
                            mp_client_get_log(mpvHandle), "cocoacb")
        mpctx = UnsafeMutablePointer<MPContext>(mp_client_get_core(mpvHandle))
        inputContext = mpctx!.pointee.input

        if let app = NSApp as? Application {
            let ptr = mp_get_config_group(mpctx!, mp_client_get_global(mpvHandle),
                                          app.getMacOSConf())
            macOpts = UnsafeMutablePointer<macos_opts>(OpaquePointer(ptr))!.pointee
        }

        mpv_observe_property(mpvHandle, 0, "ontop", MPV_FORMAT_FLAG)
        mpv_observe_property(mpvHandle, 0, "border", MPV_FORMAT_FLAG)
        mpv_observe_property(mpvHandle, 0, "keepaspect-window", MPV_FORMAT_FLAG)
        mpv_observe_property(mpvHandle, 0, "macos-title-bar-style", MPV_FORMAT_STRING)
    }

    func initRender() {
        let api = UnsafeMutableRawPointer(mutating: (MPV_RENDER_API_TYPE_OPENGL as NSString).utf8String)
        var pAddress = mpv_opengl_init_params(get_proc_address: getProcAddress,
                                              get_proc_address_ctx: nil,
                                              extra_exts: nil)
        var params: [mpv_render_param] = [
            mpv_render_param(type: MPV_RENDER_PARAM_API_TYPE, data: api),
            mpv_render_param(type: MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, data: &pAddress),
            mpv_render_param()
        ]

        if (mpv_render_context_create(&mpvRenderContext, mpvHandle, &params) < 0)
        {
            sendError("Render context init has failed.")
            exit(1)
        }
    }

    let getProcAddress: (@convention(c) (UnsafeMutableRawPointer?, UnsafePointer<Int8>?)
                        -> UnsafeMutableRawPointer?)! =
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
            sendWarning("Init mpv render context first.")
        } else {
            mpv_render_context_set_update_callback(mpvRenderContext, callback, MPVHelper.bridge(obj: object))
        }
    }

    func setRenderControlCallback(_ callback: @escaping mp_render_cb_control_fn, context object: AnyObject) {
        if mpvRenderContext == nil {
            sendWarning("Init mpv render context first.")
        } else {
            mp_render_context_set_control_callback(mpvRenderContext, callback, MPVHelper.bridge(obj: object))
        }
    }

    func reportRenderFlip() {
        if mpvRenderContext == nil { return }
            mpv_render_context_report_swap(mpvRenderContext)
    }

    func drawRender(_ surface: NSSize) {
        if mpvRenderContext != nil {
            var i: GLint = 0
            var flip: CInt = 1
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
                mpv_render_param()
            ]
            mpv_render_context_render(mpvRenderContext, &params);
        } else {
            glClearColor(0, 0, 0, 1)
            glClear(GLbitfield(GL_COLOR_BUFFER_BIT))
        }
    }

    func setRenderICCProfile(_ profile: NSColorSpace) {
        if mpvRenderContext == nil { return }
        guard var iccData = profile.iccProfileData else {
            sendWarning("Invalid ICC profile data.")
            return
        }
        let iccSize = iccData.count
        iccData.withUnsafeMutableBytes { (u8Ptr: UnsafeMutablePointer<UInt8>) in
            let iccBstr = bstrdup(nil, bstr(start: u8Ptr, len: iccSize))
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

    func command(_ cmd: String) {
        if mpvHandle == nil { return }
        mpv_command_string(mpvHandle, cmd)
    }

    func commandAsync(_ cmd: [String?], id: UInt64 = 1) {
        if mpvHandle == nil { return }
        var mCmd = cmd
        mCmd.append(nil)
        var cargs = mCmd.map { $0.flatMap { UnsafePointer<Int8>(strdup($0)) } }
        mpv_command_async(mpvHandle, id, &cargs)
        for ptr in cargs { free(UnsafeMutablePointer(mutating: ptr)) }
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
        if mpvHandle == nil { return nil }
        let value = mpv_get_property_string(mpvHandle, name)
        let str = value == nil ? nil : String(cString: value!)
        mpv_free(value)
        return str
    }

    func canBeDraggedAt(_ pos: NSPoint) -> Bool {
        if inputContext == nil { return false }
        let canDrag = !mp_input_test_dragging(inputContext!, Int32(pos.x), Int32(pos.y))
        return canDrag
    }

    func setMousePosition(_ pos: NSPoint) {
        if inputContext == nil { return }
        mp_input_set_mouse_pos(inputContext!, Int32(pos.x), Int32(pos.y))
    }

    func putAxis(_ mpkey: Int32, delta: Double) {
        if inputContext == nil { return }
        mp_input_put_wheel(inputContext!, mpkey, delta)
    }

    func sendVerbose(_ msg: String) {
        send(message: msg, type: MSGL_V)
    }

    func sendInfo(_ msg: String) {
        send(message: msg, type: MSGL_INFO)
    }

    func sendWarning(_ msg: String) {
        send(message: msg, type: MSGL_WARN)
    }

    func sendError(_ msg: String) {
        send(message: msg, type: MSGL_ERR)
    }

    func send(message msg: String, type t: Int) {
        if mpvLog == nil {
            sendFallback(message: msg, type: t)
        } else {
            let args: [CVarArg] = [ (msg as NSString).utf8String! ]
            mp_msg_va(mpvLog, Int32(t), "%s\n", getVaList(args))
        }
    }

    func sendFallback(message msg: String, type t: Int) {
        var level = "\u{001B}"
        switch t {
        case MSGL_V:
            level += "[0;30m[VERBOSE]"
        case MSGL_INFO:
            level += "[0;30m[INFO]"
        case MSGL_WARN:
            level += "[0;33m"
        case MSGL_ERR:
            level += "[0;31m"
        default:
            level += "[0;30m"
        }

        print("\(level)[osx/cocoacb] \(msg)\u{001B}[0;30m")
    }

    func deinitRender() {
        mpv_render_context_set_update_callback(mpvRenderContext, nil, nil)
        mp_render_context_set_control_callback(mpvRenderContext, nil, nil)
        mpv_render_context_free(mpvRenderContext)
        mpvRenderContext = nil
    }

    func deinitMPV(_ destroy: Bool = false) {
        if destroy {
            mpv_destroy(mpvHandle)
        }
        mpvHandle = nil
        mpvLog = nil
        inputContext = nil
        mpctx = nil
    }

    // (__bridge void*)
    class func bridge<T: AnyObject>(obj: T) -> UnsafeMutableRawPointer {
        return UnsafeMutableRawPointer(Unmanaged.passUnretained(obj).toOpaque())
    }

    // (__bridge T*)
    class func bridge<T: AnyObject>(ptr: UnsafeRawPointer) -> T {
        return Unmanaged<T>.fromOpaque(ptr).takeUnretainedValue()
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
