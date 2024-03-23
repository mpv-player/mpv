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

    var isApplication: Bool { get { NSApp is Application } }

    private override init() {
        input = InputHelper()
    }

    @objc func initMpv(_ mpv: OpaquePointer) {
        self.mpv = mpv
        mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE)
        mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE)
        mpv_observe_property(mpv, 0, "speed", MPV_FORMAT_DOUBLE)
        mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG)
        mpv_observe_property(mpv, 0, "media-title", MPV_FORMAT_STRING)
        mpv_observe_property(mpv, 0, "chapter-metadata/title", MPV_FORMAT_STRING)
        mpv_observe_property(mpv, 0, "metadata/by-key/album", MPV_FORMAT_STRING)
        mpv_observe_property(mpv, 0, "metadata/by-key/artist", MPV_FORMAT_STRING)
        event = EventHelper(mpv)
    }

    @objc func initInput(_ input: OpaquePointer?) {
        self.input.signal(input: input)
    }

    @objc func initCocoaCb() {
        guard let app = NSApp as? Application else { return }
        DispatchQueue.main.sync { app.initCocoaCb(mpv) }
    }

    @objc func startRemote() {
#if HAVE_MACOS_MEDIA_PLAYER
        if remote == nil { remote = RemoteCommandCenter() }
        remote?.start()
#endif
    }

    @objc func stopRemote() {
#if HAVE_MACOS_MEDIA_PLAYER
        remote?.stop()
#endif
    }

    let wakeup: EventHelper.wakeup_cb = { ( ctx ) in
        let event = unsafeBitCast(ctx, to: AppHub.self)
        DispatchQueue.main.async { event.eventLoop() }
    }

    func eventLoop() {
        while let mpv = mpv, let event = mpv_wait_event(mpv, 0) {
            if event.pointee.event_id == MPV_EVENT_NONE { break }
            handle(event: event)
        }
    }

    func handle(event: UnsafeMutablePointer<mpv_event>) {
        if let app = NSApp as? Application {
            app.processEvent(event)
        }

#if HAVE_MACOS_MEDIA_PLAYER
        if let remote = remote {
            remote.processEvent(event)
        }
#endif

        switch event.pointee.event_id {
        case MPV_EVENT_SHUTDOWN:
            mpv_destroy(mpv)
            mpv = nil
        default: break
        }
    }
}
