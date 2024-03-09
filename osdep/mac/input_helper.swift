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

class InputHelper: NSObject {
    var lock = NSCondition()
    private var input: OpaquePointer?

    @objc init(_ input: OpaquePointer? = nil) {
        super.init()
        self.input = input
    }

    @objc func command(_ cmd: String) -> Bool {
        lock.withLock {
            guard let input = input else { return false }
            let cCmd = UnsafePointer<Int8>(strdup(cmd))
            let mpvCmd = mp_input_parse_cmd(input, bstr0(cCmd), "")
            mp_input_queue_cmd(input, mpvCmd)
            free(UnsafeMutablePointer(mutating: cCmd))
            return true
        }
    }

    @objc func putKey(_ key: Int32) {
        lock.withLock {
            guard let input = input else { return }
            mp_input_put_key(input, key)
        }
    }

    @objc func useAltGr() -> Bool {
        lock.withLock {
            guard let input = input else { return false }
            return mp_input_use_alt_gr(input)
        }
    }

    @objc func wakeup() {
        lock.withLock {
            guard let input = input else { return }
            mp_input_wakeup(input)
        }
    }

    @objc func signal(input: OpaquePointer? = nil) {
        lock.withLock {
            self.input = input
            if input != nil { lock.signal() }
        }
    }

    @objc func wait() {
        lock.withLock { while input == nil { lock.wait() } }
    }
}
