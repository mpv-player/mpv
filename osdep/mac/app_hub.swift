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

class AppHub: NSObject {
    @objc static let shared = AppHub()

    var mpv: OpaquePointer?
    @objc var input: InputHelper
    var event: EventHelper?
#if HAVE_MACOS_MEDIA_PLAYER
    var remote: RemoteCommandCenter?
#endif
#if HAVE_MACOS_TOUCHBAR
    @objc var touchBar: TouchBar?
#endif

    var isApplication: Bool { get { NSApp is Application } }

    private override init() {
        input = InputHelper()
        super.init()
#if HAVE_MACOS_MEDIA_PLAYER
        remote = RemoteCommandCenter(self)
#endif
    }

    @objc func initMpv(_ mpv: OpaquePointer) {
        event = EventHelper(self, mpv)
        self.mpv = event?.mpv

#if HAVE_MACOS_MEDIA_PLAYER
        remote?.registerEvents()
#endif
#if HAVE_MACOS_TOUCHBAR
        touchBar = TouchBar(self)
#endif
    }

    @objc func initInput(_ input: OpaquePointer?) {
        self.input.signal(input: input)
    }

    @objc func initCocoaCb() {
        guard let app = NSApp as? Application, let mpv = mpv else { return }
        DispatchQueue.main.sync { app.initCocoaCb(mpv) }
    }

    @objc func startRemote() {
#if HAVE_MACOS_MEDIA_PLAYER
        remote?.start()
#endif
    }

    @objc func stopRemote() {
#if HAVE_MACOS_MEDIA_PLAYER
        remote?.stop()
#endif
    }

    func getIcon() -> NSImage {
        guard let iconData = app_bridge_icon(), let icon = NSImage(data: iconData) else {
            return NSImage(size: NSSize(width: 1, height: 1))
        }
        return icon
    }

    func getMacConf() -> UnsafePointer<m_sub_options>? {
        return app_bridge_mac_conf()
    }

    func getVoConf() -> UnsafePointer<m_sub_options>? {
        return app_bridge_vo_conf()
    }
}
