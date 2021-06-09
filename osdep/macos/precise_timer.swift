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

class PreciseTimer {
    unowned var common: Common
    var mpv: MPVHelper? { get { return common.mpv } }

    let condition = NSCondition()
    var events: [[String:Any]] = []
    var timebaseRatio: Double = 1.0
    var isRunning: Bool = true
    var isHighPrecision: Bool = false

    var thread: pthread_t?
    var threadPort: thread_port_t? = nil
    let typeNumber: mach_msg_type_number_t
    let policyFlavor = thread_policy_flavor_t(THREAD_TIME_CONSTRAINT_POLICY)
    let policyCount = MemoryLayout<thread_time_constraint_policy>.size /
                          MemoryLayout<integer_t>.size

    init(common com: Common) {
        common = com
        var timebase: mach_timebase_info = mach_timebase_info()
        var attr: pthread_attr_t = pthread_attr_t()
        var param: sched_param = sched_param()
        mach_timebase_info(&timebase)
        pthread_attr_init(&attr)

        typeNumber = mach_msg_type_number_t(policyCount)
        timebaseRatio = (Double(timebase.numer) / Double(timebase.denom)) / CVGetHostClockFrequency()
        param.sched_priority = sched_get_priority_max(SCHED_FIFO)
        pthread_attr_setschedparam(&attr, &param)
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO)
        pthread_create(&thread, &attr, entryC, MPVHelper.bridge(obj: self))
        threadPort = pthread_mach_thread_np(thread!)
    }

    func updatePolicy(refreshRate: Double = 60.0) {
        let period = UInt32(1.0 / refreshRate / timebaseRatio)
        var policy = thread_time_constraint_policy(
            period: period,
            computation: UInt32(200000),
            constraint:  period / 10,
            preemptible: 1
        )

        let success = withUnsafeMutablePointer(to: &policy) {
            $0.withMemoryRebound(to: integer_t.self, capacity: policyCount) {
                thread_policy_set(threadPort!, policyFlavor, $0, typeNumber)
            }
        }

        isHighPrecision = success == KERN_SUCCESS
        if !isHighPrecision {
            common.log.sendWarning("Couldn't create a high precision timer")
        }
    }

    func terminate() {
        condition.lock()
        isRunning = false
        condition.signal()
        condition.unlock()
        // TODO ! shit
        pthread_kill(thread!, SIGALRM)
        pthread_join(thread!, nil)
    }

    func scheduleAt(time: UInt64, closure: @escaping () -> () ) {
        condition.lock()
        let firstEventTime = events.first?["time"] as? UInt64 ?? 0
        let lastEventTime = events.last?["time"] as? UInt64 ?? 0
        events.append(["time": time, "closure": closure])

        if lastEventTime > time {
            events.sort{ ($0["time"] as! UInt64) < ($1["time"] as! UInt64) }
        }

        condition.signal()
        condition.unlock()

        if firstEventTime > time {
            pthread_kill(thread!, SIGALRM)
        }
    }

    let threadSignal: @convention(c) (Int32) -> () = { (sig: Int32) in }

    let entryC: @convention(c) (UnsafeMutableRawPointer) -> UnsafeMutableRawPointer? = { (ptr: UnsafeMutableRawPointer) in
        let ptimer: PreciseTimer = MPVHelper.bridge(ptr: ptr)
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

            let event = events.first
            condition.unlock()

            let time = event?["time"] as! UInt64
            let closure = event?["closure"] as! () -> ()

            mach_wait_until(time)

            condition.lock()
            if (events.first?["time"] as! UInt64) == time && isRunning {
                closure()
                events.removeFirst()
            }
            condition.unlock()
        }
    }

}
