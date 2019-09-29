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

class MPVHelper: LogHelper {

    var vo: UnsafeMutablePointer<vo>
    var vout: vo { get { return vo.pointee } }
    var opts: mp_vo_opts { get { return vout.opts.pointee } }
    var input: OpaquePointer { get { return vout.input_ctx } }
    var macOpts: macos_opts = macos_opts()

    init(_ vo: UnsafeMutablePointer<vo>, _ name: String) {
        self.vo = vo
        let newlog = mp_log_new(vo, vo.pointee.log, name)

        super.init(newlog)

        guard let app = NSApp as? Application,
              let ptr = mp_get_config_group(vo,
                                            vo.pointee.global,
                                            app.getMacOSConf()) else
        {
            sendError("macOS config group couldn't be retrieved'")
            exit(1)
        }
        macOpts = UnsafeMutablePointer<macos_opts>(OpaquePointer(ptr)).pointee
    }

    func canBeDraggedAt(_ pos: NSPoint) -> Bool {
        let canDrag = !mp_input_test_dragging(input, Int32(pos.x), Int32(pos.y))
        return canDrag
    }

    func mouseEnabled() -> Bool {
        return mp_input_mouse_enabled(input)
    }

    func setMousePosition(_ pos: NSPoint) {
        mp_input_set_mouse_pos(input, Int32(pos.x), Int32(pos.y))
    }

    func putAxis(_ mpkey: Int32, delta: Double) {
        mp_input_put_wheel(input, mpkey, delta)
    }

    func command(_ cmd: String) {
        let cCmd = UnsafePointer<Int8>(strdup(cmd))
        let mpvCmd = mp_input_parse_cmd(input, bstr0(cCmd), "")
        mp_input_queue_cmd(input, mpvCmd)
        free(UnsafeMutablePointer(mutating: cCmd))
    }

    // (__bridge void*)
    class func bridge<T: AnyObject>(obj: T) -> UnsafeMutableRawPointer {
        return UnsafeMutableRawPointer(Unmanaged.passUnretained(obj).toOpaque())
    }

    // (__bridge T*)
    class func bridge<T: AnyObject>(ptr: UnsafeRawPointer) -> T {
        return Unmanaged<T>.fromOpaque(ptr).takeUnretainedValue()
    }
}
