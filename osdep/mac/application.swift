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
    var appHub: AppHub { return AppHub.shared }
    var eventManager: NSAppleEventManager { return NSAppleEventManager.shared() }
    var playbackThreadId: mp_thread!
    var argc: Int32?
    var argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?

    override init() {
        super.init()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
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

    func application(_ application: NSApplication, open urls: [URL]) {
        appHub.open(urls: urls)
    }

    func applicationWillFinishLaunching(_ notification: Notification) {
        // register quit and exit events
        eventManager.setEventHandler(
            self,
            andSelector: #selector(handleQuit(event:replyEvent:)),
            forEventClass: AEEventClass(kCoreEventClass),
            andEventID: kAEQuitApplication
        )
        atexit_b({
            // clean up after exit() was called
            DispatchQueue.main.async {
                NSApp.hide(NSApp)
                NSApp.setActivationPolicy(.prohibited)
                self.eventManager.removeEventHandler(forEventClass: AEEventClass(kCoreEventClass), andEventID: kAEQuitApplication)
            }
        })
    }

    // quit from App icon, external quit from NSWorkspace
    @objc func handleQuit(event: NSAppleEventDescriptor?, replyEvent: NSAppleEventDescriptor?) {
        // send quit to core, terminates mpv_main called in playbackThread,
        if !appHub.input.command("quit") {
            appHub.log.warning("Could not properly shut down mpv")
            exit(1)
        }
    }

    func setupBundle() {
        if !appHub.isBundle { return }

        // started from finder the first argument after the binary may start with -psn_
        if CommandLine.argc > 1 && CommandLine.arguments[1].hasPrefix("-psn_") {
            argc? = 1
            argv?[1] = nil
        }
    }

    let playbackThread: @convention(c) (UnsafeMutableRawPointer) -> UnsafeMutableRawPointer? = { (ptr: UnsafeMutableRawPointer) in
        let application: Application = TypeHelper.bridge(ptr: ptr)
        mp_thread_set_name("core/playback")
        let exitCode: Int32 = mpv_main(application.argc ?? 1, application.argv)
        // exit of any proper shut down
        exit(exitCode)
    }

    @objc func main(_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>) -> Int {
        self.argc = argc
        self.argv = argv

        NSApp = self
        NSApp.delegate = self
        NSApp.setActivationPolicy(appHub.isBundle ? .regular : .accessory)
        setupBundle()
        pthread_create(&playbackThreadId, nil, playbackThread, TypeHelper.bridge(obj: self))
        appHub.input.wait()
        NSApp.run()

        // should never be reached
        print("""
            There was either a problem initializing Cocoa or the Runloop was stopped unexpectedly. \
            Please report this issues to a developer.\n
        """)
        pthread_join(playbackThreadId, nil)
        return 1
    }
}
