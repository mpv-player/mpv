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
import Carbon.HIToolbox

class InputHelper: NSObject {
    var option: OptionHelper?
    var lock = NSCondition()
    private var input: OpaquePointer?

    let keymap: [mp_keymap] = [
        // special keys
        .init(kVK_Return, MP_KEY_ENTER), .init(kVK_Escape, MP_KEY_ESC),
        .init(kVK_Delete, MP_KEY_BACKSPACE), .init(kVK_Tab, MP_KEY_TAB),
        .init(kVK_VolumeUp, MP_KEY_VOLUME_UP), .init(kVK_VolumeDown, MP_KEY_VOLUME_DOWN),
        .init(kVK_Mute, MP_KEY_MUTE),

        // cursor keys
        .init(kVK_UpArrow, MP_KEY_UP), .init(kVK_DownArrow, MP_KEY_DOWN),
        .init(kVK_LeftArrow, MP_KEY_LEFT), .init(kVK_RightArrow, MP_KEY_RIGHT),

        // navigation block
        .init(kVK_Help, MP_KEY_INSERT), .init(kVK_ForwardDelete, MP_KEY_DELETE),
        .init(kVK_Home, MP_KEY_HOME), .init(kVK_End, MP_KEY_END),
        .init(kVK_PageUp, MP_KEY_PAGE_UP), .init(kVK_PageDown, MP_KEY_PAGE_DOWN),

        // F-keys
        .init(kVK_F1, MP_KEY_F + 1), .init(kVK_F2, MP_KEY_F + 2), .init(kVK_F3, MP_KEY_F + 3),
        .init(kVK_F4, MP_KEY_F + 4), .init(kVK_F5, MP_KEY_F + 5), .init(kVK_F6, MP_KEY_F + 6),
        .init(kVK_F7, MP_KEY_F + 7), .init(kVK_F8, MP_KEY_F + 8), .init(kVK_F9, MP_KEY_F + 9),
        .init(kVK_F10, MP_KEY_F + 10), .init(kVK_F11, MP_KEY_F + 11), .init(kVK_F12, MP_KEY_F + 12),
        .init(kVK_F13, MP_KEY_F + 13), .init(kVK_F14, MP_KEY_F + 14), .init(kVK_F15, MP_KEY_F + 15),
        .init(kVK_F16, MP_KEY_F + 16), .init(kVK_F17, MP_KEY_F + 17), .init(kVK_F18, MP_KEY_F + 18),
        .init(kVK_F19, MP_KEY_F + 19), .init(kVK_F20, MP_KEY_F + 20),

        // numpad
        .init(kVK_ANSI_KeypadPlus, MP_KEY_KPADD),
        .init(kVK_ANSI_KeypadMinus, MP_KEY_KPSUBTRACT),
        .init(kVK_ANSI_KeypadMultiply, MP_KEY_KPMULTIPLY),
        .init(kVK_ANSI_KeypadDivide, MP_KEY_KPDIVIDE),
        .init(kVK_ANSI_KeypadEnter, MP_KEY_KPENTER), .init(kVK_ANSI_KeypadDecimal, MP_KEY_KPDEC),
        .init(kVK_ANSI_Keypad0, MP_KEY_KP0), .init(kVK_ANSI_Keypad1, MP_KEY_KP1),
        .init(kVK_ANSI_Keypad2, MP_KEY_KP2), .init(kVK_ANSI_Keypad3, MP_KEY_KP3),
        .init(kVK_ANSI_Keypad4, MP_KEY_KP4), .init(kVK_ANSI_Keypad5, MP_KEY_KP5),
        .init(kVK_ANSI_Keypad6, MP_KEY_KP6), .init(kVK_ANSI_Keypad7, MP_KEY_KP7),
        .init(kVK_ANSI_Keypad8, MP_KEY_KP8), .init(kVK_ANSI_Keypad9, MP_KEY_KP9),

        .init(0, 0)
    ]

    init(_ input: OpaquePointer? = nil, _ option: OptionHelper? = nil) {
        super.init()
        self.input = input
        self.option = option
    }

    func put(
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
            var key = (useAltGr() && event.modifierFlags.contains(.optionRight)) ? chars : charsNoMod
            if key.isEmpty { key = mapDeadKey(event) }
            if key.isEmpty { return false }
            key.withCString {
                var bstr = bstr0($0)
                putKey(bstr_decode_utf8(bstr, &bstr), modifiers: event.modifierFlags, type: event.type)
            }
            return true
        }
    }

    func processMouse(event: NSEvent) {
        if !mouseEnabled() { return }
        lock.withLock {
            putKey(map(button: event.buttonNumber), modifiers: event.modifierFlags, type: event.type)
            if event.clickCount > 1 {
                putKey(map(button: event.buttonNumber), modifiers: event.modifierFlags, type: .keyUp)
            }
        }
    }

    func processWheel(event: NSEvent) {
        if !mouseEnabled() { return }
        lock.withLock {
            guard let input = input else { return }
            let modifiers = event.modifierFlags
            let precise = event.hasPreciseScrollingDeltas
            var deltaX = event.deltaX * 0.1
            var deltaY = event.deltaY * 0.1

            if !precise {
                deltaX = modifiers.contains(.shift) ? event.scrollingDeltaY : event.scrollingDeltaX
                deltaY = modifiers.contains(.shift) ? event.scrollingDeltaX : event.scrollingDeltaY
            }

            var key = deltaY > 0 ? SWIFT_WHEEL_UP : SWIFT_WHEEL_DOWN
            var delta = Double(deltaY)
            if abs(deltaX) > abs(deltaY) {
                key = deltaX > 0 ? SWIFT_WHEEL_LEFT : SWIFT_WHEEL_RIGHT
                delta = Double(deltaX)
            }

            mp_input_put_wheel(input, key | mapModifier(modifiers), precise ? abs(delta) : 1)
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

    func setMouse(position: NSPoint) {
        if !mouseEnabled() { return }
        lock.withLock {
            guard let input = input else { return }
            mp_input_set_mouse_pos(input, Int32(position.x), Int32(position.y), false)
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
        let typeMapping: [NSEvent.EventType: UInt32] = [
            .keyDown: MP_KEY_STATE_DOWN,
            .keyUp: MP_KEY_STATE_UP,
            .leftMouseDown: MP_KEY_STATE_DOWN,
            .leftMouseUp: MP_KEY_STATE_UP,
            .rightMouseDown: MP_KEY_STATE_DOWN,
            .rightMouseUp: MP_KEY_STATE_UP,
            .otherMouseDown: MP_KEY_STATE_DOWN,
            .otherMouseUp: MP_KEY_STATE_UP
        ]

        return Int32(typeMapping[type] ?? 0)
    }

    private func mapModifier(_ modifiers: NSEvent.ModifierFlags) -> Int32 {
        var mask: UInt32 = 0

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

    private func map(button: Int) -> Int32 {
        let buttonMapping: [Int: Int32] = [
            0: SWIFT_MBTN_LEFT,
            1: SWIFT_MBTN_RIGHT,
            2: SWIFT_MBTN_MID,
            3: SWIFT_MBTN_FORWARD,
            4: SWIFT_MBTN_BACK
        ]

        return Int32(buttonMapping[button] ?? SWIFT_MBTN9 + Int32(button - 5))
    }

    func mapDeadKey(_ event: NSEvent) -> String {
        let keyboard = TISCopyCurrentKeyboardInputSource().takeRetainedValue()
        let layoutPtr = TISGetInputSourceProperty(keyboard, kTISPropertyUnicodeKeyLayoutData)
        let layoutData = unsafeBitCast(layoutPtr, to: CFData.self)
        let layout = unsafeBitCast(CFDataGetBytePtr(layoutData), to: UnsafePointer<UCKeyboardLayout>.self)
        let maxLength = 2
        let modifiers: UInt32 = UInt32(event.modifierFlags.rawValue >> 16) & 0xff
        var deadKeyState: UInt32 = 0
        var length = 0
        var chars = [UniChar](repeating: 0, count: maxLength)
        let err = UCKeyTranslate(layout, event.keyCode, UInt16(kUCKeyActionDisplay), modifiers,
                                 UInt32(LMGetKbdType()), 0, &deadKeyState, maxLength, &length, &chars)

        if err != noErr || length < 1 { return "" }
        return String(utf16CodeUnits: chars, count: length)
    }

    @objc func open(files: [String], append: Bool = false) {
        lock.withLock {
            guard let input = input else { return }
            if (option?.vo.drag_and_drop ?? -1) == -2 { return }

            var action = DND_APPEND
            if !append {
                action = NSEvent.modifierFlags.contains(.shift) ? DND_APPEND : DND_REPLACE
                if (option?.vo.drag_and_drop ?? -1) >= 0 {
                    action = mp_dnd_action(UInt32(option?.vo.drag_and_drop ?? Int32(DND_REPLACE.rawValue)))
                }
            }

            let filesClean = files.map { $0.hasPrefix("file:///.file/id=") ? (URL(string: $0)?.path ?? $0) : $0 }
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

    func signal(input: OpaquePointer? = nil) {
        lock.withLock {
            self.input = input
            if input != nil { lock.signal() }
        }
    }

    @objc func wait() {
        lock.withLock { while input == nil { lock.wait() } }
    }
}
