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

        init(key: Int32 = 0, type: KeyType = .normal, handler: @escaping ConfigHandler = { event in return .commandFailed }) {
            self.key = key
            self.type = type
            self.handler = handler
        }
    }
}

class RemoteCommandCenter: EventSubscriber {
    unowned let appHub: AppHub
    var event: EventHelper? { get { return appHub.event } }
    var configs: [MPRemoteCommand:Config] = [:]
    var disabledCommands: [MPRemoteCommand] = []
    var isPaused: Bool = false { didSet { updateInfoCenter() } }
    var duration: Double = 0 { didSet { updateInfoCenter() } }
    var position: Double = 0 { didSet { updateInfoCenter() } }
    var rate: Double = 1 { didSet { updateInfoCenter() } }
    var title: String = "" { didSet { updateInfoCenter() } }
    var chapter: String? { didSet { updateInfoCenter() } }
    var album: String? { didSet { updateInfoCenter() } }
    var artist: String? { didSet { updateInfoCenter() } }
    var cover: NSImage = NSImage(size: NSSize(width: 256, height: 256))

    var infoCenter: MPNowPlayingInfoCenter { get { return MPNowPlayingInfoCenter.default() } }
    var commandCenter: MPRemoteCommandCenter { get { return MPRemoteCommandCenter.shared() } }

    init(_ appHub: AppHub) {
        self.appHub = appHub

        configs = [
            commandCenter.pauseCommand: Config(key: MP_KEY_PAUSEONLY, handler: keyHandler),
            commandCenter.playCommand: Config(key: MP_KEY_PLAYONLY, handler: keyHandler),
            commandCenter.stopCommand: Config(key: MP_KEY_STOP, handler: keyHandler),
            commandCenter.nextTrackCommand: Config(key: MP_KEY_NEXT, handler: keyHandler),
            commandCenter.previousTrackCommand: Config(key: MP_KEY_PREV, handler: keyHandler),
            commandCenter.togglePlayPauseCommand: Config(key: MP_KEY_PLAY, handler: keyHandler),
            commandCenter.seekForwardCommand: Config(key: MP_KEY_FORWARD, type: .repeatable, handler: keyHandler),
            commandCenter.seekBackwardCommand: Config(key: MP_KEY_REWIND, type: .repeatable, handler: keyHandler),
            commandCenter.changePlaybackPositionCommand: Config(handler: seekHandler),
        ]

        disabledCommands = [
            commandCenter.changePlaybackRateCommand,
            commandCenter.changeRepeatModeCommand,
            commandCenter.changeShuffleModeCommand,
            commandCenter.skipForwardCommand,
            commandCenter.skipBackwardCommand,
            commandCenter.enableLanguageOptionCommand,
            commandCenter.disableLanguageOptionCommand,
            commandCenter.ratingCommand,
            commandCenter.likeCommand,
            commandCenter.dislikeCommand,
            commandCenter.bookmarkCommand,
        ]

        cover = (NSApp as? Application)?.getMPVIcon() ?? cover

        for cmd in disabledCommands {
            cmd.isEnabled = false
        }
    }

    func registerEvents() {
        event?.subscribe(self, event: .init(name: "duration", format: MPV_FORMAT_DOUBLE))
        event?.subscribe(self, event: .init(name: "time-pos", format: MPV_FORMAT_DOUBLE))
        event?.subscribe(self, event: .init(name: "speed", format: MPV_FORMAT_DOUBLE))
        event?.subscribe(self, event: .init(name: "pause", format: MPV_FORMAT_FLAG))
        event?.subscribe(self, event: .init(name: "media-title", format: MPV_FORMAT_STRING))
        event?.subscribe(self, event: .init(name: "chapter-metadata/title", format: MPV_FORMAT_STRING))
        event?.subscribe(self, event: .init(name: "metadata/by-key/album", format: MPV_FORMAT_STRING))
        event?.subscribe(self, event: .init(name: "metadata/by-key/artist", format: MPV_FORMAT_STRING))
    }

    func start() {
        for (cmd, config) in configs {
            cmd.isEnabled = true
            cmd.addTarget(handler: config.handler)
        }

        updateInfoCenter()

        NotificationCenter.default.addObserver(
            self,
            selector: #selector(self.makeCurrent),
            name: NSApplication.willBecomeActiveNotification,
            object: nil
        )
    }

    func stop() {
        for (cmd, _) in configs {
            cmd.isEnabled = false
            cmd.removeTarget(nil)
        }

        infoCenter.nowPlayingInfo = nil
        infoCenter.playbackState = .unknown

        NotificationCenter.default.removeObserver(
            self,
            name: NSApplication.willBecomeActiveNotification,
            object: nil
        )
    }

    @objc func makeCurrent(notification: NSNotification) {
        infoCenter.playbackState = .paused
        infoCenter.playbackState = .playing
        updateInfoCenter()
    }

    func updateInfoCenter() {
        infoCenter.playbackState = isPaused ? .paused : .playing
        infoCenter.nowPlayingInfo = (infoCenter.nowPlayingInfo ?? [:]).merging([
            MPNowPlayingInfoPropertyMediaType: NSNumber(value: MPNowPlayingInfoMediaType.video.rawValue),
            MPNowPlayingInfoPropertyPlaybackProgress: NSNumber(value: 0.0),
            MPNowPlayingInfoPropertyPlaybackRate: NSNumber(value: isPaused ? 0 : rate),
            MPNowPlayingInfoPropertyElapsedPlaybackTime: NSNumber(value: position),
            MPMediaItemPropertyPlaybackDuration: NSNumber(value: duration),
            MPMediaItemPropertyTitle: title,
            MPMediaItemPropertyArtist: artist ?? chapter ?? "",
            MPMediaItemPropertyAlbumTitle: album ?? "",
            MPMediaItemPropertyArtwork: MPMediaItemArtwork(boundsSize: cover.size) { _ in return self.cover }
        ]) { (_, new) in new }
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

        AppHub.shared.input.put(key: config.key | Int32(state))

        return .success
    }

    lazy var seekHandler: ConfigHandler = { event in
        guard let posEvent = event as? MPChangePlaybackPositionCommandEvent else {
            return .commandFailed
        }

        let cmd = String(format: "seek %.02f absolute", posEvent.positionTime)
        return AppHub.shared.input.command(cmd) ? .success : .commandFailed
    }

    func handle(event: EventHelper.Event) {
        switch event.name {
        case "time-pos":
            let newPosition = max(event.double ?? 0, 0)
            if Int((floor(newPosition) - floor(position)) / rate) != 0 {
                position = newPosition
            }
        case "pause": isPaused = event.bool ?? false
        case "duration": duration = event.double ?? 0
        case "speed": rate = event.double ?? 1
        case "media-title": title = event.string ?? ""
        case "chapter-metadata/title": chapter = event.string
        case "metadata/by-key/album": album = event.string
        case "metadata/by-key/artist": artist = event.string
        default: break
        }
    }
}
