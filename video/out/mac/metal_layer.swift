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

class MetalLayer: CAMetalLayer {
    unowned var common: MacCommon

    init(common com: MacCommon) {
        common = com
        super.init()

        //layer.framebufferOnly = false
        //layer.drawableSize = NSSize(width: 1024, height: 576)
        pixelFormat = .rgba16Float
        backgroundColor = NSColor.black.cgColor

        if #available(macOS 10.13, *) {
            displaySyncEnabled = true
        }
    }

    // necessary for when the layer containing window changes the screen
    override init(layer: Any) {
        guard let oldLayer = layer as? MetalLayer else {
            fatalError("init(layer: Any) passed an invalid layer")
        }
        common = oldLayer.common
        super.init()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
