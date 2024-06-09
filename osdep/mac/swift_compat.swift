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

#if !swift(>=5.7)
extension NSCondition {
    func withLock<R>(_ body: () throws -> R) rethrows -> R {
        self.lock()
        defer { self.unlock() }
        return try body()
    }
}
#endif

#if !swift(>=5.0)
extension Data {
    mutating func withUnsafeMutableBytes<Type>(_ body: (UnsafeMutableRawBufferPointer) throws -> Type) rethrows -> Type {
        let dataCount = count
        return try withUnsafeMutableBytes { (ptr: UnsafeMutablePointer<UInt8>) throws -> Type in
            try body(UnsafeMutableRawBufferPointer(start: ptr, count: dataCount))
        }
    }
}
#endif

#if !swift(>=4.2)
extension NSDraggingInfo {
    var draggingPasteboard: NSPasteboard {
        return draggingPasteboard()
    }
}
#endif
