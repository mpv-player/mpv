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

protocol EventSubscriber: AnyObject {
    var uid: Int { get }
    func handle(event: EventHelper.Event)
}

extension EventSubscriber {
    var uid: Int { return Int(bitPattern: ObjectIdentifier(self)) }
}

extension EventHelper {
    typealias WakeupCallback = (@convention(c) (UnsafeMutableRawPointer?) -> Void)?

    struct Event {
        var id: String {
            return name + (name.starts(with: "MPV_EVENT_") ? "" : String(format.rawValue))
        }
        var idReset: String {
            return name + (name.starts(with: "MPV_EVENT_") ? "" : String(MPV_FORMAT_NONE.rawValue))
        }
        let name: String
        let format: mpv_format
        let string: String?
        let bool: Bool?
        let double: Double?

        init(
            name: String = "",
            format: mpv_format = MPV_FORMAT_NONE,
            string: String? = nil,
            bool: Bool? = nil,
            double: Double? = nil

        ) {
            self.name = name
            self.format = format
            self.string = string
            self.bool = bool
            self.double = double
        }
    }
}

class EventHelper {
    unowned let appHub: AppHub
    var mpv: OpaquePointer?
    var events: [String: [Int: EventSubscriber]] = [:]

    init?(_ appHub: AppHub, _ mpv: OpaquePointer) {
        if !appHub.isApplication {
            mpv_destroy(mpv)
            return nil
        }

        self.appHub = appHub
        self.mpv = mpv
        mpv_set_wakeup_callback(mpv, wakeup, TypeHelper.bridge(obj: self))
    }

    func subscribe(_ subscriber: EventSubscriber, event: Event) {
        guard let mpv = mpv else { return }

        if !event.name.isEmpty {
            if !events.keys.contains(event.idReset) {
                events[event.idReset] = [:]
            }
            if !events.keys.contains(event.id) {
                mpv_observe_property(mpv, 0, event.name, event.format)
                events[event.id] = [:]
            }
            events[event.idReset]?[subscriber.uid] = subscriber
            events[event.id]?[subscriber.uid] = subscriber
        }
    }

    let wakeup: WakeupCallback = { ( ctx ) in
        let event = unsafeBitCast(ctx, to: EventHelper.self)
        DispatchQueue.main.async { event.eventLoop() }
    }

    func eventLoop() {
        while let mpv = mpv, let event = mpv_wait_event(mpv, 0) {
            if event.pointee.event_id == MPV_EVENT_NONE { break }
            handle(event: event)
        }
    }

    func handle(event: UnsafeMutablePointer<mpv_event>) {
        switch event.pointee.event_id {
        case MPV_EVENT_PROPERTY_CHANGE:
            handle(property: event)
        default:
            for (_, subscriber) in events[String(describing: event.pointee.event_id)] ?? [:] {
                subscriber.handle(event: .init(name: String(describing: event.pointee.event_id)))
            }
        }

        if event.pointee.event_id == MPV_EVENT_SHUTDOWN {
            mpv_destroy(mpv)
            mpv = nil
        }
    }

    func handle(property mpvEvent: UnsafeMutablePointer<mpv_event>) {
        let pData = OpaquePointer(mpvEvent.pointee.data)
        guard let property = UnsafePointer<mpv_event_property>(pData)?.pointee else {
            return
        }

        let name = String(cString: property.name)
        let format = property.format
        for (_, subscriber) in events[name + String(format.rawValue)] ?? [:] {
            var event: Event?
            switch format {
            case MPV_FORMAT_STRING:
                event = .init(name: name, format: format, string: TypeHelper.toString(property.data))
            case MPV_FORMAT_FLAG:
                event = .init(name: name, format: format, bool: TypeHelper.toBool(property.data))
            case MPV_FORMAT_DOUBLE:
                event = .init(name: name, format: format, double: TypeHelper.toDouble(property.data))
            case MPV_FORMAT_NONE:
                event = .init(name: name, format: format)
            default: break
            }

            if let e = event { subscriber.handle(event: e) }
        }
    }
}
