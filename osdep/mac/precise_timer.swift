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

struct Timing {
    let time: UInt64
    let closure: () -> Void
}

class PreciseTimer {
    unowned var common: Common

    let nanoPerSecond: Double = 1e+9
    let machToNano: Double = {
        var timebase: mach_timebase_info = mach_timebase_info()
        mach_timebase_info(&timebase)
        return Double(timebase.numer) / Double(timebase.denom)
    }()

    let condition = NSCondition()
    var events: [Timing] = []
    var isRunning: Bool = true
    var isHighPrecision: Bool = false

    var thread: pthread_t!
    var threadPort: thread_port_t = thread_port_t()
    let policyFlavor = thread_policy_flavor_t(THREAD_TIME_CONSTRAINT_POLICY)
    let policyCount = MemoryLayout<thread_time_constraint_policy>.size /
                          MemoryLayout<integer_t>.size
    var typeNumber: mach_msg_type_number_t {
        return mach_msg_type_number_t(policyCount)
    }
    var threadAttr: pthread_attr_t = {
        var attr = pthread_attr_t()
        var param = sched_param()
        pthread_attr_init(&attr)
        param.sched_priority = sched_get_priority_max(SCHED_FIFO)
        pthread_attr_setschedparam(&attr, &param)
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO)
        return attr
    }()

    init?(common com: Common) {
        common = com

        pthread_create(&thread, &threadAttr, entryC, TypeHelper.bridge(obj: self))
        if thread == nil {
            common.log.warning("Couldn't create pthread for high precision timer")
            return nil
        }

        threadPort = pthread_mach_thread_np(thread)
    }

    func updatePolicy(periodSeconds: Double = 1 / 60.0) {
        let period = periodSeconds * nanoPerSecond / machToNano
        var policy = thread_time_constraint_policy(
            period: UInt32(period),
            computation: UInt32(0.75 * period),
            constraint: UInt32(0.85 * period),
            preemptible: 1
        )

        let success = withUnsafeMutablePointer(to: &policy) {
            $0.withMemoryRebound(to: integer_t.self, capacity: policyCount) {
                thread_policy_set(threadPort, policyFlavor, $0, typeNumber)
            }
        }

        isHighPrecision = success == KERN_SUCCESS
        if !isHighPrecision {
            common.log.warning("Couldn't create a high precision timer")
        }
    }

    func terminate() {
        condition.lock()
        isRunning = false
        condition.signal()
        condition.unlock()
        pthread_kill(thread, SIGALRM)
        pthread_join(thread, nil)
    }

    func scheduleAt(time: UInt64, closure: @escaping () -> Void) {
        condition.lock()
        let firstEventTime = events.first?.time ?? 0
        let lastEventTime = events.last?.time ?? 0
        events.append(Timing(time: time, closure: closure))

        if lastEventTime > time {
            events.sort { $0.time < $1.time }
        }

        condition.signal()
        condition.unlock()

        if firstEventTime > time {
            pthread_kill(thread, SIGALRM)
        }
    }

    let threadSignal: @convention(c) (Int32) -> Void = { (_ sig: Int32) in }

    let entryC: @convention(c) (UnsafeMutableRawPointer) -> UnsafeMutableRawPointer? = { (ptr: UnsafeMutableRawPointer) in
        let ptimer: PreciseTimer = TypeHelper.bridge(ptr: ptr)
        ptimer.entry()
        return nil
    }

    func entry() {
        signal(SIGALRM, threadSignal)

        while isRunning {
            condition.lock()
            while events.count == 0 && isRunning {
                condition.wait()
            }

            if !isRunning { break }

            guard let event = events.first else {
                continue
            }
            condition.unlock()

            mach_wait_until(event.time)

            condition.lock()
            if events.first?.time == event.time && isRunning {
                event.closure()
                events.removeFirst()
            }
            condition.unlock()
        }
    }
}
