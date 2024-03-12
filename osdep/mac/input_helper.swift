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

import Carbon.HIToolbox

class InputHelper: NSObject {
    var mpv: MPVHelper?
    var lock = NSCondition()
    private var input: OpaquePointer?

    let keymap: [mp_keymap] = [
        // special keys
        mp_keymap(from: Int32(kVK_Return), to: MP_KEY_ENTER),
        mp_keymap(from: Int32(kVK_Escape), to: MP_KEY_ESC),
        mp_keymap(from: Int32(kVK_Delete), to: MP_KEY_BACKSPACE),
        mp_keymap(from: Int32(kVK_Tab), to: MP_KEY_TAB),

        // cursor keys
        mp_keymap(from: Int32(kVK_UpArrow), to: MP_KEY_UP),
        mp_keymap(from: Int32(kVK_DownArrow), to: MP_KEY_DOWN),
        mp_keymap(from: Int32(kVK_LeftArrow), to: MP_KEY_LEFT),
        mp_keymap(from: Int32(kVK_RightArrow), to: MP_KEY_RIGHT),

        // navigation block
        mp_keymap(from: Int32(kVK_Help), to: MP_KEY_INSERT),
        mp_keymap(from: Int32(kVK_ForwardDelete), to: MP_KEY_DELETE),
        mp_keymap(from: Int32(kVK_Home), to: MP_KEY_HOME),
        mp_keymap(from: Int32(kVK_End), to: MP_KEY_END),
        mp_keymap(from: Int32(kVK_PageUp), to: MP_KEY_PAGE_UP),
        mp_keymap(from: Int32(kVK_PageDown), to: MP_KEY_PAGE_DOWN),

        // F-keys
        mp_keymap(from: Int32(kVK_F1), to: MP_KEY_F + 1),
        mp_keymap(from: Int32(kVK_F2), to: MP_KEY_F + 2),
        mp_keymap(from: Int32(kVK_F3), to: MP_KEY_F + 3),
        mp_keymap(from: Int32(kVK_F4), to: MP_KEY_F + 4),
        mp_keymap(from: Int32(kVK_F5), to: MP_KEY_F + 5),
        mp_keymap(from: Int32(kVK_F6), to: MP_KEY_F + 6),
        mp_keymap(from: Int32(kVK_F7), to: MP_KEY_F + 7),
        mp_keymap(from: Int32(kVK_F8), to: MP_KEY_F + 8),
        mp_keymap(from: Int32(kVK_F9), to: MP_KEY_F + 9),
        mp_keymap(from: Int32(kVK_F10), to: MP_KEY_F + 10),
        mp_keymap(from: Int32(kVK_F11), to: MP_KEY_F + 11),
        mp_keymap(from: Int32(kVK_F12), to: MP_KEY_F + 12),
        mp_keymap(from: Int32(kVK_F13), to: MP_KEY_F + 13),
        mp_keymap(from: Int32(kVK_F14), to: MP_KEY_F + 14),
        mp_keymap(from: Int32(kVK_F15), to: MP_KEY_F + 15),
        mp_keymap(from: Int32(kVK_F16), to: MP_KEY_F + 16),
        mp_keymap(from: Int32(kVK_F17), to: MP_KEY_F + 17),
        mp_keymap(from: Int32(kVK_F18), to: MP_KEY_F + 18),
        mp_keymap(from: Int32(kVK_F19), to: MP_KEY_F + 19),
        mp_keymap(from: Int32(kVK_F20), to: MP_KEY_F + 20),

        // numpad
        mp_keymap(from: Int32(kVK_ANSI_KeypadPlus), to: Int32(Character("+").asciiValue ?? 0)),
        mp_keymap(from: Int32(kVK_ANSI_KeypadMinus), to: Int32(Character("-").asciiValue ?? 0)),
        mp_keymap(from: Int32(kVK_ANSI_KeypadMultiply), to: Int32(Character("*").asciiValue ?? 0)),
        mp_keymap(from: Int32(kVK_ANSI_KeypadDivide), to: Int32(Character("/").asciiValue ?? 0)),
        mp_keymap(from: Int32(kVK_ANSI_KeypadEnter), to: MP_KEY_KPENTER),
        mp_keymap(from: Int32(kVK_ANSI_KeypadDecimal), to: MP_KEY_KPDEC),
        mp_keymap(from: Int32(kVK_ANSI_Keypad0), to: MP_KEY_KP0),
        mp_keymap(from: Int32(kVK_ANSI_Keypad1), to: MP_KEY_KP1),
        mp_keymap(from: Int32(kVK_ANSI_Keypad2), to: MP_KEY_KP2),
        mp_keymap(from: Int32(kVK_ANSI_Keypad3), to: MP_KEY_KP3),
        mp_keymap(from: Int32(kVK_ANSI_Keypad4), to: MP_KEY_KP4),
        mp_keymap(from: Int32(kVK_ANSI_Keypad5), to: MP_KEY_KP5),
        mp_keymap(from: Int32(kVK_ANSI_Keypad6), to: MP_KEY_KP6),
        mp_keymap(from: Int32(kVK_ANSI_Keypad7), to: MP_KEY_KP7),
        mp_keymap(from: Int32(kVK_ANSI_Keypad8), to: MP_KEY_KP8),
        mp_keymap(from: Int32(kVK_ANSI_Keypad9), to: MP_KEY_KP9),

        mp_keymap(from: 0, to: 0)
    ]

    @objc init(_ input: OpaquePointer? = nil, _ mpv: MPVHelper? = nil) {
        super.init()
        self.input = input
        self.mpv = mpv
    }

    @objc func put(
        key: Int32,
        modifiers: NSEvent.ModifierFlags = .init(rawValue: 0),
        type: NSEvent.EventType = .applicationDefined
    ) {
        lock.withLock {
            putKey(key, modifiers: modifiers, type: type)
        }
    }

    private func putKey(
        _ key: Int32,
        modifiers: NSEvent.ModifierFlags = .init(rawValue: 0),
        type: NSEvent.EventType = .applicationDefined
    ) {
        if key < 1 { return }

        guard let input = input else { return }
        let code = key | mapModifier(modifiers) | mapType(type)
        mp_input_put_key(input, code)

        if type == .keyUp {
            mp_input_put_key(input, MP_INPUT_RELEASE_ALL)
        }
    }

    @objc func processKey(event: NSEvent) -> Bool {
        if event.type != .keyDown && event.type != .keyUp { return false }
        if NSApp.mainMenu?.performKeyEquivalent(with: event) ?? false || event.isARepeat { return true }

        return lock.withLock {
            let mpkey = lookup_keymap_table(keymap, Int32(event.keyCode))
            if mpkey > 0 {
                putKey(mpkey, modifiers: event.modifierFlags, type: event.type)
                return true
            }

            guard let chars = event.characters, let charsNoMod = event.charactersIgnoringModifiers else { return false }
            let key = (useAltGr() && event.modifierFlags.contains(.optionRight)) ? chars : charsNoMod
            key.withCString {
                var bstr = bstr0($0)
                putKey(bstr_decode_utf8(bstr, &bstr), modifiers: event.modifierFlags, type: event.type)
            }
            return true
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

    private func mapType(_ type: NSEvent.EventType) -> Int32 {
        let typeMapping: [NSEvent.EventType:UInt32] = [
            .keyDown: MP_KEY_STATE_DOWN,
            .keyUp: MP_KEY_STATE_UP,
        ]

        return Int32(typeMapping[type] ?? 0);
    }

    private func mapModifier(_ modifiers: NSEvent.ModifierFlags) -> Int32 {
        var mask: UInt32 = 0;

        if modifiers.contains(.shift) {
            mask |= MP_KEY_MODIFIER_SHIFT
        }
        if modifiers.contains(.control) {
            mask |= MP_KEY_MODIFIER_CTRL
        }
        if modifiers.contains(.command) {
            mask |= MP_KEY_MODIFIER_META
        }
        if modifiers.contains(.optionLeft) || modifiers.contains(.optionRight) && !useAltGr() {
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

    private func useAltGr() -> Bool {
        guard let input = input else { return false }
        return mp_input_use_alt_gr(input)
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
