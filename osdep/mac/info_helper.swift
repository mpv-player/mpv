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

class InfoHelper {
    static var mpvVersion: String { return String(cString: swift_mpv_version) }
    static var mpvCopyright: String { return String(cString: swift_mpv_copyright) }

    static var libplaceboVersion: String { return String(cString: app_bridge_pl_version()) }

    static var ffmpegVersion: String { return String(FFMPEG_VERSION) }
    static var libavcodecVersion: String { return "\(LIBAVCODEC_VERSION_MAJOR).\(LIBAVCODEC_VERSION_MINOR).\(LIBAVCODEC_VERSION_MICRO)" }
    static var libavfilterVersion: String { return "\(LIBAVFILTER_VERSION_MAJOR).\(LIBAVFILTER_VERSION_MINOR).\(LIBAVFILTER_VERSION_MICRO)" }
    static var libavformatVersion: String { return "\(LIBAVFORMAT_VERSION_MAJOR).\(LIBAVFORMAT_VERSION_MINOR).\(LIBAVFORMAT_VERSION_MICRO)" }
    static var libavutilVersion: String { return "\(LIBAVUTIL_VERSION_MAJOR).\(LIBAVUTIL_VERSION_MINOR).\(LIBAVUTIL_VERSION_MICRO)" }
    static var libswresampleVersion: String { return "\(LIBSWRESAMPLE_VERSION_MAJOR).\(LIBSWRESAMPLE_VERSION_MINOR).\(LIBSWRESAMPLE_VERSION_MICRO)" }
    static var libswscaleVersion: String { return "\(LIBSWSCALE_VERSION_MAJOR).\(LIBSWSCALE_VERSION_MINOR).\(LIBSWSCALE_VERSION_MICRO)" }
    static var libavdeviceVersion: String? {
#if HAVE_LIBAVDEVICE
        return "\(LIBAVDEVICE_VERSION_MAJOR).\(LIBAVDEVICE_VERSION_MINOR).\(LIBAVDEVICE_VERSION_MICRO)"
#else
        return nil
#endif
    }
}
