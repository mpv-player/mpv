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

extension OptionHelper {
    typealias WakeupCallback = (@convention(c) (UnsafeMutableRawPointer?) -> Void)?
}

class OptionHelper {
    var voCachePtr: UnsafeMutablePointer<m_config_cache>
    var macCachePtr: UnsafeMutablePointer<m_config_cache>

    var voPtr: UnsafeMutablePointer<mp_vo_opts>
        { get { return UnsafeMutablePointer<mp_vo_opts>(OpaquePointer(voCachePtr.pointee.opts)) } }
    var macPtr: UnsafeMutablePointer<macos_opts>
        { get { return UnsafeMutablePointer<macos_opts>(OpaquePointer(macCachePtr.pointee.opts)) } }

    // these computed properties return a local copy of the struct accessed:
    // - don't use if you rely on the pointers
    // - only for reading
    var vo: mp_vo_opts { get { return voPtr.pointee } }
    var mac: macos_opts { get { return macPtr.pointee } }

    init(_ taParent: UnsafeMutableRawPointer, _ global: OpaquePointer?) {
        voCachePtr = m_config_cache_alloc(taParent, global, AppHub.shared.getVoConf())
        macCachePtr = m_config_cache_alloc(taParent, global, AppHub.shared.getMacConf())
    }

    func nextChangedOption(property: inout UnsafeMutableRawPointer?) -> Bool {
        return m_config_cache_get_next_changed(voCachePtr, &property)
    }

    func setOption(fullscreen: Bool) {
        voPtr.pointee.fullscreen = fullscreen
        _ = withUnsafeMutableBytes(of: &voPtr.pointee.fullscreen) { (ptr: UnsafeMutableRawBufferPointer) in
            m_config_cache_write_opt(voCachePtr, ptr.baseAddress)
        }
    }

    func setOption(minimized: Bool) {
        voPtr.pointee.window_minimized = minimized
        _ = withUnsafeMutableBytes(of: &voPtr.pointee.window_minimized) { (ptr: UnsafeMutableRawBufferPointer) in
            m_config_cache_write_opt(voCachePtr, ptr.baseAddress)
        }
    }

    func setOption(maximized: Bool) {
        voPtr.pointee.window_maximized = maximized
        _ = withUnsafeMutableBytes(of: &voPtr.pointee.window_maximized) { (ptr: UnsafeMutableRawBufferPointer) in
            m_config_cache_write_opt(voCachePtr, ptr.baseAddress)
        }
    }

    func setMacOptionCallback(_ callback: WakeupCallback, context object: AnyObject) {
        m_config_cache_set_wakeup_cb(macCachePtr, callback, TypeHelper.bridge(obj: object))
    }

    func nextChangedMacOption(property: inout UnsafeMutableRawPointer?) -> Bool {
        return m_config_cache_get_next_changed(macCachePtr, &property)
    }
}
