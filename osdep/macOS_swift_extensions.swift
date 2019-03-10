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

extension NSScreen {

    public var displayID: CGDirectDisplayID {
        get {
            return deviceDescription["NSScreenNumber"] as! CGDirectDisplayID
        }
    }

    public var displayName: String? {
        get {
            var name: String? = nil
            var object: io_object_t
            var iter = io_iterator_t()
            let matching = IOServiceMatching("IODisplayConnect")
            let result = IOServiceGetMatchingServices(kIOMasterPortDefault, matching, &iter)

            if result != KERN_SUCCESS || iter == 0 { return nil }

            repeat {
                object = IOIteratorNext(iter)
                let info = IODisplayCreateInfoDictionary(object, IOOptionBits(kIODisplayOnlyPreferredName)).takeRetainedValue() as! [String:AnyObject]
                if (info[kDisplayVendorID] as? UInt32 == CGDisplayVendorNumber(displayID) &&
                    info[kDisplayProductID] as? UInt32 == CGDisplayModelNumber(displayID) &&
                    info[kDisplaySerialNumber] as? UInt32 ?? 0 == CGDisplaySerialNumber(displayID))
                {
                    if let productNames = info["DisplayProductName"] as? [String:String],
                       let productName = productNames.first?.value
                    {
                        name = productName
                        break
                    }
                }
            } while object != 0

            IOObjectRelease(iter)
            return name
        }
    }
}
