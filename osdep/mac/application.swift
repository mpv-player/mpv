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
    var appHub: AppHub { get { return AppHub.shared } }
    var playbackThreadId: mp_thread!
    var argc: Int32?
    var argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?

    override init() {
        super.init()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    deinit {
        let eventManager = NSAppleEventManager.shared()
        eventManager.removeEventHandler(forEventClass: AEEventClass(kCoreEventClass), andEventID: kAEQuitApplication)
    }

    func initApplication(_ regular: Bool) {
        NSApp = self
        NSApp.delegate = self

        // Will be set to Regular from cocoa_common during UI creation so that we
        // don't create an icon when playing audio only files.
        NSApp.setActivationPolicy(regular ? .regular : .accessory)

        atexit_b({
            // Because activation policy has just been set to behave like a real
            // application, that policy must be reset on exit to prevent, among
            // other things, the menubar created here from remaining on screen.
            DispatchQueue.main.async { NSApp.setActivationPolicy(.prohibited) }
        })
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

    func application(_ application: NSApplication, open urls: [URL]) {
        appHub.open(urls: urls)
    }

    func applicationWillFinishLaunching(_ notification: Notification) {
        let eventManager = NSAppleEventManager.shared()
        eventManager.setEventHandler(
            self,
            andSelector: #selector(handleQuit(event:replyEvent:)),
            forEventClass: AEEventClass(kCoreEventClass),
            andEventID: kAEQuitApplication
        )
    }

    // quit from App icon
    @objc func handleQuit(event: NSAppleEventDescriptor?, replyEvent: NSAppleEventDescriptor?) {
        if !appHub.input.command("quit") {
            terminateApplication()
        }
    }

    func bundleStartedFromFinder() -> Bool {
        return ProcessInfo.processInfo.environment["MPVBUNDLE"] == "true"
    }

    func setupBundle() {
        // started from finder the first argument after the binary may start with -psn_
        // remove it and all following
        if CommandLine.argc > 1 && CommandLine.arguments[1].hasPrefix("-psn_") {
            argc? = 1
            argv?[1] = nil
        }

        let path = (ProcessInfo.processInfo.environment["PATH"] ?? "") +
            ":/usr/local/bin:/usr/local/sbin:/opt/local/bin:/opt/local/sbin"
        _ = path.withCString { setenv("PATH", $0, 1) }
    }

    let playbackThread: @convention(c) (UnsafeMutableRawPointer) -> UnsafeMutableRawPointer? = { (ptr: UnsafeMutableRawPointer) in
        let application: Application = TypeHelper.bridge(ptr: ptr)
        mp_thread_set_name("core/playback")
        let r: Int32 = mpv_main(application.argc ?? 1, application.argv)
        application.terminateApplication()
        // normally never reached - unless the cocoa mainloop hasn't started yet
        exit(r)
    }

    @objc func main(_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>) -> Int {
        self.argc = argc
        self.argv = argv

        if bundleStartedFromFinder() {
            setupBundle()
            initApplication(true)
        } else {
            initApplication(false)
        }

        pthread_create(&playbackThreadId, nil, playbackThread, TypeHelper.bridge(obj: self))
        appHub.input.wait()
        NSApp.run()

        // This should never be reached: NSApp.run() blocks until the process is quit
        print("""
            There was either a problem initializing Cocoa or the Runloop was stopped unexpectedly. \
            Please report this issues to a developer.\n
        """)
        pthread_join(playbackThreadId, nil)
        return 1
    }
}
