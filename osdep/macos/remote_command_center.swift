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

import MediaPlayer

class RemoteCommandCenter: NSObject {
    enum KeyType {
        case normal
        case repeatable
    }

    struct Config {
        let key: Int32
        let type: KeyType
        var state: UInt32 = 0

        init(key: Int32, type: KeyType = .normal) {
            self.key = key
            self.type = type
        }
    }

    var configs: [MPRemoteCommand:Config] = [
        MPRemoteCommandCenter.shared().pauseCommand: Config(key: MP_KEY_PAUSEONLY),
        MPRemoteCommandCenter.shared().playCommand: Config(key: MP_KEY_PLAYONLY),
        MPRemoteCommandCenter.shared().stopCommand: Config(key: MP_KEY_STOP),
        MPRemoteCommandCenter.shared().nextTrackCommand: Config(key: MP_KEY_NEXT),
        MPRemoteCommandCenter.shared().previousTrackCommand: Config(key: MP_KEY_PREV),
        MPRemoteCommandCenter.shared().togglePlayPauseCommand: Config(key: MP_KEY_PLAY),
        MPRemoteCommandCenter.shared().seekForwardCommand:
            Config(key: MP_KEY_FORWARD, type: .repeatable),
        MPRemoteCommandCenter.shared().seekBackwardCommand:
            Config(key: MP_KEY_REWIND, type: .repeatable)
    ]

    var nowPlayingInfo: [String: Any] = [
        MPNowPlayingInfoPropertyMediaType: NSNumber(value: MPNowPlayingInfoMediaType.video.rawValue),
        MPNowPlayingInfoPropertyDefaultPlaybackRate: NSNumber(value: 1),
        MPNowPlayingInfoPropertyPlaybackProgress: NSNumber(value: 0.0),
        MPMediaItemPropertyPlaybackDuration: NSNumber(value: 0),
        MPMediaItemPropertyTitle: "mpv",
        MPMediaItemPropertyAlbumTitle: "mpv",
        MPMediaItemPropertyArtist: "mpv",
    ]

    let disabledCommands: [MPRemoteCommand] = [
        MPRemoteCommandCenter.shared().changePlaybackRateCommand,
        MPRemoteCommandCenter.shared().changeRepeatModeCommand,
        MPRemoteCommandCenter.shared().changeShuffleModeCommand,
        MPRemoteCommandCenter.shared().skipForwardCommand,
        MPRemoteCommandCenter.shared().skipBackwardCommand,
        MPRemoteCommandCenter.shared().changePlaybackPositionCommand,
        MPRemoteCommandCenter.shared().enableLanguageOptionCommand,
        MPRemoteCommandCenter.shared().disableLanguageOptionCommand,
        MPRemoteCommandCenter.shared().ratingCommand,
        MPRemoteCommandCenter.shared().likeCommand,
        MPRemoteCommandCenter.shared().dislikeCommand,
        MPRemoteCommandCenter.shared().bookmarkCommand,
    ]

    var mpInfoCenter: MPNowPlayingInfoCenter { get { return MPNowPlayingInfoCenter.default() } }
    var isPaused: Bool = false { didSet { updatePlaybackState() } }

    @objc override init() {
        super.init()

        for cmd in disabledCommands {
            cmd.isEnabled = false
        }
    }

    @objc func start() {
        for (cmd, _) in configs {
            cmd.isEnabled = true
            cmd.addTarget { [unowned self] event in
                return self.cmdHandler(event)
            }
        }

        if let app = NSApp as? Application, let icon = app.getMPVIcon() {
            let albumArt = MPMediaItemArtwork(boundsSize: icon.size) { _ in
                return icon
            }
            nowPlayingInfo[MPMediaItemPropertyArtwork] = albumArt
        }

        mpInfoCenter.nowPlayingInfo = nowPlayingInfo
        mpInfoCenter.playbackState = .playing

        NotificationCenter.default.addObserver(
            self,
            selector: #selector(self.makeCurrent),
            name: NSApplication.willBecomeActiveNotification,
            object: nil
        )
    }

    @objc func stop() {
        for (cmd, _) in configs {
            cmd.isEnabled = false
            cmd.removeTarget(nil)
        }

        mpInfoCenter.nowPlayingInfo = nil
        mpInfoCenter.playbackState = .unknown
    }

    @objc func makeCurrent(notification: NSNotification) {
        mpInfoCenter.playbackState = .paused
        mpInfoCenter.playbackState = .playing
        updatePlaybackState()
    }

    func updatePlaybackState() {
        mpInfoCenter.playbackState = isPaused ? .paused : .playing
    }

    func cmdHandler(_ event: MPRemoteCommandEvent) -> MPRemoteCommandHandlerStatus {
        guard let config = configs[event.command] else {
            return .commandFailed
        }

        var state = config.state
        if config.type == .repeatable {
            state = MP_KEY_STATE_DOWN
            configs[event.command]?.state = MP_KEY_STATE_DOWN
            if config.state == MP_KEY_STATE_DOWN {
                state = MP_KEY_STATE_UP
                configs[event.command]?.state = MP_KEY_STATE_UP
            }
        }

        EventsResponder.sharedInstance().handleMPKey(config.key, withMask: Int32(state))

        return .success
    }

    @objc func processEvent(_ event: UnsafeMutablePointer<mpv_event>) {
        switch event.pointee.event_id {
        case MPV_EVENT_PROPERTY_CHANGE:
            handlePropertyChange(event)
        default:
            break
        }
    }

    func handlePropertyChange(_ event: UnsafeMutablePointer<mpv_event>) {
        let pData = OpaquePointer(event.pointee.data)
        guard let property = UnsafePointer<mpv_event_property>(pData)?.pointee else {
            return
        }

        switch String(cString: property.name) {
        case "pause" where property.format == MPV_FORMAT_FLAG:
            isPaused = LibmpvHelper.mpvFlagToBool(property.data) ?? false
        default:
            break
        }
    }
}
