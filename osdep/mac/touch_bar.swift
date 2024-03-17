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

extension NSTouchBar.CustomizationIdentifier {
    public static let customId: NSTouchBar.CustomizationIdentifier = "io.mpv.touchbar"
}

extension NSTouchBarItem.Identifier {
    public static let seekBar = NSTouchBarItem.Identifier(custom: ".seekbar")
    public static let play = NSTouchBarItem.Identifier(custom: ".play")
    public static let nextItem = NSTouchBarItem.Identifier(custom: ".nextItem")
    public static let previousItem = NSTouchBarItem.Identifier(custom: ".previousItem")
    public static let nextChapter = NSTouchBarItem.Identifier(custom: ".nextChapter")
    public static let previousChapter = NSTouchBarItem.Identifier(custom: ".previousChapter")
    public static let cycleAudio = NSTouchBarItem.Identifier(custom: ".cycleAudio")
    public static let cycleSubtitle = NSTouchBarItem.Identifier(custom: ".cycleSubtitle")
    public static let currentPosition = NSTouchBarItem.Identifier(custom: ".currentPosition")
    public static let timeLeft = NSTouchBarItem.Identifier(custom: ".timeLeft")

    init(custom: String) {
        self.init(NSTouchBar.CustomizationIdentifier.customId + custom)
    }
}

extension TouchBar {
    enum `Type` {
        case button
        case text
        case slider
    }

    struct Config {
        let name: String
        let type: Type
        let command: String
        var view: NSView?
        var item: NSCustomTouchBarItem?
        var constraint: NSLayoutConstraint?
        let image: NSImage
        let imageAlt: NSImage

        init(
            name: String = "",
            type: Type = .button,
            command: String = "",
            view: NSView? = nil,
            item: NSCustomTouchBarItem? = nil,
            constraint: NSLayoutConstraint? = nil,
            image: NSImage? = nil,
            imageAlt: NSImage? = nil
        ) {
            self.name = name
            self.type = type
            self.command = command
            self.view = view
            self.item = item
            self.constraint = constraint
            self.image = image ?? NSImage(size: NSSize(width: 1, height: 1))
            self.imageAlt = imageAlt ?? NSImage(size: NSSize(width: 1, height: 1))
        }
    }
}

class TouchBar: NSTouchBar, NSTouchBarDelegate {
    var configs: [NSTouchBarItem.Identifier:Config] = [:]
    var isPaused: Bool = false { didSet { updatePlayButton() } }
    var position: Double = 0 { didSet { updateTouchBarTimeItems() } }
    var duration: Double = 0 { didSet { updateTouchBarTimeItems() } }
    var rate: Double = 0

    override init() {
        super.init()

        configs = [
            .seekBar: Config(name: "Seek Bar", type: .slider, command: "seek %f absolute-percent"),
            .currentPosition: Config(name: "Current Position", type: .text),
            .timeLeft: Config(name: "Time Left", type: .text),
            .play: Config(
                name: "Play Button",
                type: .button,
                command: "cycle pause",
                image: .init(named: NSImage.touchBarPauseTemplateName),
                imageAlt: .init(named: NSImage.touchBarPlayTemplateName)
            ),
            .previousItem: Config(
                name: "Previous Playlist Item",
                type: .button,
                command: "playlist-prev",
                image: .init(named: NSImage.touchBarGoBackTemplateName)
            ),
            .nextItem: Config(
                name: "Next Playlist Item",
                type: .button,
                command: "playlist-next",
                image: .init(named: NSImage.touchBarGoForwardTemplateName)
            ),
            .previousChapter: Config(
                name: "Previous Chapter",
                type: .button,
                command: "add chapter -1",
                image: .init(named: NSImage.touchBarSkipBackTemplateName)
            ),
            .nextChapter: Config(
                name: "Next Chapter",
                type: .button,
                command: "add chapter 1",
                image: .init(named: NSImage.touchBarSkipAheadTemplateName)
            ),
            .cycleAudio: Config(
                name: "Cycle Audio",
                type: .button,
                command: "cycle audio",
                image: .init(named: NSImage.touchBarAudioInputTemplateName)
                ),
            .cycleSubtitle: Config(
                name: "Cycle Subtitle",
                type: .button,
                command: "cycle sub",
                image: .init(named: NSImage.touchBarComposeTemplateName)
            )
        ]

        delegate = self
        customizationIdentifier = .customId;
        defaultItemIdentifiers = [.play, .previousItem, .nextItem, .seekBar]
        customizationAllowedItemIdentifiers = [.play, .seekBar, .previousItem, .nextItem,
            .previousChapter, .nextChapter, .cycleAudio, .cycleSubtitle, .currentPosition, .timeLeft]
        addObserver(self, forKeyPath: "visible", options: [.new], context: nil)
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
    }

    func touchBar(_ touchBar: NSTouchBar, makeItemForIdentifier identifier: NSTouchBarItem.Identifier) -> NSTouchBarItem? {
        guard let config = configs[identifier] else { return nil }

        switch config.type {
        case .button:
            let item = NSCustomTouchBarItem(identifier: identifier)
            let image = config.image
            let button = NSButton(image: image, target: self, action: #selector(buttonAction(_:)))
            item.view = button;
            item.customizationLabel = config.name
            configs[identifier]?.view = button
            configs[identifier]?.item = item
            item.addObserver(self, forKeyPath: "visible", options: [.new], context: nil)
            return item
        case .text:
            let item = NSCustomTouchBarItem(identifier: identifier)
            let text = NSTextField(labelWithString: "0:00")
            text.alignment = .center
            item.view = text;
            item.customizationLabel = config.name
            configs[identifier]?.view = text
            configs[identifier]?.item = item
            item.addObserver(self, forKeyPath: "visible", options: [.new], context: nil)
            return item
        case .slider:
            let item = NSCustomTouchBarItem(identifier: identifier)
            let slider = NSSlider(target: self, action: #selector(seekbarChanged(_:)))
            slider.minValue = 0
            slider.maxValue = 100
            item.view = slider;
            item.customizationLabel = config.name
            configs[identifier]?.view = slider
            configs[identifier]?.item = item
            item.addObserver(self, forKeyPath: "visible", options: [.new], context: nil)
            return item
        }
    }

    override func observeValue(
        forKeyPath keyPath: String?,
        of object: Any?,
        change: [NSKeyValueChangeKey:Any]?,
        context: UnsafeMutableRawPointer?
    ) {
        guard let visible = change?[.newKey] as? Bool else { return }
        if keyPath == "isVisible" && visible {
            updateTouchBarTimeItems()
            updatePlayButton()
        }
    }

    func updateTouchBarTimeItems() {
        if !isVisible { return }
        updateSlider()
        updateTimeLeft()
        updateCurrentPosition()
    }

    func updateSlider() {
        guard let config = configs[.seekBar], let slider = config.view as? NSSlider else { return }
        if !(config.item?.isVisible ?? false) { return }

        slider.isEnabled = duration > 0
        if !slider.isHighlighted {
            slider.doubleValue = slider.isEnabled ? (position / duration) * 100 : 0
        }
    }

    func updateTimeLeft() {
        guard let config = configs[.timeLeft], let text = config.view as? NSTextField else { return }
        if !(config.item?.isVisible ?? false) { return }

        removeConstraintFor(identifier: .timeLeft)
        text.stringValue = duration > 0 ? "-" + format(time: Int(floor(duration) - floor(position))) : ""
        if !text.stringValue.isEmpty {
            applyConstraintFrom(string: "-" + format(time: Int(duration)), identifier: .timeLeft)
        }
    }

    func updateCurrentPosition() {
        guard let config = configs[.currentPosition], let text = config.view as? NSTextField else { return }
        if !(config.item?.isVisible ?? false) { return }

        text.stringValue = format(time: Int(floor(position)))
        removeConstraintFor(identifier: .currentPosition)
        applyConstraintFrom(string: format(time: Int(duration > 0 ? duration : position)), identifier: .currentPosition)
    }

    func updatePlayButton() {
        guard let config = configs[.play], let button = config.view as? NSButton else { return }
        if !isVisible || !(config.item?.isVisible ?? false) { return }
        button.image = isPaused ? configs[.play]?.imageAlt : configs[.play]?.image
    }

    @objc func buttonAction(_ button: NSButton) {
        guard let identifier = getIdentifierFrom(view: button), let command = configs[identifier]?.command else { return }
        EventsResponder.sharedInstance().inputHelper.command(command)
    }

    @objc func seekbarChanged(_ slider: NSSlider) {
        guard let identifier = getIdentifierFrom(view: slider), let command = configs[identifier]?.command else { return }
        EventsResponder.sharedInstance().inputHelper.command(String(format: command, slider.doubleValue))
    }

    func format(time: Int) -> String {
        let formatter = DateComponentsFormatter()
        formatter.unitsStyle = .positional
        formatter.zeroFormattingBehavior = time >= (60 * 60) ? [.dropLeading] : []
        formatter.allowedUnits = time >= (60 * 60) ? [.hour, .minute, .second] : [.minute, .second]
        return formatter.string(from: TimeInterval(time)) ?? "0:00"
    }

    func removeConstraintFor(identifier: NSTouchBarItem.Identifier) {
        guard let text = configs[identifier]?.view as? NSTextField,
              let constraint = configs[identifier]?.constraint as? NSLayoutConstraint else { return }
        text.removeConstraint(constraint)
    }

    func applyConstraintFrom(string: String, identifier: NSTouchBarItem.Identifier) {
        guard let text = configs[identifier]?.view as? NSTextField else { return }
        let fullString = string.components(separatedBy: .decimalDigits).joined(separator: "0")
        let textField = NSTextField(labelWithString: fullString)
        let con = NSLayoutConstraint(item: text, attribute: .width, relatedBy: .equal, toItem: nil,
            attribute: .notAnAttribute, multiplier: 1.1, constant: ceil(textField.frame.size.width))
        text.addConstraint(con)
        configs[identifier]?.constraint = con
    }

    func getIdentifierFrom(view: NSView) -> NSTouchBarItem.Identifier? {
        for (identifier, config) in configs {
            if config.view == view {
                return identifier
            }
        }
        return nil
    }

    @objc func processEvent(_ event: UnsafeMutablePointer<mpv_event>) {
        switch event.pointee.event_id {
        case MPV_EVENT_END_FILE:
            position = 0
            duration = 0
        case MPV_EVENT_PROPERTY_CHANGE:
            handlePropertyChange(event)
        default:
            break
        }
    }

    func handlePropertyChange(_ event: UnsafeMutablePointer<mpv_event>) {
        let pData = OpaquePointer(event.pointee.data)
        guard let property = UnsafePointer<mpv_event_property>(pData)?.pointee else { return }

        switch String(cString: property.name) {
        case "time-pos" where property.format == MPV_FORMAT_DOUBLE:
            let newPosition = max(LibmpvHelper.mpvDoubleToDouble(property.data) ?? 0, 0)
            if Int((floor(newPosition) - floor(position)) / rate) != 0 {
                position = newPosition
            }
        case "duration" where property.format == MPV_FORMAT_DOUBLE:
            duration = LibmpvHelper.mpvDoubleToDouble(property.data) ?? 0
        case "pause" where property.format == MPV_FORMAT_FLAG:
            isPaused = LibmpvHelper.mpvFlagToBool(property.data) ?? false
        case "speed" where property.format == MPV_FORMAT_DOUBLE:
            rate = LibmpvHelper.mpvDoubleToDouble(property.data) ?? 1
        default:
            break
        }
    }
}
