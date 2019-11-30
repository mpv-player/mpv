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

@available(macOS 10.12.2, *)
class RemoteCommandCenter: NSObject {

    enum KeyType {
        case normal
        case repeatable
    }

    var config: [MPRemoteCommand:[String:Any]] = [
        MPRemoteCommandCenter.shared().pauseCommand: [
            "mpKey": MP_KEY_PAUSE,
            "keyType": KeyType.normal
        ],
        MPRemoteCommandCenter.shared().playCommand: [
            "mpKey": MP_KEY_PLAY,
            "keyType": KeyType.normal
        ],
        MPRemoteCommandCenter.shared().stopCommand: [
            "mpKey": MP_KEY_STOP,
            "keyType": KeyType.normal
        ],
        MPRemoteCommandCenter.shared().nextTrackCommand: [
            "mpKey": MP_KEY_NEXT,
            "keyType": KeyType.normal
        ],
        MPRemoteCommandCenter.shared().previousTrackCommand: [
            "mpKey": MP_KEY_PREV,
            "keyType": KeyType.normal
        ],
        MPRemoteCommandCenter.shared().togglePlayPauseCommand: [
            "mpKey": MP_KEY_PLAYPAUSE,
            "keyType": KeyType.normal
        ],
        MPRemoteCommandCenter.shared().seekForwardCommand: [
            "mpKey": MP_KEY_FORWARD,
            "keyType": KeyType.repeatable,
            "state": MP_KEY_STATE_UP
        ],
        MPRemoteCommandCenter.shared().seekBackwardCommand: [
            "mpKey": MP_KEY_REWIND,
            "keyType": KeyType.repeatable,
            "state": MP_KEY_STATE_UP
        ],
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

    let application: Application;

    @objc init(app: Application) {
        application = app

        super.init()

        for cmd in disabledCommands {
            cmd.isEnabled = false
        }
    }

    @objc func makeCurrent() {
        MPNowPlayingInfoCenter.default().playbackState = .paused
        MPNowPlayingInfoCenter.default().playbackState = .playing
    }

    @objc func start() {
        for (cmd, _) in config {
            cmd.isEnabled = true
            cmd.addTarget { [unowned self] event in
                return self.cmdHandler(event)
            }
        }

        if let icon = application.getMPVIcon(), #available(macOS 10.13.2, *) {
            let albumArt = MPMediaItemArtwork(boundsSize:icon.size) { _ in
                return icon
            }
            nowPlayingInfo[MPMediaItemPropertyArtwork] = albumArt
        }

        MPNowPlayingInfoCenter.default().nowPlayingInfo = nowPlayingInfo
        MPNowPlayingInfoCenter.default().playbackState = .playing
    }

    @objc func stop() {
        for (cmd, _) in config {
            cmd.isEnabled = false
            cmd.removeTarget(nil)
        }

        MPNowPlayingInfoCenter.default().nowPlayingInfo = nil
        MPNowPlayingInfoCenter.default().playbackState = .unknown
    }

    func cmdHandler(_ event: MPRemoteCommandEvent) -> MPRemoteCommandHandlerStatus {
        guard let cmdConfig = config[event.command],
              let mpKey = cmdConfig["mpKey"] as? Int32,
              let keyType = cmdConfig["keyType"] as? KeyType else
        {
            return .commandFailed
        }

        var state = cmdConfig["state"] as? UInt32 ?? 0

        if let currentState = cmdConfig["state"] as? UInt32, keyType == .repeatable {
            state = MP_KEY_STATE_DOWN
            config[event.command]?["state"] = MP_KEY_STATE_DOWN
            if currentState == MP_KEY_STATE_DOWN {
                state = MP_KEY_STATE_UP
                config[event.command]?["state"] = MP_KEY_STATE_UP
            }
        }

        application.handleMPKey(mpKey, withMask: Int32(state));

        return .success
    }
}