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
import VisionKit

class View: NSView, CALayerDelegate, ImageAnalysisOverlayViewDelegate, EventSubscriber {
    unowned var common: Common
    var input: InputHelper? { return common.input }

    var tracker: NSTrackingArea?
    var hasMouseDown: Bool = false
    var lastMouseDownEvent: NSEvent?
    private var analyzer: ImageAnalyzer?
    private var analyzerConfig: ImageAnalyzer.Configuration?
    private var analysisOverlayView: ImageAnalysisOverlayView?
    private var isPaused: Bool = false { didSet { updateAnalysis() } }
    private var imageConverter: ImageConversion?

    override var isFlipped: Bool { return true }
    override var acceptsFirstResponder: Bool { return true }

    override var isOpaque: Bool {
        if let metalLayer = layer as? MetalLayer {
            return !metalLayer.isOpaque
        }

        return true
    }

    init(frame: NSRect, common com: Common) {
        common = com
        super.init(frame: frame)
        autoresizingMask = [.width, .height]
        wantsBestResolutionOpenGLSurface = true
        wantsExtendedDynamicRangeOpenGLSurface = true
        registerForDraggedTypes([ .fileURL, .URL, .string ])
        self.autoresizesSubviews = true

        if #available(macOS 13, *) {
            if ImageAnalyzer.isSupported {
                analyzer = ImageAnalyzer()
                analyzerConfig = ImageAnalyzer.Configuration([.text])
                let overlayView = ImageAnalysisOverlayView()
                overlayView.preferredInteractionTypes = [.automaticTextOnly]
                overlayView.autoresizingMask = [.width, .height]
                overlayView.frame = CGRect(x: 0, y: 0, width: frame.width, height: frame.height)
                overlayView.isSupplementaryInterfaceHidden = true
                overlayView.delegate = self
                analysisOverlayView = overlayView
                addSubview(overlayView)

                imageConverter = ImageConversion(mp_client_get_global(AppHub.shared.mpv)!, mp_client_get_log(AppHub.shared.mpv)!)

                setupAnalysisEvents()
            }
        }
    }

    @available(macOS 13, *)
    func overlayView(_ overlayView: ImageAnalysisOverlayView,
        shouldBeginAt point: CGPoint,
        forAnalysisType analysisType: ImageAnalysisOverlayView.InteractionTypes
    ) -> Bool {
        return true
    }

    @available(macOS 13, *)
    private func setImageAnalysis(_ analysis: ImageAnalysis?) {
        if let analysisView = analysisOverlayView {
            analysisView.resetSelection()
            analysisView.analysis = isPaused ? analysis : .none
        }
    }

    @available(macOS 13, *)
    private func setupAnalysisEvents() {
        if let event = AppHub.shared.event {
            event.subscribe(self, event: .init(name: "pause", format: MPV_FORMAT_FLAG))
            event.subscribe(self, event: .init(name: "video-target-params/w", format: MPV_FORMAT_INT64))
            event.subscribe(self, event: .init(name: "video-target-params/h", format: MPV_FORMAT_INT64))
        }
    }

    func handle(event: EventHelper.Event) {
        switch event.name {
        case "pause": isPaused = event.bool ?? false
        case "video-target-params/w", "video-target-params/h": if #available(macOS 13, *) {
            if let overlayView = analysisOverlayView {
                overlayView.setContentsRectNeedsUpdate()
            }
            updateAnalysis()
        }
        default: break
        }
    }

    private func updateAnalysis() {
        if #available(macOS 13, *) {
            if !isPaused {
                setImageAnalysis(.none)
            } else {
                if let mpv = AppHub.shared.mpv, let a = analyzer, let config = analyzerConfig, let converter = imageConverter {
                    if let image = mp_take_screenshot(mpv, 1) {
                        if let converted = converter.convertImage(image.pointee) {
                            Task {
                                do {
                                    setImageAnalysis(try await a.analyze(converted, orientation: .up, configuration: config))
                                } catch {
                                    setImageAnalysis(.none)
                                }
                            }
                        }
                    }
                }
            }
        }
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
                if url.isUrl() { return url }
                if path.starts(with: "/") { return path }
                return nil
            }
        }
        if files.isEmpty { return false }
        input?.open(files: files)
        return true
    }

    override func acceptsFirstMouse(for event: NSEvent?) -> Bool {
        return false
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
        super.mouseEntered(with: event)
    }

    override func mouseExited(with event: NSEvent) {
        if input?.mouseEnabled() ?? true {
            input?.put(key: SWIFT_KEY_MOUSE_LEAVE)
        }
        common.titleBar?.hide()
        common.setCursorVisibility(true)
        super.mouseExited(with: event)
    }

    override func mouseMoved(with event: NSEvent) {
        signalMouseMovement(event)
        common.titleBar?.show()
        super.mouseMoved(with: event)
    }

    override func mouseDragged(with event: NSEvent) {
        signalMouseMovement(event)
        super.mouseDragged(with: event)
    }

    override func mouseDown(with event: NSEvent) {
        hasMouseDown = event.clickCount <= 1
        input?.processMouse(event: event)
        lastMouseDownEvent = event
        super.mouseDown(with: event)
    }

    override func mouseUp(with event: NSEvent) {
        hasMouseDown = false
        common.window?.isMoving = false
        input?.processMouse(event: event)
        super.mouseUp(with: event)
    }

    override func rightMouseDown(with event: NSEvent) {
        hasMouseDown = event.clickCount <= 1
        input?.processMouse(event: event)
    }

    override func rightMouseUp(with event: NSEvent) {
        hasMouseDown = false
        input?.processMouse(event: event)
    }

    override func otherMouseDown(with event: NSEvent) {
        hasMouseDown = event.clickCount <= 1
        input?.processMouse(event: event)
    }

    override func otherMouseUp(with event: NSEvent) {
        hasMouseDown = false
        input?.processMouse(event: event)
    }

    override func magnify(with event: NSEvent) {
        common.window?.isAnimating = event.phase != .ended
        event.phase == .ended ? common.windowDidEndLiveResize() : common.windowWillStartLiveResize()
        common.window?.addWindowScale(Double(event.magnification))
    }

    func signalMouseMovement(_ event: NSEvent) {
        if #available(macOS 13, *) {
            if let overlayView = analysisOverlayView, hasMouseDown {
                if overlayView.hasInteractiveItem(at: event.locationInWindow) {
                    return
                }
            }
        }

        var point = convert(event.locationInWindow, from: nil)
        point = convertToBacking(point)
        point.y = -point.y

        if !(common.window?.isMoving ?? false) {
            input?.setMouse(position: point)
        }
    }

    override func scrollWheel(with event: NSEvent) {
        input?.processWheel(event: event)
    }

    func contentView(for overlayView: ImageAnalysisOverlayView) -> NSView? {
        return self
    }

    func contentsRect(for overlayView: ImageAnalysisOverlayView) -> CGRect {
        return CGRect(x: 0.0, y: 0.0, width: 1.0, height: 1.0)
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
