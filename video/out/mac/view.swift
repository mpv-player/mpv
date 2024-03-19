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

class View: NSView, CALayerDelegate {
    unowned var common: Common
    var option: OptionHelper? { get { return common.option } }
    var input: InputHelper? { get { return common.input } }

    var tracker: NSTrackingArea?
    var hasMouseDown: Bool = false

    override var isFlipped: Bool { return true }
    override var acceptsFirstResponder: Bool { return true }


    init(frame: NSRect, common com: Common) {
        common = com
        super.init(frame: frame)
        autoresizingMask = [.width, .height]
        wantsBestResolutionOpenGLSurface = true
        registerForDraggedTypes([ .fileURL, .URL, .string ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func updateTrackingAreas() {
        if let tracker = self.tracker {
            removeTrackingArea(tracker)
        }

        tracker = NSTrackingArea(rect: bounds,
            options: [.activeAlways, .mouseEnteredAndExited, .mouseMoved, .enabledDuringMouseDrag],
            owner: self, userInfo: nil)
        // here tracker is guaranteed to be none-nil
        addTrackingArea(tracker!)

        if containsMouseLocation() {
            input?.put(key: SWIFT_KEY_MOUSE_LEAVE)
        }
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        guard let types = sender.draggingPasteboard.types else { return [] }
        if types.contains(.fileURL) || types.contains(.URL) || types.contains(.string) {
            return .copy
        }
        return []
    }

    func isURL(_ str: String) -> Bool {
        // force unwrapping is fine here, regex is guaranteed to be valid
        let regex = try! NSRegularExpression(pattern: "^(https?|ftp)://[^\\s/$.?#].[^\\s]*$",
                                             options: .caseInsensitive)
        let isURL = regex.numberOfMatches(in: str,
                                     options: [],
                                       range: NSRange(location: 0, length: str.count))
        return isURL > 0
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        let pb = sender.draggingPasteboard
        guard let types = pb.types else { return false }
        var files: [String] = []

        if types.contains(.fileURL) || types.contains(.URL) {
             guard let urls = pb.readObjects(forClasses: [NSURL.self]) as? [URL] else { return false }
             files = urls.map { $0.absoluteString }
        } else if types.contains(.string) {
            guard let str = pb.string(forType: .string) else { return false }
            files = str.components(separatedBy: "\n").compactMap {
                let url = $0.trimmingCharacters(in: .whitespacesAndNewlines)
                let path = (url as NSString).expandingTildeInPath
                if isURL(url) { return url }
                if path.starts(with: "/") { return path }
                return nil
            }
        }
        if files.isEmpty { return false }
        input?.open(files: files)
        return true
    }

    override func acceptsFirstMouse(for event: NSEvent?) -> Bool {
        return true
    }

    override func becomeFirstResponder() -> Bool {
        return true
    }

    override func resignFirstResponder() -> Bool {
        return true
    }

    override func mouseEntered(with event: NSEvent) {
        if input?.mouseEnabled() ?? true {
            input?.put(key: SWIFT_KEY_MOUSE_ENTER)
        }
        common.updateCursorVisibility()
    }

    override func mouseExited(with event: NSEvent) {
        if input?.mouseEnabled() ?? true {
            input?.put(key: SWIFT_KEY_MOUSE_LEAVE)
        }
        common.titleBar?.hide()
        common.setCursorVisibility(true)
    }

    override func mouseMoved(with event: NSEvent) {
        signalMouseMovement(event)
        common.titleBar?.show()
    }

    override func mouseDragged(with event: NSEvent) {
        signalMouseMovement(event)
    }

    override func mouseDown(with event: NSEvent) {
        hasMouseDown = true
        input?.processMouse(event: event)
    }

    override func mouseUp(with event: NSEvent) {
        hasMouseDown = false
        common.window?.isMoving = false
        input?.processMouse(event: event)
    }

    override func rightMouseDown(with event: NSEvent) {
        hasMouseDown = true
        input?.processMouse(event: event)
    }

    override func rightMouseUp(with event: NSEvent) {
        hasMouseDown = false
        input?.processMouse(event: event)
    }

    override func otherMouseDown(with event: NSEvent) {
        hasMouseDown = true
        input?.processMouse(event: event)
    }

    override func otherMouseUp(with event: NSEvent) {
        hasMouseDown = false
        input?.processMouse(event: event)
    }

    override func magnify(with event: NSEvent) {
        event.phase == .ended ?
            common.windowDidEndLiveResize() : common.windowWillStartLiveResize()

        common.window?.addWindowScale(Double(event.magnification))
    }

    func signalMouseMovement(_ event: NSEvent) {
        var point = convert(event.locationInWindow, from: nil)
        point = convertToBacking(point)
        point.y = -point.y

        common.window?.updateMovableBackground(point)
        if !(common.window?.isMoving ?? false) {
            input?.setMouse(position: point)
        }
    }

    override func scrollWheel(with event: NSEvent) {
        input?.processWheel(event: event)
    }

    func containsMouseLocation() -> Bool {
        var topMargin: CGFloat = 0.0
        let menuBarHeight = NSApp.mainMenu?.menuBarHeight ?? 23.0

        guard let window = common.window else { return false }
        guard var vF = window.screen?.frame else { return false }

        if window.isInFullscreen && (menuBarHeight > 0) {
            topMargin = TitleBar.height + 1 + menuBarHeight
        }

        vF.size.height -= topMargin

        let vFW = window.convertFromScreen(vF)
        let vFV = convert(vFW, from: nil)
        let pt = convert(window.mouseLocationOutsideOfEventStream, from: nil)

        var clippedBounds = bounds.intersection(vFV)
        if !window.isInFullscreen {
            clippedBounds.origin.y += TitleBar.height
            clippedBounds.size.height -= TitleBar.height
        }
        return clippedBounds.contains(pt)
    }

    func canHideCursor() -> Bool {
        guard let window = common.window else { return false }
        return !hasMouseDown && containsMouseLocation() && window.isKeyWindow
    }
}
