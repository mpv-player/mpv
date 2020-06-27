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

class MPVHelper {
    var log: LogHelper
    var vo: UnsafeMutablePointer<vo>
    var optsCachePtr: UnsafeMutablePointer<m_config_cache>
    var optsPtr: UnsafeMutablePointer<mp_vo_opts>

    // these computed properties return a local copy of the struct accessed:
    // - don't use if you rely on the pointers
    // - only for reading
    var vout: vo { get { return vo.pointee } }
    var optsCache: m_config_cache { get { return optsCachePtr.pointee } }
    var opts: mp_vo_opts { get { return optsPtr.pointee } }

    var input: OpaquePointer { get { return vout.input_ctx } }
    var macOpts: macos_opts = macos_opts()

    init(_ vo: UnsafeMutablePointer<vo>, _ log: LogHelper) {
        self.vo = vo
        self.log = log

        guard let app = NSApp as? Application,
              let cache = m_config_cache_alloc(vo, vo.pointee.global, app.getVoSubConf()) else
        {
            log.sendError("NSApp couldn't be retrieved")
            exit(1)
        }

        optsCachePtr = cache
        optsPtr = UnsafeMutablePointer<mp_vo_opts>(OpaquePointer(cache.pointee.opts))

        guard let ptr = mp_get_config_group(vo,
                                            vo.pointee.global,
                                            app.getMacOSConf()) else
        {
            // will never be hit, mp_get_config_group asserts for invalid groups
            return
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

    func nextChangedConfig(property: inout UnsafeMutableRawPointer?) -> Bool {
        return m_config_cache_get_next_changed(optsCachePtr, &property)
    }

    func setConfigProperty(fullscreen: Bool) {
        optsPtr.pointee.fullscreen = fullscreen
        m_config_cache_write_opt(optsCachePtr, UnsafeMutableRawPointer(&optsPtr.pointee.fullscreen))
    }

    func setConfigProperty(minimized: Bool) {
        optsPtr.pointee.window_minimized = Int32(minimized)
        m_config_cache_write_opt(optsCachePtr, UnsafeMutableRawPointer(&optsPtr.pointee.window_minimized))
    }

    func setConfigProperty(maximized: Bool) {
        optsPtr.pointee.window_maximized = Int32(maximized)
        m_config_cache_write_opt(optsCachePtr, UnsafeMutableRawPointer(&optsPtr.pointee.window_maximized))
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
