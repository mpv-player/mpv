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

#if !HAVE_MACOS_10_14_FEATURES
extension NSAppearance.Name {
    static let darkAqua: NSAppearance.Name = NSAppearance.Name(rawValue: "NSAppearanceNameDarkAqua")
    static let accessibilityHighContrastAqua: NSAppearance.Name = NSAppearance.Name(rawValue: "NSAppearanceNameAccessibilityAqua")
    static let accessibilityHighContrastDarkAqua: NSAppearance.Name = NSAppearance.Name(rawValue: "NSAppearanceNameAccessibilityDarkAqua")
    static let accessibilityHighContrastVibrantLight: NSAppearance.Name = NSAppearance.Name(rawValue: "NSAppearanceNameAccessibilityVibrantLight")
    static let accessibilityHighContrastVibrantDark: NSAppearance.Name = NSAppearance.Name(rawValue: "NSAppearanceNameAccessibilityVibrantDark")
}

@available(OSX 10.12, *)
extension String {
    static let RGBA16Float: String = kCAContentsFormatRGBA16Float
    static let RGBA8Uint: String = kCAContentsFormatRGBA8Uint
    static let gray8Uint: String = kCAContentsFormatGray8Uint
}
#endif

extension NSPasteboard.PasteboardType {
    static let fileURLCompat: NSPasteboard.PasteboardType = {
        if #available(OSX 10.13, *) {
            return .fileURL
        } else {
            return NSPasteboard.PasteboardType(kUTTypeURL as String)
        }
    } ()

    static let URLCompat: NSPasteboard.PasteboardType = {
        if #available(OSX 10.13, *) {
            return .URL
        } else {
            return NSPasteboard.PasteboardType(kUTTypeFileURL as String)
        }
    } ()
}

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
        get { return draggingPasteboard() }
    }
}
#endif
