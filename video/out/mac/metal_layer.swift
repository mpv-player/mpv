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
            if pixelFormat != oldValue {
                log.verbose("Metal layer pixel format changed: \(pixelFormat.name)")
            }
        }
    }

    // workaround for nil to none-nil values, oldValue is same as current in those cases
    var previousColorspace: CGColorSpace?
    override var colorspace: CGColorSpace? {
        didSet {
            if colorspace != previousColorspace {
                log.verbose("Metal layer colorspace changed: \(colorspace?.longName ?? "nil")")
            }
            previousColorspace = colorspace
        }
    }

    override var edrMetadata: CAEDRMetadata? {
        didSet {
            if edrMetadata != oldValue {
                log.verbose("Metal layer HDR metadata changed: \(edrMetadata?.description ?? "nil")")
            }
        }
    }

    override var wantsExtendedDynamicRangeContent: Bool {
        didSet {
            if wantsExtendedDynamicRangeContent != oldValue {
                log.verbose("Metal layer HDR \(wantsExtendedDynamicRangeContent ? "active" : "inactive")")
            }
        }
    }

    override var displaySyncEnabled: Bool {
        didSet {
            if displaySyncEnabled != oldValue {
                log.verbose("Metal layer display sync \(displaySyncEnabled ? "active" : "inactive")")
            }
        }
    }

    // workaround for MoltenVK problem setting this to false even when no transparent content is rendered
    var wantsAlpha: Bool = false { didSet { isOpaque = !wantsAlpha } }
    override var isOpaque: Bool {
        get { return super.isOpaque }
        set {
            let isForced = newValue == wantsAlpha
            if isOpaque == wantsAlpha || isForced {
                super.isOpaque = !wantsAlpha
                backgroundColor = (wantsAlpha ? NSColor.clear : NSColor.black).cgColor
                log.verbose("Metal layer is opaque (direct-to-display possible): \(isOpaque)" + (isForced ? " (forced)" : ""))
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
}
