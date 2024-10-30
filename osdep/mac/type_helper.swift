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

    // char ** OPT_STRINGLIST
    class func toStringArray(_ obj: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?) -> [String] {
        guard var cStringArray = obj else { return [] }
        var stringArray: [String] = []
        while let cString = cStringArray.pointee {
            stringArray.append(String(cString: cString))
            cStringArray += 1
        }
        return stringArray
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

    // MPV_FORMAT_NODE
    class func toNode(_ obj: UnsafeMutableRawPointer) -> mpv_node? {
        return UnsafePointer<mpv_node>(OpaquePointer(obj))?.pointee
    }

    // MPV_FORMAT_NODE > MPV_FORMAT_STRING
    class func nodeToString(_ node: mpv_node?) -> String? {
        guard let cString = node?.u.string else { return nil }
        return String(cString: cString)
    }

    // MPV_FORMAT_NODE > MPV_FORMAT_FLAG
    class func nodeToBool(_ node: mpv_node?) -> Bool? {
        guard let flag = node?.u.flag else { return nil }
        return Bool(flag)
    }

    // MPV_FORMAT_NODE > MPV_FORMAT_INT64
    class func nodeToInt(_ node: mpv_node?) -> Int64? {
        return node?.u.int64
    }

    // MPV_FORMAT_NODE > MPV_FORMAT_DOUBLE
    class func nodeToDouble(_ node: mpv_node?) -> Double? {
        return node?.u.double_
    }

    // MPV_FORMAT_NODE > MPV_FORMAT_NODE_ARRAY
    class func nodeToArray(_ node: mpv_node?) -> [Any?] {
        var array: [Any?] = []
        guard let list = node?.u.list?.pointee,
              let values = list.values else { return array }

        for index in 0..<Int(list.num) {
            array.append(TypeHelper.nodeToAny(values[index]))
        }

        return array
    }

    // MPV_FORMAT_NODE > MPV_FORMAT_NODE_MAP
    class func nodeToDict(_ node: mpv_node?) -> [String: Any?] {
        var dict: [String: Any?] = [:]
        guard let list = node?.u.list?.pointee,
              let values = list.values else { return dict }

        for index in 0..<Int(list.num) {
            guard let keyPtr = list.keys?[index] else { continue }
            let key = String(cString: keyPtr)
            dict[key] = TypeHelper.nodeToAny(values[index])
        }

        return dict
    }

    // MPV_FORMAT_NODE
    class func nodeToAny(_ node: mpv_node) -> Any? {
        switch node.format {
        case MPV_FORMAT_STRING: return TypeHelper.nodeToString(node)
        case MPV_FORMAT_FLAG: return TypeHelper.nodeToBool(node)
        case MPV_FORMAT_INT64: return TypeHelper.nodeToInt(node)
        case MPV_FORMAT_DOUBLE: return TypeHelper.nodeToDouble(node)
        case MPV_FORMAT_NODE_ARRAY: return TypeHelper.nodeToArray(node)
        case MPV_FORMAT_NODE_MAP: return TypeHelper.nodeToDict(node)
        default: return nil
        }
    }
}
