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

class TypeHelper {
    // (__bridge void*)
    class func bridge<T: AnyObject>(obj: T) -> UnsafeMutableRawPointer {
        return UnsafeMutableRawPointer(Unmanaged.passUnretained(obj).toOpaque())
    }

    // (__bridge T*)
    class func bridge<T: AnyObject>(ptr: UnsafeRawPointer) -> T {
        return Unmanaged<T>.fromOpaque(ptr).takeUnretainedValue()
    }

    class func withUnsafeMutableRawPointers(_ arguments: [Any],
                                            pointers: [UnsafeMutableRawPointer?] = [],
                                            closure: (_ pointers: [UnsafeMutableRawPointer?]) -> Void) {
        if arguments.count > 0 {
            let args = Array(arguments.dropFirst(1))
            var newPtrs = pointers
            var firstArg = arguments.first
            withUnsafeMutableBytes(of: &firstArg) { (ptr: UnsafeMutableRawBufferPointer) in
                newPtrs.append(ptr.baseAddress)
                withUnsafeMutableRawPointers(args, pointers: newPtrs, closure: closure)
            }

            return
        }

        closure(pointers)
    }

    class func toPointer<T>(_ value: inout T) -> UnsafeMutableRawPointer? {
        return withUnsafeMutableBytes(of: &value) { (ptr: UnsafeMutableRawBufferPointer) in
            ptr.baseAddress
        }
    }

    // *(char **) MPV_FORMAT_STRING
    class func toString(_ obj: UnsafeMutableRawPointer?) -> String? {
        guard let str = obj else { return nil }
        let cstr = UnsafeMutablePointer<UnsafeMutablePointer<Int8>>(OpaquePointer(str))
        return String(cString: cstr[0])
    }

    // MPV_FORMAT_FLAG
    class func toBool(_ obj: UnsafeMutableRawPointer) -> Bool? {
        return UnsafePointer<Bool>(OpaquePointer(obj))?.pointee
    }

    // MPV_FORMAT_DOUBLE
    class func toDouble(_ obj: UnsafeMutableRawPointer) -> Double? {
        return UnsafePointer<Double>(OpaquePointer(obj))?.pointee
    }
}
