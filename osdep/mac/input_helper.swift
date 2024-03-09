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
    var mpv: MPVHelper?
    var lock = NSCondition()
    private var input: OpaquePointer?

    @objc init(_ input: OpaquePointer? = nil, _ mpv: MPVHelper? = nil) {
        super.init()
        self.input = input
        self.mpv = mpv
    }

    @objc func putKey(_ key: Int32, modifiers: NSEvent.ModifierFlags = .init(rawValue: 0)) {
        lock.withLock {
            guard let input = input else { return }
            mp_input_put_key(input, key | mapModifier(modifiers))
        }
    }

    func draggable(at pos: NSPoint) -> Bool {
        lock.withLock {
            guard let input = input else { return false }
            return !mp_input_test_dragging(input, Int32(pos.x), Int32(pos.y))
        }
    }

    func mouseEnabled() -> Bool {
        lock.withLock {
            guard let input = input else { return true }
            return mp_input_mouse_enabled(input)
        }
    }

    func setMouse(position pos: NSPoint) {
        lock.withLock {
            guard let input = input else { return }
            mp_input_set_mouse_pos(input, Int32(pos.x), Int32(pos.y))
        }
    }

    func putAxis(_ mpkey: Int32, modifiers: NSEvent.ModifierFlags, delta: Double) {
        lock.withLock {
            guard let input = input else { return }
            mp_input_put_wheel(input, mpkey | mapModifier(modifiers), delta)
        }
    }

    @discardableResult @objc func command(_ cmd: String) -> Bool {
        lock.withLock {
            guard let input = input else { return false }
            let cCmd = UnsafePointer<Int8>(strdup(cmd))
            let mpvCmd = mp_input_parse_cmd(input, bstr0(cCmd), "")
            mp_input_queue_cmd(input, mpvCmd)
            free(UnsafeMutablePointer(mutating: cCmd))
            return true
        }
    }

    private func mapModifier(_ modifiers: NSEvent.ModifierFlags) -> Int32 {
        var mask: UInt32 = 0;
        guard let input = input else { return Int32(mask) }

        if modifiers.contains(.shift) {
            mask |= MP_KEY_MODIFIER_SHIFT
        }
        if modifiers.contains(.control) {
            mask |= MP_KEY_MODIFIER_CTRL
        }
        if modifiers.contains(.command) {
            mask |= MP_KEY_MODIFIER_META
        }
        if modifiers.rawValue & UInt(NX_DEVICELALTKEYMASK) != 0 ||
           modifiers.rawValue & UInt(NX_DEVICERALTKEYMASK) != 0 && !mp_input_use_alt_gr(input)
        {
            mask |= MP_KEY_MODIFIER_ALT
        }

        return Int32(mask)
    }

    @objc func open(files: [String]) {
        lock.withLock {
            guard let input = input else { return }
            if (mpv?.opts.drag_and_drop ?? -1) == -2 { return }

            var action = NSEvent.modifierFlags.contains(.shift) ? DND_APPEND : DND_REPLACE
            if (mpv?.opts.drag_and_drop ?? -1) >= 0  {
                action = mp_dnd_action(UInt32(mpv?.opts.drag_and_drop ?? Int32(DND_REPLACE.rawValue)))
            }

            let filesClean = files.map{ $0.hasPrefix("file:///.file/id=") ? (URL(string: $0)?.path ?? $0) : $0 }
            var filesPtr = filesClean.map { UnsafeMutablePointer<CChar>(strdup($0)) }
            mp_event_drop_files(input, Int32(files.count), &filesPtr, action)
            for charPtr in filesPtr { free(UnsafeMutablePointer(mutating: charPtr)) }
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
