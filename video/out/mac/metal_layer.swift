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
import QuartzCore

class MetalLayer: CAMetalLayer {
    unowned var common: MacCommon
    var log: LogHelper { return common.log }

    // workaround for a MoltenVK workaround that sets the drawableSize to 1x1 to forcefully complete
    // the presentation, this causes flicker and the drawableSize possibly staying at 1x1
    override var drawableSize: CGSize {
        get { return super.drawableSize }
        set {
            if Int(newValue.width) > 1 && Int(newValue.height) > 1 {
                super.drawableSize = newValue
            }
        }
    }

    override var pixelFormat: MTLPixelFormat {
        didSet {
            if pixelFormat != oldValue { logState(property: "pixel format") }
        }
    }

    // workaround for nil to none-nil values, oldValue is same as current in those cases
    var previousColorspace: CGColorSpace?
    override var colorspace: CGColorSpace? {
        didSet {
            if colorspace != previousColorspace { logState(property: "colorspace") }
            previousColorspace = colorspace
        }
    }

    override var edrMetadata: CAEDRMetadata? {
        didSet {
            if edrMetadata != oldValue { logState(property: "HDR metadata") }
        }
    }

    override var wantsExtendedDynamicRangeContent: Bool {
        didSet {
            if wantsExtendedDynamicRangeContent != oldValue { logState(property: "HDR") }
        }
    }

    override var displaySyncEnabled: Bool {
        didSet {
            if displaySyncEnabled != oldValue { logState(property: "display sync") }
        }
    }

    // workaround for MoltenVK problem setting this to false even when no transparent content is rendered
    var wantsAlpha: Bool = false { didSet { isOpaque = !wantsAlpha } }
    var opaqueForced: Bool = false
    override var isOpaque: Bool {
        get { return super.isOpaque }
        set {
            opaqueForced = newValue == wantsAlpha
            if isOpaque == wantsAlpha || opaqueForced {
                super.isOpaque = !wantsAlpha
                backgroundColor = (wantsAlpha ? NSColor.clear : NSColor.black).cgColor
                logState(property: "opaque")
            }
        }
    }

    init(common com: MacCommon) {
        common = com
        super.init()

        pixelFormat = .rgba16Float
        previousColorspace = colorspace
        backgroundColor = NSColor.black.cgColor
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

    func logState(property: String) {
        let dtdPixelFormats: [MTLPixelFormat] = [
            .bgra8Unorm,
            .rgba16Float,
            .bgr10a2Unorm
        ]
        let dtdPossible = isOpaque && dtdPixelFormats.contains(pixelFormat)

        log.verbose("""
        Metal layer state changed (\(property))
        Pixel Format: \(pixelFormat.name) - Opaque: \(isOpaque) \(opaqueForced ? "(forced)" : "")
        Colorspace: \(colorspace?.longName ?? "nil")
        HDR: \(wantsExtendedDynamicRangeContent ? "active" : "inactive") - Metadata: \(edrMetadata?.description ?? "nil")
        Direct-to-Display: \(dtdPossible ? "possible" : "inactive") - Display Sync: \(displaySyncEnabled ? "active" : "inactive")
        """)
    }
}
