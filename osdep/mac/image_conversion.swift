class ImageConversion {
    var global: UnsafeMutablePointer<mpv_global>
    var log: OpaquePointer

    init(_ g: UnsafeMutablePointer<mpv_global>, _ l: OpaquePointer) {
        global = g
        log = l
    }

    private func getColorspaceName(_ plcsp: pl_color_space, gray: Bool) -> CFString? {
        if gray {
            if plcsp.transfer == PL_COLOR_TRC_LINEAR {
                return CGColorSpace.linearGray
            } else if plcsp.transfer == PL_COLOR_TRC_GAMMA22 {
                return CGColorSpace.genericGrayGamma2_2
            }
        } else {
            switch plcsp.primaries {
            case PL_COLOR_PRIM_DISPLAY_P3:
                if plcsp.transfer == PL_COLOR_TRC_BT_1886 {
                    return CGColorSpace.displayP3
                } else if plcsp.transfer == PL_COLOR_TRC_HLG {
                    return CGColorSpace.displayP3_HLG
                }
            case PL_COLOR_PRIM_BT_709:
                if plcsp.transfer == PL_COLOR_TRC_LINEAR {
                    return CGColorSpace.linearSRGB
                } else if plcsp.transfer == PL_COLOR_TRC_BT_1886 {
                    return CGColorSpace.itur_709
                } else if plcsp.transfer == PL_COLOR_TRC_SRGB {
                    return CGColorSpace.sRGB
                }
            case PL_COLOR_PRIM_DCI_P3:
                if plcsp.transfer == PL_COLOR_TRC_BT_1886 {
                    return CGColorSpace.dcip3
                }
            case PL_COLOR_PRIM_BT_2020:
                if plcsp.transfer == PL_COLOR_TRC_BT_1886 {
                    return CGColorSpace.itur_2020
                }
            case PL_COLOR_PRIM_ADOBE:
                return CGColorSpace.adobeRGB1998
            case PL_COLOR_PRIM_APPLE:
                if plcsp.transfer == PL_COLOR_TRC_LINEAR {
                    return CGColorSpace.genericRGBLinear
                }
            default:
                break
            }
        }

        return nil
    }

    private func convertIntoRep(_ rep: NSBitmapImageRep, imgfmt: Int32, plcsp: pl_color_space, bps: Int32, image: mp_image) -> Bool {
        var image = image
        var dest = mp_image()
        mp_image_setfmt(&dest, imgfmt)
        mp_image_set_size(&dest, image.w, image.h)

        let planes = UnsafeMutablePointer<UnsafeMutablePointer<UInt8>?>.allocate(capacity: 5)
        rep.getBitmapDataPlanes(planes)

        if !withUnsafeMutableBytes(of: &dest.stride, { (stridePtr) -> Bool in
            return withUnsafeMutableBytes(of: &dest.planes) { (planesPtr) -> Bool in
                guard let destStrides = stridePtr.baseAddress?.assumingMemoryBound(to: type(of: image.stride.0)) else {
                    return false
                }
                guard let destPlanes = planesPtr.baseAddress?.assumingMemoryBound(to: type(of: image.planes.0)) else {
                    return false
                }

                for i in 0..<Int(MP_MAX_PLANES) {
                    destPlanes[i] = planes[i]
                    destStrides[i] = Int32(rep.bytesPerRow)
                }

                return true
            }
        }) {
            assert(false, "Binding pointer to stride or planes array failed; this should be impossible")
            return false
        }

        dest.params.repr.sys = PL_COLOR_SYSTEM_RGB
        dest.params.repr.levels = PL_COLOR_LEVELS_FULL
        dest.params.repr.alpha = rep.hasAlpha ? (rep.bitmapFormat.contains(.alphaNonpremultiplied) ? PL_ALPHA_INDEPENDENT : PL_ALPHA_PREMULTIPLIED) : PL_ALPHA_UNKNOWN
        dest.params.repr.bits.sample_depth = bps
        dest.params.repr.bits.color_depth = bps

        dest.params.color = plcsp

        return mp_image_swscale(&dest, &image, global, log) >= 0
    }

    private func createImageRep(_ image: mp_image) -> NSBitmapImageRep? {
        // Need it to nominally be mutable to pass to C functions later
        var image = image
        var imgfmt = image.imgfmt

        var compatible = true
        switch imgfmt {
        case IMGFMT_YAP8, IMGFMT_YAP16, IMGFMT_Y8, IMGFMT_Y16, IMGFMT_ARGB, IMGFMT_RGBA, IMGFMT_RGB0, IMGFMT_RGBA64:
            break
        default:
            compatible = false
        }

        if image.params.repr.levels != PL_COLOR_LEVELS_FULL {
            compatible = false
        }

        if image.num_planes > 5 {
            return nil
        }

        let planes = UnsafeMutablePointer<UnsafeMutablePointer<UInt8>?>.allocate(capacity: 5)
        planes.initialize(repeating: nil, count: 5)

        var bps = image.fmt.comps.0.size
        var spp = mp_imgfmt_desc_get_num_comps(&image.fmt)
        let alpha = (image.fmt.flags & MP_IMGFLAG_ALPHA) != 0
        let gray = (image.fmt.flags & MP_IMGFLAG_GRAY) != 0
        let csp: NSColorSpaceName = gray ? .calibratedWhite : .calibratedRGB
        var formatFlags: NSBitmapImageRep.Format = []
        if alpha && image.fmt.comps.3.plane == 0 && image.fmt.comps.3.offset == 0 {
            formatFlags.insert(.alphaFirst)
        }
        if image.params.repr.alpha == PL_ALPHA_INDEPENDENT {
            formatFlags.insert(.alphaNonpremultiplied)
        }
        var bpp = image.fmt.bpp.0
        var bytesPerRow = image.stride.0
        var planar = image.num_planes > 1

        if !withUnsafeBytes(of: &image.stride, { (rawPtr) -> Bool in
            let ptr = rawPtr.baseAddress!.assumingMemoryBound(to: type(of: image.stride.0))
            for i in 0..<Int(image.num_planes) where ptr[i] != bytesPerRow {
                return false
            }
            return true
        }) {
            compatible = false
        }

        if compatible {
            withUnsafeBytes(of: &image.planes) { (rawPtr) in
                let ptr = rawPtr.baseAddress!.assumingMemoryBound(to: type(of: image.planes.0))
                for i in 0..<Int(image.num_planes) {
                    planes[i] = ptr[i]
                }
            }

            if bpp == 24 {
                bpp = 32
            }
        } else {
            bps = bps <= 8 ? 8 : 16
            formatFlags.remove(.alphaFirst)
            if gray {
                if bps > 8 {
                    imgfmt = alpha ? IMGFMT_YAP16 : IMGFMT_Y16
                    bpp = 16
                } else {
                    imgfmt = alpha ? IMGFMT_YAP8 : IMGFMT_Y8
                    bpp = 8
                }
            } else {
                if bps > 8 {
                    imgfmt = IMGFMT_RGBA64
                    bpp = 64
                } else {
                    imgfmt = alpha ? IMGFMT_RGBA : IMGFMT_RGB0
                    bpp = 32
                }
            }

            bytesPerRow = 0
            planar = (gray && alpha)
            spp = (gray ? 1 : 3) + (alpha ? 1 : 0)
        }

        guard let rep = NSBitmapImageRep(bitmapDataPlanes: planes,
            pixelsWide: Int(image.w),
            pixelsHigh: Int(image.h),
            bitsPerSample: Int(bps),
            samplesPerPixel: Int(spp),
            hasAlpha: alpha,
            isPlanar: planar,
            colorSpaceName: csp,
            bitmapFormat: formatFlags,
            bytesPerRow: Int(bytesPerRow),
            bitsPerPixel: Int(bpp)) else {
            return nil
        }

        var plcsp = image.params.color
        let cgSpaceName = getColorspaceName(plcsp, gray: gray)

        if cgSpaceName == nil {
            compatible = false
            plcsp.primaries = PL_COLOR_PRIM_BT_709
            plcsp.transfer = PL_COLOR_TRC_SRGB
        }

        guard let nscsp = (!gray && image.icc_profile != nil) ?
            NSColorSpace(iccProfileData: Data(bytes: image.icc_profile.pointee.data, count: image.icc_profile.pointee.size)) :
            (CGColorSpace(name: cgSpaceName ?? CGColorSpace.sRGB).flatMap { NSColorSpace(cgColorSpace: $0) }) else {
            return nil
        }

        guard let rep = rep.retagging(with: nscsp) else {
            return nil
        }

        if !compatible && !convertIntoRep(rep, imgfmt: Int32(imgfmt.rawValue), plcsp: plcsp, bps: Int32(bps), image: image) {
            return nil
        }

        return rep
    }

    func convertImage(_ image: mp_image) -> CGImage? {
        guard let rep = createImageRep(image) else {
            return .none
        }

        return rep.cgImage
    }
}
