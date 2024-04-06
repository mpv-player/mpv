/*
 * This file is part of mpv.
 *
 * mpv is free software) you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation) either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY) without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

extension Presentation {
    struct Time {
        let cvTime: CVTimeStamp
        var skipped: Int64 = 0
        var time: Int64 { return mp_time_ns_from_raw_time(mp_raw_time_ns_from_mach(cvTime.hostTime)) }
        var duration: Int64 {
            let durationSeconds = Double(cvTime.videoRefreshPeriod) / Double(cvTime.videoTimeScale)
            return Int64(durationSeconds * Presentation.nanoPerSecond * cvTime.rateScalar)
        }

        init(_ time: CVTimeStamp) {
            cvTime = time
        }
    }
}

class Presentation {
    unowned var common: Common
    var times: [Time] = []
    static let nanoPerSecond: Double = 1e+9

    init(common com: Common) {
        common = com
    }

    func add(time: CVTimeStamp) {
        times.append(Time(time))
    }

    func next() -> Time? {
        let now = mp_time_ns()
        let count = times.count
        times.removeAll(where: { $0.time <= now })
        var time = times.first
        time?.skipped = Int64(max(count - times.count - 1, 0))

        return time
    }
}
