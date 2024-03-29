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

class Application: NSApplication, NSApplicationDelegate {
    let appHub: AppHub
    let MPV_PROTOCOL: String = "mpv://"
    @objc var openCount: Int = 0

    override init() {
        appHub = AppHub.shared
        super.init()

        let eventManager = NSAppleEventManager.shared()
        eventManager.setEventHandler(
            self,
            andSelector: #selector(self.getUrl(event:replyEvent:)),
            forEventClass: AEEventClass(kInternetEventClass),
            andEventID: AEEventID(kAEGetURL)
        )
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    deinit {
        let eventManager = NSAppleEventManager.shared()
        eventManager.removeEventHandler(forEventClass: AEEventClass(kInternetEventClass), andEventID: AEEventID(kAEGetURL))
        eventManager.removeEventHandler(forEventClass: AEEventClass(kCoreEventClass), andEventID: kAEQuitApplication)
    }

    func terminateApplication() {
        DispatchQueue.main.async {
            NSApp.hide(NSApp)
            NSApp.terminate(NSApp)
        }
    }

    override func sendEvent(_ event: NSEvent) {
        if modalWindow != nil || !appHub.input.processKey(event: event) {
            super.sendEvent(event)
        }
        appHub.input.wakeup()
    }

#if HAVE_MACOS_TOUCHBAR
    override func makeTouchBar() -> NSTouchBar? {
        return appHub.touchBar
    }
#endif

    func applicationWillFinishLaunching(_ notification: Notification) {
        let eventManager = NSAppleEventManager.shared()
        eventManager.setEventHandler(
            self,
            andSelector: #selector(handleQuit(event:replyEvent:)),
            forEventClass: AEEventClass(kCoreEventClass),
            andEventID: kAEQuitApplication
        )
    }

    @objc func handleQuit(event: NSAppleEventDescriptor?, replyEvent: NSAppleEventDescriptor?) {
        if !appHub.input.command("quit") {
            terminateApplication()
        }
    }

    @objc func getUrl(event: NSAppleEventDescriptor?, replyEvent: NSAppleEventDescriptor?) {
        guard var url: String = event?.paramDescriptor(forKeyword: keyDirectObject)?.stringValue else { return }

        if url.hasPrefix(MPV_PROTOCOL) {
            url.removeFirst(MPV_PROTOCOL.count)
        }

        url = url.removingPercentEncoding ?? url
        appHub.input.open(files: [url])
    }

    func application(_ sender: NSApplication, openFiles: [String]) {
        if openCount > 0 {
            openCount -= openFiles.count
            return
        }

        let files = openFiles.sorted { (strL: String, strR: String) -> Bool in
            return strL.localizedStandardCompare(strR) == .orderedAscending
        }
        appHub.input.open(files: files)
    }
}
