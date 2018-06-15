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

class Window: NSWindow, NSWindowDelegate {

    weak var cocoaCB: CocoaCB! = nil
    var mpv: MPVHelper! {
        get { return cocoaCB == nil ? nil : cocoaCB.mpv }
    }

    var targetScreen: NSScreen?
    var previousScreen: NSScreen?
    var currentScreen: NSScreen?
    var unfScreen: NSScreen?

    var unfsContentFrame: NSRect?
    var isInFullscreen: Bool = false
    var isAnimating: Bool = false
    var isMoving: Bool = false
    var forceTargetScreen: Bool = false

    var keepAspect: Bool = true {
        didSet {
            if !isInFullscreen {
                unfsContentFrame = convertToScreen(contentView!.frame)
            }

            if keepAspect {
                contentAspectRatio = unfsContentFrame!.size
            } else {
                resizeIncrements = NSSize(width: 1.0, height: 1.0)
            }
        }
    }

    var border: Bool = true {
        didSet { if !border { hideTitleBar() } }
    }

    var titleBarEffect: NSVisualEffectView?
    var titleBar: NSView {
        get { return (standardWindowButton(.closeButton)?.superview)! }
    }
    var titleBarHeight: CGFloat {
        get { return NSWindow.frameRect(forContentRect: CGRect.zero, styleMask: .titled).size.height }
    }
    var titleButtons: [NSButton] {
        get { return ([.closeButton, .miniaturizeButton, .zoomButton] as [NSWindowButton]).flatMap { standardWindowButton($0) } }
    }

    override var canBecomeKey: Bool { return true }
    override var canBecomeMain: Bool { return true }

    override var styleMask: NSWindowStyleMask {
        get { return super.styleMask }
        set {
            let responder = firstResponder
            let windowTitle = title
            super.styleMask = newValue
            makeFirstResponder(responder)
            title = windowTitle
        }
    }

    convenience init(contentRect: NSRect, screen: NSScreen?, view: NSView, cocoaCB ccb: CocoaCB) {
        self.init(contentRect: contentRect,
                  styleMask: [.titled, .closable, .miniaturizable, .resizable],
                  backing: .buffered, defer: false, screen: screen)
        cocoaCB = ccb
        title = cocoaCB.title
        minSize = NSMakeSize(160, 90)
        collectionBehavior = .fullScreenPrimary
        delegate = self
        contentView!.addSubview(view)
        view.frame = contentView!.frame

        unfsContentFrame = convertToScreen(contentView!.frame)
        targetScreen = screen!
        currentScreen = screen!
        unfScreen = screen!
        initTitleBar()

        if let app = NSApp as? Application {
            app.menuBar.register(#selector(setHalfWindowSize), for: MPM_H_SIZE)
            app.menuBar.register(#selector(setNormalWindowSize), for: MPM_N_SIZE)
            app.menuBar.register(#selector(setDoubleWindowSize), for: MPM_D_SIZE)
            app.menuBar.register(#selector(performMiniaturize(_:)), for: MPM_MINIMIZE)
            app.menuBar.register(#selector(performZoom(_:)), for: MPM_ZOOM)
        }
    }

    func initTitleBar() {
        var f = contentView!.bounds
        f.origin.y = f.size.height - titleBarHeight
        f.size.height = titleBarHeight

        styleMask.insert(.fullSizeContentView)
        titleBar.alphaValue = 0
        titlebarAppearsTransparent = true
        titleBarEffect = NSVisualEffectView(frame: f)
        titleBarEffect!.alphaValue = 0
        titleBarEffect!.blendingMode = .withinWindow
        titleBarEffect!.autoresizingMask = [.viewWidthSizable, .viewMinYMargin]

        setTitleBarStyle(Int(mpv.macOpts!.macos_title_bar_style))
        contentView!.addSubview(titleBarEffect!, positioned: .above, relativeTo: nil)
    }

    func setTitleBarStyle(_ style: Any) {
        var effect: String

        if style is Int {
            switch style as! Int {
            case 4:
                effect = "auto"
            case 3:
                effect = "mediumlight"
            case 2:
                effect = "light"
            case 1:
                effect = "ultradark"
            case 0: fallthrough
            default:
                effect = "dark"
            }
        } else {
            effect = style as! String
        }

        if effect == "auto" {
            let systemStyle = UserDefaults.standard.string(forKey: "AppleInterfaceStyle")
            effect = systemStyle == nil ? "mediumlight" : "ultradark"
        }

        switch effect {
        case "mediumlight":
            appearance = NSAppearance(named: NSAppearanceNameVibrantLight)
            titleBarEffect!.material = .titlebar
            titleBarEffect!.state = .followsWindowActiveState
        case "light":
            appearance = NSAppearance(named: NSAppearanceNameVibrantLight)
            titleBarEffect!.material = .light
            titleBarEffect!.state = .active
        case "ultradark":
            appearance = NSAppearance(named: NSAppearanceNameVibrantDark)
            titleBarEffect!.material = .titlebar
            titleBarEffect!.state = .followsWindowActiveState
        case "dark": fallthrough
        default:
            appearance = NSAppearance(named: NSAppearanceNameVibrantDark)
            titleBarEffect!.material = .dark
            titleBarEffect!.state = .active
        }
    }

    func showTitleBar() {
        if titleBarEffect == nil || (!border && !isInFullscreen) { return }
        let loc = cocoaCB.view.convert(mouseLocationOutsideOfEventStream, from: nil)

        titleButtons.forEach { $0.isHidden = false }
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = 0.20
            titleBar.animator().alphaValue = 1
            if !isInFullscreen && !isAnimating {
                titleBarEffect!.animator().alphaValue = 1
            }
        }, completionHandler: nil )

        if loc.y > titleBarHeight {
            hideTitleBarDelayed()
        } else {
            NSObject.cancelPreviousPerformRequests(withTarget: self, selector: #selector(hideTitleBar), object: nil)
        }
    }

    func hideTitleBar() {
        if titleBarEffect == nil { return }
        if isInFullscreen && !isAnimating {
            titleBarEffect!.alphaValue = 0
            return
        }
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = 0.20
            titleBar.animator().alphaValue = 0
            titleBarEffect!.animator().alphaValue = 0
        }, completionHandler: {
            self.titleButtons.forEach { $0.isHidden = true }
        })
    }

    func hideTitleBarDelayed() {
        NSObject.cancelPreviousPerformRequests(withTarget: self,
                                                 selector: #selector(hideTitleBar),
                                                   object: nil)
        perform(#selector(hideTitleBar), with: nil, afterDelay: 0.5)
    }

    override func toggleFullScreen(_ sender: Any?) {
        if isAnimating {
            return
        }

        isAnimating = true

        targetScreen = cocoaCB.getTargetScreen(forFullscreen: !isInFullscreen)
        if targetScreen == nil && previousScreen == nil {
            targetScreen = screen
        } else if targetScreen == nil {
            targetScreen = previousScreen
            previousScreen = nil
        } else {
            previousScreen = screen
        }

        if !isInFullscreen {
            unfsContentFrame = convertToScreen(contentView!.frame)
            unfScreen = screen
        }
        // move window to target screen when going to fullscreen
        if !isInFullscreen && (targetScreen != screen) {
            let frame = calculateWindowPosition(for: targetScreen!, withoutBounds: false)
            setFrame(frame, display: true)
        }

        if mpv.getBoolProperty("native-fs") {
            super.toggleFullScreen(sender)
        } else {
            if !isInFullscreen {
                setToFullScreen()
            }
            else {
                setToWindow()
            }
        }
    }

    func customWindowsToEnterFullScreen(for window: NSWindow) -> [NSWindow]? {
        return [window]
    }

    func customWindowsToExitFullScreen(for window: NSWindow) -> [NSWindow]? {
        return [window]
    }

    func window(_ window: NSWindow, startCustomAnimationToEnterFullScreenWithDuration duration: TimeInterval) {
        cocoaCB.view.layerContentsPlacement = .scaleProportionallyToFit
        hideTitleBar()
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = getFsAnimationDuration(duration - 0.05)
            window.animator().setFrame(targetScreen!.frame, display: true)
        }, completionHandler: { })
    }

    func window(_ window: NSWindow, startCustomAnimationToExitFullScreenWithDuration duration: TimeInterval) {
        let newFrame = calculateWindowPosition(for: targetScreen!, withoutBounds: targetScreen == screen)
        let intermediateFrame = aspectFit(rect: newFrame, in: screen!.frame)
        cocoaCB.view.layerContentsPlacement = .scaleProportionallyToFill
        hideTitleBar()
        setFrame(intermediateFrame, display: true)

        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = getFsAnimationDuration(duration - 0.05)
            window.animator().setFrame(newFrame, display: true)
        }, completionHandler: { })
    }

    func windowDidEnterFullScreen(_ notification: Notification) {
        isInFullscreen = true
        cocoaCB.flagEvents(VO_EVENT_FULLSCREEN_STATE)
        cocoaCB.updateCusorVisibility()
        endAnimation(frame)
        showTitleBar()
    }

    func windowDidExitFullScreen(_ notification: Notification) {
        isInFullscreen = false
        cocoaCB.flagEvents(VO_EVENT_FULLSCREEN_STATE)
        endAnimation(calculateWindowPosition(for: targetScreen!, withoutBounds: targetScreen == screen))
        cocoaCB.view.layerContentsPlacement = .scaleProportionallyToFit
    }

    func windowDidFailToEnterFullScreen(_ window: NSWindow) {
        let newFrame = calculateWindowPosition(for: targetScreen!, withoutBounds: targetScreen == screen)
        setFrame(newFrame, display: true)
        endAnimation()
    }

    func windowDidFailToExitFullScreen(_ window: NSWindow) {
        let newFrame = targetScreen!.frame
        setFrame(newFrame, display: true)
        endAnimation()
        cocoaCB.view.layerContentsPlacement = .scaleProportionallyToFit
    }

    func endAnimation(_ newFrame: NSRect = NSZeroRect) {
        if !NSEqualRects(newFrame, NSZeroRect) && isAnimating {
            NSAnimationContext.runAnimationGroup({ (context) -> Void in
                context.duration = 0.01
                self.animator().setFrame(newFrame, display: true)
            }, completionHandler: nil )
        }

        isAnimating = false
        cocoaCB.layer.update()
        cocoaCB.checkShutdown()
    }

    func setToFullScreen() {
        styleMask.insert(.fullScreen)
        NSApp.presentationOptions = [.autoHideMenuBar, .autoHideDock]
        setFrame(targetScreen!.frame, display: true)
        endAnimation()
        isInFullscreen = true
        cocoaCB.flagEvents(VO_EVENT_FULLSCREEN_STATE)
        cocoaCB.layer.update()
    }

    func setToWindow() {
        let newFrame = calculateWindowPosition(for: targetScreen!, withoutBounds: targetScreen == screen)
        NSApp.presentationOptions = []
        setFrame(newFrame, display: true)
        styleMask.remove(.fullScreen)
        endAnimation()
        isInFullscreen = false
        cocoaCB.flagEvents(VO_EVENT_FULLSCREEN_STATE)
        cocoaCB.layer.update()
    }

    func getFsAnimationDuration(_ def: Double) -> Double{
        let duration = mpv.getStringProperty("macos-fs-animation-duration") ?? "default"
        if duration == "default" {
            return def
        } else {
            return Double(duration)!/1000
        }
    }

    func setOnTop(_ state: Bool, _ ontopLevel: Any) {
        if state {
            if ontopLevel is Int {
                switch ontopLevel as! Int {
                case -1:
                    level = Int(CGWindowLevelForKey(.floatingWindow))
                case -2:
                    level = Int(CGWindowLevelForKey(.statusWindow))+1
                default:
                    level = ontopLevel as! Int
                }
            } else {
                switch ontopLevel as! String {
                case "window":
                    level = Int(CGWindowLevelForKey(.floatingWindow))
                case "system":
                    level = Int(CGWindowLevelForKey(.statusWindow))+1
                default:
                    level = Int(ontopLevel as! String)!
                }
            }
            collectionBehavior.remove(.transient)
            collectionBehavior.insert(.managed)
        } else {
            level = Int(CGWindowLevelForKey(.normalWindow))
        }
    }

    func updateMovableBackground(_ pos: NSPoint) {
        if !isInFullscreen {
            isMovableByWindowBackground = mpv.canBeDraggedAt(pos)
        } else {
            isMovableByWindowBackground = false
        }
    }

    func updateFrame(_ rect: NSRect) {
        if rect != frame {
            let cRect = frameRect(forContentRect: rect)
            unfsContentFrame = rect
            setFrame(cRect, display: true)
        }
    }

    func updateSize(_ size: NSSize) {
        if size != contentView!.frame.size {
            let newContentFrame = centeredContentSize(for: frame, size: size)
            if !isInFullscreen {
                updateFrame(newContentFrame)
            } else {
                unfsContentFrame = newContentFrame
            }
        }
    }

    override func setFrame(_ frameRect: NSRect, display flag: Bool) {
        let newFrame = !isAnimating && isInFullscreen ? targetScreen!.frame :
                                                        frameRect
        super.setFrame(newFrame, display: flag)

        if keepAspect {
            contentAspectRatio = unfsContentFrame!.size
        }
    }

    func centeredContentSize(for rect: NSRect, size sz: NSSize) -> NSRect {
        let cRect = contentRect(forFrameRect: rect)
        let dx = (cRect.size.width  - sz.width)  / 2
        let dy = (cRect.size.height - sz.height) / 2
        return NSInsetRect(cRect, dx, dy)
    }

    func aspectFit(rect r: NSRect, in rTarget: NSRect) -> NSRect {
        var s = rTarget.width / r.width;
        if r.height*s > rTarget.height {
            s = rTarget.height / r.height
        }
        let w = r.width * s
        let h = r.height * s
        return NSRect(x: rTarget.midX - w/2, y: rTarget.midY - h/2, width: w, height: h)
    }

    func calculateWindowPosition(for tScreen: NSScreen, withoutBounds: Bool) -> NSRect {
        var newFrame = frameRect(forContentRect: unfsContentFrame!)
        let targetFrame = tScreen.frame
        let targetVisibleFrame = tScreen.visibleFrame
        let unfsScreenFrame = unfScreen!.frame
        let visibleWindow = NSIntersectionRect(unfsScreenFrame, newFrame)

        // calculate visible area of every side
        let left = newFrame.origin.x - unfsScreenFrame.origin.x
        let right = unfsScreenFrame.size.width -
            (newFrame.origin.x - unfsScreenFrame.origin.x + newFrame.size.width)
        let bottom = newFrame.origin.y - unfsScreenFrame.origin.y
        let top = unfsScreenFrame.size.height -
            (newFrame.origin.y - unfsScreenFrame.origin.y + newFrame.size.height)

        // normalize visible areas, decide which one to take horizontal/vertical
        var xPer = (unfsScreenFrame.size.width - visibleWindow.size.width)
        var yPer = (unfsScreenFrame.size.height - visibleWindow.size.height)
        if xPer != 0 { xPer = (left >= 0 || right < 0 ? left : right) / xPer }
        if yPer != 0 { yPer = (bottom >= 0 || top < 0 ? bottom : top) / yPer }

        // calculate visible area for every side for target screen
        let xNewLeft = targetFrame.origin.x +
            (targetFrame.size.width - visibleWindow.size.width) * xPer
        let xNewRight = targetFrame.origin.x + targetFrame.size.width -
            (targetFrame.size.width - visibleWindow.size.width) * xPer - newFrame.size.width
        let yNewBottom = targetFrame.origin.y +
            (targetFrame.size.height - visibleWindow.size.height) * yPer
        let yNewTop = targetFrame.origin.y + targetFrame.size.height -
            (targetFrame.size.height - visibleWindow.size.height) * yPer - newFrame.size.height

        // calculate new coordinates, decide which one to take horizontal/vertical
        newFrame.origin.x = left >= 0 || right < 0 ? xNewLeft : xNewRight
        newFrame.origin.y = bottom >= 0 || top < 0 ? yNewBottom : yNewTop

        // don't place new window on top of a visible menubar
        let topMar = targetFrame.size.height -
            (newFrame.origin.y - targetFrame.origin.y + newFrame.size.height)
        let menuBarHeight = targetFrame.size.height -
            (targetVisibleFrame.size.height + targetVisibleFrame.origin.y)
        if topMar < menuBarHeight {
            newFrame.origin.y -= top - menuBarHeight
        }

        if withoutBounds {
            return newFrame
        }

        // screen bounds right and left
        if newFrame.origin.x + newFrame.size.width > targetFrame.origin.x + targetFrame.size.width {
            newFrame.origin.x = targetFrame.origin.x + targetFrame.size.width - newFrame.size.width
        }
        if newFrame.origin.x < targetFrame.origin.x {
            newFrame.origin.x = targetFrame.origin.x
        }

        // screen bounds top and bottom
        if newFrame.origin.y + newFrame.size.height > targetFrame.origin.y + targetFrame.size.height {
            newFrame.origin.y = targetFrame.origin.y + targetFrame.size.height - newFrame.size.height
        }
        if newFrame.origin.y < targetFrame.origin.y {
            newFrame.origin.y = targetFrame.origin.y
        }
        return newFrame
    }

    override func constrainFrameRect(_ frameRect: NSRect, to tScreen: NSScreen?) -> NSRect {
        if (isAnimating && !isInFullscreen) || (!isAnimating && isInFullscreen) {
            return frameRect
        }

        var nf: NSRect = frameRect
        let ts: NSScreen = tScreen ?? screen ?? NSScreen.main()!
        let of: NSRect = frame
        let vf: NSRect = (isAnimating ? targetScreen! : ts).visibleFrame
        let ncf: NSRect = contentRect(forFrameRect: nf)

        // screen bounds top and bottom
        if NSMaxY(nf) > NSMaxY(vf) {
            nf.origin.y = NSMaxY(vf) - NSHeight(nf)
        }
        if NSMaxY(ncf) < NSMinY(vf) {
            nf.origin.y = NSMinY(vf) + NSMinY(ncf) - NSMaxY(ncf)
        }

        // screen bounds right and left
        if NSMinX(nf) > NSMaxX(vf) {
            nf.origin.x = NSMaxX(vf) - NSWidth(nf)
        }
        if NSMaxX(nf) < NSMinX(vf) {
            nf.origin.x = NSMinX(vf)
        }

        if NSHeight(nf) < NSHeight(vf) && NSHeight(of) > NSHeight(vf) && !isInFullscreen {
            // If the window height is smaller than the visible frame, but it was
            // bigger previously recenter the smaller window vertically. This is
            // needed to counter the 'snap to top' behaviour.
            nf.origin.y = (NSHeight(vf) - NSHeight(nf)) / 2
        }
        return nf
    }

    func setNormalWindowSize() { setWindowScale(1.0) }
    func setHalfWindowSize()   { setWindowScale(0.5) }
    func setDoubleWindowSize() { setWindowScale(2.0) }

    func setWindowScale(_ scale: Double) {
        mpv.commandAsync(["osd-auto", "set", "window-scale", "\(scale)"])
    }

    func windowDidChangeScreen(_ notification: Notification) {
        if screen == nil {
            return
        }
        if !isAnimating && (currentScreen != screen) {
            previousScreen = screen
        }
        if currentScreen != screen {
            cocoaCB.updateDisplaylink()
        }
        currentScreen = screen
    }

    func windowDidChangeScreenProfile(_ notification: Notification) {
        cocoaCB.layer.needsICCUpdate = true
    }

    func windowDidChangeBackingProperties(_ notification: Notification) {
        cocoaCB.layer.contentsScale = backingScaleFactor
    }

    func windowWillStartLiveResize(_ notification: Notification) {
        cocoaCB.layer.inLiveResize = true
    }

    func windowDidEndLiveResize(_ notification: Notification) {
        cocoaCB.layer.inLiveResize = false
    }

    func windowShouldClose(_ sender: Any) -> Bool {
        cocoa_put_key(SWIFT_KEY_CLOSE_WIN)
        return false
    }

    func windowDidResignKey(_ notification: Notification) {
        cocoaCB.setCursorVisiblility(true)
    }

    func windowDidBecomeKey(_ notification: Notification) {
        cocoaCB.updateCusorVisibility()
    }

    func windowWillMove(_ notification: Notification) {
        isMoving = true
    }
}
