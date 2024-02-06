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

extension RemoteCommandCenter {
    typealias ConfigHandler = (MPRemoteCommandEvent) -> (MPRemoteCommandHandlerStatus)

    enum KeyType {
        case normal
        case repeatable
    }

    struct Config {
        let key: Int32
        let type: KeyType
        var state: UInt32 = 0
        let handler: ConfigHandler

        init(key: Int32, type: KeyType = .normal, handler: @escaping ConfigHandler = { event in return .commandFailed }) {
            self.key = key
            self.type = type
            self.handler = handler
        }
    }
}

class RemoteCommandCenter: NSObject {
    var nowPlayingInfo: [String:Any] = [:]
    var configs: [MPRemoteCommand:Config] = [:]
    var disabledCommands: [MPRemoteCommand] = []
    var isPaused: Bool = false { didSet { updatePlaybackState() } }

    var infoCenter: MPNowPlayingInfoCenter { get { return MPNowPlayingInfoCenter.default() } }
    var commandCenter: MPRemoteCommandCenter { get { return MPRemoteCommandCenter.shared() } }

    @objc override init() {
        super.init()

        nowPlayingInfo = [
            MPNowPlayingInfoPropertyMediaType: NSNumber(value: MPNowPlayingInfoMediaType.video.rawValue),
            MPNowPlayingInfoPropertyDefaultPlaybackRate: NSNumber(value: 1),
            MPNowPlayingInfoPropertyPlaybackProgress: NSNumber(value: 0.0),
            MPMediaItemPropertyPlaybackDuration: NSNumber(value: 0),
            MPMediaItemPropertyTitle: "mpv",
            MPMediaItemPropertyAlbumTitle: "mpv",
            MPMediaItemPropertyArtist: "mpv",
        ]

        configs = [
            commandCenter.pauseCommand: Config(key: MP_KEY_PAUSEONLY, handler: keyHandler),
            commandCenter.playCommand: Config(key: MP_KEY_PLAYONLY, handler: keyHandler),
            commandCenter.stopCommand: Config(key: MP_KEY_STOP, handler: keyHandler),
            commandCenter.nextTrackCommand: Config(key: MP_KEY_NEXT, handler: keyHandler),
            commandCenter.previousTrackCommand: Config(key: MP_KEY_PREV, handler: keyHandler),
            commandCenter.togglePlayPauseCommand: Config(key: MP_KEY_PLAY, handler: keyHandler),
            commandCenter.seekForwardCommand: Config(key: MP_KEY_FORWARD, type: .repeatable, handler: keyHandler),
            commandCenter.seekBackwardCommand: Config(key: MP_KEY_REWIND, type: .repeatable, handler: keyHandler)
        ]

        disabledCommands = [
            commandCenter.changePlaybackRateCommand,
            commandCenter.changeRepeatModeCommand,
            commandCenter.changeShuffleModeCommand,
            commandCenter.skipForwardCommand,
            commandCenter.skipBackwardCommand,
            commandCenter.changePlaybackPositionCommand,
            commandCenter.enableLanguageOptionCommand,
            commandCenter.disableLanguageOptionCommand,
            commandCenter.ratingCommand,
            commandCenter.likeCommand,
            commandCenter.dislikeCommand,
            commandCenter.bookmarkCommand,
        ]

        if let app = NSApp as? Application, let icon = app.getMPVIcon() {
            let albumArt = MPMediaItemArtwork(boundsSize: icon.size) { _ in
                return icon
            }
            nowPlayingInfo[MPMediaItemPropertyArtwork] = albumArt
        }

        for cmd in disabledCommands {
            cmd.isEnabled = false
        }
    }

    @objc func start() {
        for (cmd, config) in configs {
            cmd.isEnabled = true
            cmd.addTarget(handler: config.handler)
        }

        infoCenter.nowPlayingInfo = nowPlayingInfo
        infoCenter.playbackState = .playing

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

        infoCenter.nowPlayingInfo = nil
        infoCenter.playbackState = .unknown
    }

    @objc func makeCurrent(notification: NSNotification) {
        infoCenter.playbackState = .paused
        infoCenter.playbackState = .playing
        updatePlaybackState()
    }

    func updatePlaybackState() {
        infoCenter.playbackState = isPaused ? .paused : .playing
    }

    lazy var keyHandler: ConfigHandler = { event in
        guard let config = self.configs[event.command] else {
            return .commandFailed
        }

        var state = config.state
        if config.type == .repeatable {
            state = config.state == MP_KEY_STATE_DOWN ? MP_KEY_STATE_UP : MP_KEY_STATE_DOWN
            self.configs[event.command]?.state = state
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
