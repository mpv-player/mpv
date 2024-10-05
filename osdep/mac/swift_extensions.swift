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
import IOKit.hidsystem

extension NSDeviceDescriptionKey {
    static let screenNumber = NSDeviceDescriptionKey("NSScreenNumber")
}

extension NSScreen {
    public var displayID: CGDirectDisplayID {
        return deviceDescription[.screenNumber] as? CGDirectDisplayID ?? 0
    }

    public var serialNumber: String {
        return String(CGDisplaySerialNumber(displayID))
    }

    public var name: String {
        guard let regex = try? NSRegularExpression(pattern: " \\(\\d+\\)$", options: .caseInsensitive) else {
            return localizedName
        }
        return regex.stringByReplacingMatches(
            in: localizedName,
            range: NSRange(location: 0, length: localizedName.count),
            withTemplate: ""
        )
    }

    public var uniqueName: String {
        return name + " (\(serialNumber))"
    }
}

extension NSColor {
    convenience init(hex: String) {
        let int = Int(hex.dropFirst(), radix: 16) ?? 0
        let alpha = CGFloat((int >> 24) & 0x000000FF)/255
        let red   = CGFloat((int >> 16) & 0x000000FF)/255
        let green = CGFloat((int >> 8)  & 0x000000FF)/255
        let blue  = CGFloat((int)       & 0x000000FF)/255

        self.init(calibratedRed: red, green: green, blue: blue, alpha: alpha)
    }
}

extension NSEvent.ModifierFlags {
    public static var optionLeft: NSEvent.ModifierFlags = .init(rawValue: UInt(NX_DEVICELALTKEYMASK))
    public static var optionRight: NSEvent.ModifierFlags = .init(rawValue: UInt(NX_DEVICERALTKEYMASK))
}

extension mp_keymap {
    init(_ f: Int, _ t: Int32) {
        self.init(from: Int32(f), to: t)
    }
}

extension mpv_event_id: CustomStringConvertible {
    public var description: String {
        switch self {
        case MPV_EVENT_NONE: return "MPV_EVENT_NONE"
        case MPV_EVENT_SHUTDOWN: return "MPV_EVENT_SHUTDOWN"
        case MPV_EVENT_LOG_MESSAGE: return "MPV_EVENT_LOG_MESSAGE"
        case MPV_EVENT_GET_PROPERTY_REPLY: return "MPV_EVENT_GET_PROPERTY_REPLY"
        case MPV_EVENT_SET_PROPERTY_REPLY: return "MPV_EVENT_SET_PROPERTY_REPLY"
        case MPV_EVENT_COMMAND_REPLY: return "MPV_EVENT_COMMAND_REPLY"
        case MPV_EVENT_START_FILE: return "MPV_EVENT_START_FILE"
        case MPV_EVENT_END_FILE: return "MPV_EVENT_END_FILE"
        case MPV_EVENT_FILE_LOADED: return "MPV_EVENT_FILE_LOADED"
        case MPV_EVENT_IDLE: return "MPV_EVENT_IDLE"
        case MPV_EVENT_TICK: return "MPV_EVENT_TICK"
        case MPV_EVENT_CLIENT_MESSAGE: return "MPV_EVENT_CLIENT_MESSAGE"
        case MPV_EVENT_VIDEO_RECONFIG: return "MPV_EVENT_VIDEO_RECONFIG"
        case MPV_EVENT_AUDIO_RECONFIG: return "MPV_EVENT_AUDIO_RECONFIG"
        case MPV_EVENT_SEEK: return "MPV_EVENT_SEEK"
        case MPV_EVENT_PLAYBACK_RESTART: return "MPV_EVENT_PLAYBACK_RESTART"
        case MPV_EVENT_PROPERTY_CHANGE: return "MPV_EVENT_PROPERTY_CHANGE"
        case MPV_EVENT_QUEUE_OVERFLOW: return "MPV_EVENT_QUEUE_OVERFLOW"
        case MPV_EVENT_HOOK: return "MPV_EVENT_HOOK"
        default: return "MPV_EVENT_" + String(self.rawValue)
        }
    }
}

extension Bool {
    init(_ int32: Int32) {
        self.init(int32 != 0)
    }
}

extension Int32 {
    init(_ bool: Bool) {
        self.init(bool ? 1 : 0)
    }
}
