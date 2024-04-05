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

class AppHub: NSObject {
    @objc static let shared = AppHub()

    var mpv: OpaquePointer?
    var input: InputHelper
    var log: LogHelper
    var option: OptionHelper?
    var event: EventHelper?
    var menu: MenuBar?
#if HAVE_MACOS_MEDIA_PLAYER
    var remote: RemoteCommandCenter?
#endif
#if HAVE_MACOS_TOUCHBAR
    var touchBar: TouchBar?
#endif
#if HAVE_MACOS_COCOA_CB
    var cocoaCb: CocoaCB?
#endif

    let MPV_PROTOCOL: String = "mpv://"
    var isApplication: Bool { get { NSApp is Application } }
    var openEvents: Int = 0

    private override init() {
        input = InputHelper()
        log = LogHelper()
        super.init()
        if isApplication { menu = MenuBar(self) }
#if HAVE_MACOS_MEDIA_PLAYER
        remote = RemoteCommandCenter(self)
#endif
        log.verbose("AppHub initialised")
    }

    @objc func initMpv(_ mpv: OpaquePointer) {
        event = EventHelper(self, mpv)
        if let mpv = event?.mpv {
            self.mpv = mpv
            log.log = mp_log_new(UnsafeMutablePointer(mpv), mp_client_get_log(mpv), "app")
            option = OptionHelper(UnsafeMutablePointer(mpv), mp_client_get_global(mpv))
            input.option = option
        }

#if HAVE_MACOS_MEDIA_PLAYER
        remote?.registerEvents()
#endif
#if HAVE_MACOS_TOUCHBAR
        touchBar = TouchBar(self)
#endif
        log.verbose("AppHub functionality initialised")
    }

    @objc func initInput(_ input: OpaquePointer?) {
        log.verbose("Initialising Input")
        self.input.signal(input: input)
    }

    @objc func initCocoaCb() {
#if HAVE_MACOS_COCOA_CB
        if !isApplication { return }
        log.verbose("Initialising CocoaCB")
        DispatchQueue.main.sync {
            self.cocoaCb = self.cocoaCb ?? CocoaCB(mpv_create_client(mpv, "cocoacb"))
        }
#endif
    }

    @objc func startRemote() {
#if HAVE_MACOS_MEDIA_PLAYER
        log.verbose("Starting RemoteCommandCenter")
        remote?.start()
#endif
    }

    @objc func stopRemote() {
#if HAVE_MACOS_MEDIA_PLAYER
        log.verbose("Stoping RemoteCommandCenter")
        remote?.stop()
#endif
    }

    func open(urls: [URL]) {
        let files = urls.map {
            if $0.isFileURL { return $0.path }
            var path = $0.absoluteString
            if path.hasPrefix(MPV_PROTOCOL) { path.removeFirst(MPV_PROTOCOL.count) }
            return path.removingPercentEncoding ?? path
        }.sorted { (strL: String, strR: String) -> Bool in
            return strL.localizedStandardCompare(strR) == .orderedAscending
        }
        log.verbose("\(openEvents > 0 ? "Appending" : "Opening") dropped files: \(files)")
        input.open(files: files, append: openEvents > 0)
        openEvents += 1
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) { self.openEvents -= 1 }
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
