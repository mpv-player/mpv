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

class Clipboard: NSObject {
    var pasteboard: NSPasteboard { return NSPasteboard.general }
    var changeCount: Int = 0

    @objc override init() {
        super.init()
        changeCount = pasteboard.changeCount
    }

    @objc func get(params: UnsafeMutablePointer<clipboard_access_params>,
                   out: UnsafeMutablePointer<clipboard_data>,
                   tallocCtx: UnsafeMutableRawPointer) -> clipboard_result {
        switch params.pointee.type {
        case CLIPBOARD_DATA_TEXT: return fillWithString(out, tallocCtx)
        default: break
        }

        return CLIPBOARD_FAILED
    }

    @objc func set(params: UnsafeMutablePointer<clipboard_access_params>,
                   data: UnsafeMutablePointer<clipboard_data>) -> clipboard_result {
        switch (params.pointee.type, data.pointee.type) {
        case (CLIPBOARD_DATA_TEXT, CLIPBOARD_DATA_TEXT): return setString(data)
        default: break
        }

        return CLIPBOARD_FAILED
    }

    @objc func changed() -> Bool {
        if changeCount != pasteboard.changeCount {
            changeCount = pasteboard.changeCount
            return true
        }

        return false
    }

    func fillWithString(_ out: UnsafeMutablePointer<clipboard_data>,
                        _ tallocCtx: UnsafeMutableRawPointer) -> clipboard_result {
        (pasteboard.string(forType: .string) ?? "").withCString {
            out.pointee.type = CLIPBOARD_DATA_TEXT
            out.pointee.u.text = ta_xstrdup(tallocCtx, $0)
        }

        return CLIPBOARD_SUCCESS
    }

    func setString(_ data: UnsafeMutablePointer<clipboard_data>) -> clipboard_result {
        let text = String(cString: data.pointee.u.text)
        pasteboard.clearContents()
        pasteboard.setString(text, forType: .string)

        return CLIPBOARD_SUCCESS
    }
}
