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
            "mpKey": MP_KEY_PAUSEONLY,
            "keyType": KeyType.normal
        ],
        MPRemoteCommandCenter.shared().playCommand: [
            "mpKey": MP_KEY_PLAYONLY,
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
            "mpKey": MP_KEY_PLAY,
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

    var mpInfoCenter: MPNowPlayingInfoCenter { get { return MPNowPlayingInfoCenter.default() } }
    var isPaused: Bool = false { didSet { updatePlaybackState() } }

    @objc override init() {
        super.init()

        for cmd in disabledCommands {
            cmd.isEnabled = false
        }
    }

    @objc func start() {
        for (cmd, _) in config {
            cmd.isEnabled = true
            cmd.addTarget { [unowned self] event in
                return self.cmdHandler(event)
            }
        }

        if let app = NSApp as? Application, let icon = app.getMPVIcon(),
               #available(macOS 10.13.2, *)
        {
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
        for (cmd, _) in config {
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

        EventsResponder.sharedInstance().handleMPKey(mpKey, withMask: Int32(state))

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
