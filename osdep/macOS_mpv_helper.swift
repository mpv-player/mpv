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

class MPVHelper: NSObject {

    var mpvHandle: OpaquePointer?
    var mpvGLCBContext: OpaquePointer?
    var mpvLog: OpaquePointer?
    var inputContext: OpaquePointer?
    var mpctx: UnsafeMutablePointer<MPContext>?
    var fbo: GLint = 1

    init(_ mpv: OpaquePointer) {
        super.init()
        mpvHandle = mpv
        mpvLog = mp_log_new(UnsafeMutablePointer<MPContext>(mpvHandle),
                            mp_client_get_log(mpvHandle), "cocoacb")
        mpctx = UnsafeMutablePointer<MPContext>(mp_client_get_core(mpvHandle))
        inputContext = mpctx!.pointee.input

        mpv_observe_property(mpvHandle, 0, "ontop", MPV_FORMAT_FLAG)
        mpv_observe_property(mpvHandle, 0, "border", MPV_FORMAT_FLAG)
        mpv_observe_property(mpvHandle, 0, "keepaspect-window", MPV_FORMAT_FLAG)
    }

    func setGLCB() {
        if mpvHandle == nil {
            sendError("No mpv handle available.")
            exit(1)
        }
        mpvGLCBContext = OpaquePointer(mp_get_sub_api2(mpvHandle, MPV_SUB_API_OPENGL_CB, false))
        if mpvGLCBContext == nil {
            sendError("libmpv does not have the opengl-cb sub-API.")
            exit(1)
        }
    }

    func initGLCB() {
        if mpvGLCBContext == nil {
            setGLCB()
        }
        if mpv_opengl_cb_init_gl(mpvGLCBContext, nil, getProcAddress, nil) < 0 {
            sendError("GL init has failed.")
            exit(1)
        }
    }

    let getProcAddress: mpv_opengl_cb_get_proc_address_fn = {
            (ctx: UnsafeMutableRawPointer?, name: UnsafePointer<Int8>?) -> UnsafeMutableRawPointer? in
        let symbol: CFString = CFStringCreateWithCString(
                                kCFAllocatorDefault, name, kCFStringEncodingASCII)
        let indentifier = CFBundleGetBundleWithIdentifier("com.apple.opengl" as CFString)
        let addr = CFBundleGetFunctionPointerForName(indentifier, symbol)

        if symbol as String == "glFlush" {
            return glDummyPtr()
        }

        return addr
    }

    func setGLCBUpdateCallback(_ callback: @escaping mpv_opengl_cb_update_fn, context object: AnyObject) {
        if mpvGLCBContext == nil {
            sendWarning("Init mpv opengl-cb first.")
        } else {
            mpv_opengl_cb_set_update_callback(mpvGLCBContext, callback, MPVHelper.bridge(obj: object))
        }
    }

    func setGLCBControlCallback(_ callback: @escaping mpv_opengl_cb_control_fn, context object: AnyObject) {
        if mpvGLCBContext == nil {
            sendWarning("Init mpv opengl-cb first.")
        } else {
            mp_client_set_control_callback(mpvGLCBContext, callback, MPVHelper.bridge(obj: object))
        }
    }

    func reportGLCBFlip() {
        if mpvGLCBContext == nil { return }
            mpv_opengl_cb_report_flip(mpvGLCBContext, 0)
    }

    func drawGLCB(_ surface: NSSize) {
        if mpvGLCBContext != nil {
            var i: GLint = 0
            glGetIntegerv(GLenum(GL_DRAW_FRAMEBUFFER_BINDING), &i)
            // CAOpenGLLayer has ownership of FBO zero yet can return it to us,
            // so only utilize a newly received FBO ID if it is nonzero.
            fbo = i != 0 ? i : fbo

            mpv_opengl_cb_draw(mpvGLCBContext, fbo, Int32(surface.width), Int32(-surface.height))
        } else {
            glClearColor(0, 0, 0, 1)
            glClear(GLbitfield(GL_COLOR_BUFFER_BIT))
        }
    }

    func setGLCBICCProfile(_ profile: NSColorSpace) {
        if mpvGLCBContext == nil { return }
        var iccData = profile.iccProfileData
        iccData!.withUnsafeMutableBytes { (u8Ptr: UnsafeMutablePointer<UInt8>) in
            let icc = bstrdup(nil, bstr(start: u8Ptr, len: iccData!.count))
            mp_client_set_icc_profile(mpvGLCBContext, icc)
        }
    }

    func setGLCBLux(_ lux: Int) {
        if mpvGLCBContext == nil { return }
        mp_client_set_ambient_lux(mpvGLCBContext, Int32(lux))
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

    func deinitGLCB() {
        mpv_opengl_cb_set_update_callback(mpvGLCBContext, nil, nil)
        mp_client_set_control_callback(mpvGLCBContext, nil, nil)
        mpv_opengl_cb_uninit_gl(mpvGLCBContext)
        mpvGLCBContext = nil
    }

    func deinitMPV() {
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
