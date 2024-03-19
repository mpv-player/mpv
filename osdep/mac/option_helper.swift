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

typealias swift_wakeup_cb_fn = (@convention(c) (UnsafeMutableRawPointer?) -> Void)?

class OptionHelper: NSObject {
    var vo: UnsafeMutablePointer<vo>
    var optsCachePtr: UnsafeMutablePointer<m_config_cache>
    var macOptsCachePtr: UnsafeMutablePointer<m_config_cache>

    var optsPtr: UnsafeMutablePointer<mp_vo_opts>
        { get { return UnsafeMutablePointer<mp_vo_opts>(OpaquePointer(optsCachePtr.pointee.opts)) } }
    var macOptsPtr: UnsafeMutablePointer<macos_opts>
        { get { return UnsafeMutablePointer<macos_opts>(OpaquePointer(macOptsCachePtr.pointee.opts)) } }

    // these computed properties return a local copy of the struct accessed:
    // - don't use if you rely on the pointers
    // - only for reading
    var opts: mp_vo_opts { get { return optsPtr.pointee } }
    var macOpts: macos_opts { get { return macOptsPtr.pointee } }

    init(_ vo: UnsafeMutablePointer<vo>) {
        self.vo = vo

        guard let cache = m_config_cache_alloc(vo, vo.pointee.global, Application.getVoSubConf()),
              let macCache = m_config_cache_alloc(vo, vo.pointee.global, Application.getMacOSConf()) else
        {
            // will never be hit, mp_get_config_group asserts for invalid groups
            exit(1)
        }
        optsCachePtr = cache
        macOptsCachePtr = macCache
    }

    func nextChangedOption(property: inout UnsafeMutableRawPointer?) -> Bool {
        return m_config_cache_get_next_changed(optsCachePtr, &property)
    }

    func setOption(fullscreen: Bool) {
        optsPtr.pointee.fullscreen = fullscreen
        _ = withUnsafeMutableBytes(of: &optsPtr.pointee.fullscreen) { (ptr: UnsafeMutableRawBufferPointer) in
            m_config_cache_write_opt(optsCachePtr, ptr.baseAddress)
        }
    }

    func setOption(minimized: Bool) {
        optsPtr.pointee.window_minimized = minimized
        _ = withUnsafeMutableBytes(of: &optsPtr.pointee.window_minimized) { (ptr: UnsafeMutableRawBufferPointer) in
            m_config_cache_write_opt(optsCachePtr, ptr.baseAddress)
        }
    }

    func setOption(maximized: Bool) {
        optsPtr.pointee.window_maximized = maximized
        _ = withUnsafeMutableBytes(of: &optsPtr.pointee.window_maximized) { (ptr: UnsafeMutableRawBufferPointer) in
            m_config_cache_write_opt(optsCachePtr, ptr.baseAddress)
        }
    }

    func setMacOptionCallback(_ callback: swift_wakeup_cb_fn, context object: AnyObject) {
        m_config_cache_set_wakeup_cb(macOptsCachePtr, callback, TypeHelper.bridge(obj: object))
    }

    func nextChangedMacOption(property: inout UnsafeMutableRawPointer?) -> Bool {
        return m_config_cache_get_next_changed(macOptsCachePtr, &property)
    }
}
