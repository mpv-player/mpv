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

class LogHelper {
    var log: OpaquePointer?

    init(_ log: OpaquePointer?) {
        self.log = log
    }

    func sendVerbose(_ msg: String) {
        send(message: msg, type: MSGL_V)
    }

    func sendInfo(_ msg: String) {
        send(message: msg, type: MSGL_INFO)
    }

    func sendWarning(_ msg: String) {
        send(message: msg, type: MSGL_WARN)
    }

    func sendError(_ msg: String) {
        send(message: msg, type: MSGL_ERR)
    }

    func send(message msg: String, type t: Int) {
        let args: [CVarArg] = [ (msg as NSString).utf8String ?? "NO MESSAGE"]
        mp_msg_va(log, Int32(t), "%s\n", getVaList(args))
    }
}
