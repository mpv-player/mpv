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
import os

class LogHelper {
    var log: OpaquePointer?
#if HAVE_MACOS_11_FEATURES
    @available(macOS 11.0, *)
    var logger: Logger? {
        return Logger(subsystem: "io.mpv", category: "mpv")
    }
#endif

    let loggerMapping: [Int: OSLogType] = [
        MSGL_V: .debug,
        MSGL_INFO: .info,
        MSGL_WARN: .error,
        MSGL_ERR: .fault
    ]

    init(_ log: OpaquePointer? = nil) {
        self.log = log
    }

    func verbose(_ message: String) {
        send(message: message, type: MSGL_V)
    }

    func info(_ message: String) {
        send(message: message, type: MSGL_INFO)
    }

    func warning(_ message: String) {
        send(message: message, type: MSGL_WARN)
    }

    func error(_ message: String) {
        send(message: message, type: MSGL_ERR)
    }

    func send(message: String, type: Int) {
        guard let log = log else {
#if HAVE_MACOS_11_FEATURES
        if #available(macOS 11.0, *) {
            logger?.log(level: loggerMapping[type] ?? .default, "\(message, privacy: .public)")
        }
#endif
            return
        }

        let args: [CVarArg] = [(message as NSString).utf8String ?? "NO MESSAGE"]
        mp_msg_va(log, Int32(type), "%s\n", getVaList(args))
    }

    deinit {
        // only a manual dereferencing will trigger this, cleanup properly in that case
        ta_free(UnsafeMutablePointer(log))
        log = nil
    }
}
