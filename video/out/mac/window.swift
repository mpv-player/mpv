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
    weak var common: Common! = nil
    var mpv: MPVHelper? { get { return common.mpv } }

    var targetScreen: NSScreen?
    var previousScreen: NSScreen?
    var currentScreen: NSScreen?
    var unfScreen: NSScreen?

    var unfsContentFrame: NSRect?
    var isInFullscreen: Bool = false
    var isMoving: Bool = false
    var previousStyleMask: NSWindow.StyleMask = [.titled, .closable, .miniaturizable, .resizable]

    var isAnimating: Bool = false
    let animationLock: NSCondition = NSCondition()

    var unfsContentFramePixel: NSRect { get { return convertToBacking(unfsContentFrame ?? NSRect(x: 0, y: 0, width: 160, height: 90)) } }
    var framePixel: NSRect { get { return convertToBacking(frame) } }

    var keepAspect: Bool = true {
        didSet {
            if let contentViewFrame = contentView?.frame, !isInFullscreen {
                unfsContentFrame = convertToScreen(contentViewFrame)
            }

            if keepAspect {
                contentAspectRatio = unfsContentFrame?.size ?? contentAspectRatio
            } else {
                resizeIncrements = NSSize(width: 1.0, height: 1.0)
            }
        }
    }

    var border: Bool = true {
        didSet { if !border { common.titleBar?.hide() } }
    }

    override var canBecomeKey: Bool { return true }
    override var canBecomeMain: Bool { return true }

    override var styleMask: NSWindow.StyleMask {
        get { return super.styleMask }
        set {
            let responder = firstResponder
            let windowTitle = title
            previousStyleMask = super.styleMask
            super.styleMask = newValue
            makeFirstResponder(responder)
            title = windowTitle
        }
    }

    convenience init(contentRect: NSRect, screen: NSScreen?, view: NSView, common com: Common) {
        self.init(contentRect: contentRect,
                  styleMask: [.titled, .closable, .miniaturizable, .resizable],
                  backing: .buffered, defer: false, screen: screen)

        // workaround for an AppKit bug where the NSWindow can't be placed on a
        // none Main screen NSScreen outside the Main screen's frame bounds
        if let wantedScreen = screen, screen != NSScreen.main {
            var absoluteWantedOrigin = contentRect.origin
            absoluteWantedOrigin.x += wantedScreen.frame.origin.x
            absoluteWantedOrigin.y += wantedScreen.frame.origin.y

            if !NSEqualPoints(absoluteWantedOrigin, self.frame.origin) {
                self.setFrameOrigin(absoluteWantedOrigin)
            }
        }

        common = com
        title = com.title
        minSize = NSMakeSize(160, 90)
        collectionBehavior = .fullScreenPrimary
        ignoresMouseEvents = mpv?.opts.cursor_passthrough ?? false
        delegate = self

        if let cView = contentView {
            cView.addSubview(view)
            view.frame = cView.frame
            unfsContentFrame = convertToScreen(cView.frame)
        }

        targetScreen = screen
        currentScreen = screen
        unfScreen = screen

        if let app = NSApp as? Application {
            app.menuBar.register(#selector(setHalfWindowSize), key: .halfSize)
            app.menuBar.register(#selector(setNormalWindowSize), key: .normalSize)
            app.menuBar.register(#selector(setDoubleWindowSize), key: .doubleSize)
            app.menuBar.register(#selector(performMiniaturize(_:)), key: .minimize)
            app.menuBar.register(#selector(performZoom(_:)), key: .zoom)
        }
    }

    override func toggleFullScreen(_ sender: Any?) {
        if isAnimating {
            return
        }

        animationLock.lock()
        isAnimating = true
        animationLock.unlock()

        targetScreen = common.getTargetScreen(forFullscreen: !isInFullscreen)
        if targetScreen == nil && previousScreen == nil {
            targetScreen = screen
        } else if targetScreen == nil {
            targetScreen = previousScreen
            previousScreen = nil
        } else {
            previousScreen = screen
        }

        if let contentViewFrame = contentView?.frame, !isInFullscreen {
            unfsContentFrame = convertToScreen(contentViewFrame)
            unfScreen = screen
        }
        // move window to target screen when going to fullscreen
        if let tScreen = targetScreen, !isInFullscreen && (tScreen != screen) {
            let frame = calculateWindowPosition(for: tScreen, withoutBounds: false)
            setFrame(frame, display: true)
        }

        if Bool(mpv?.opts.native_fs ?? true) {
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
        guard let tScreen = targetScreen else { return }
        common.view?.layerContentsPlacement = .scaleProportionallyToFit
        common.titleBar?.hide()
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = getFsAnimationDuration(duration - 0.05)
            window.animator().setFrame(tScreen.frame, display: true)
        }, completionHandler: nil)
    }

    func window(_ window: NSWindow, startCustomAnimationToExitFullScreenWithDuration duration: TimeInterval) {
        guard let tScreen = targetScreen, let currentScreen = screen else { return }
        let newFrame = calculateWindowPosition(for: tScreen, withoutBounds: tScreen == screen)
        let intermediateFrame = aspectFit(rect: newFrame, in: currentScreen.frame)
        common.titleBar?.hide(0.0)

        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = 0.0
            common.view?.layerContentsPlacement = .scaleProportionallyToFill
            window.animator().setFrame(intermediateFrame, display: true)
        }, completionHandler: {
            NSAnimationContext.runAnimationGroup({ (context) -> Void in
                context.duration = self.getFsAnimationDuration(duration - 0.05)
                self.styleMask.remove(.fullScreen)
                window.animator().setFrame(newFrame, display: true)
            }, completionHandler: nil)
        })
    }

    func windowDidEnterFullScreen(_ notification: Notification) {
        isInFullscreen = true
        mpv?.setOption(fullscreen: isInFullscreen)
        common.updateCursorVisibility()
        endAnimation(frame)
        common.titleBar?.show()
    }

    func windowDidExitFullScreen(_ notification: Notification) {
        guard let tScreen = targetScreen else { return }
        isInFullscreen = false
        mpv?.setOption(fullscreen: isInFullscreen)
        endAnimation(calculateWindowPosition(for: tScreen, withoutBounds: targetScreen == screen))
        common.view?.layerContentsPlacement = .scaleProportionallyToFit
    }

    func windowDidFailToEnterFullScreen(_ window: NSWindow) {
        guard let tScreen = targetScreen else { return }
        let newFrame = calculateWindowPosition(for: tScreen, withoutBounds: targetScreen == screen)
        setFrame(newFrame, display: true)
        endAnimation()
    }

    func windowDidFailToExitFullScreen(_ window: NSWindow) {
        guard let targetFrame = targetScreen?.frame else { return }
        setFrame(targetFrame, display: true)
        endAnimation()
        common.view?.layerContentsPlacement = .scaleProportionallyToFit
    }

    func endAnimation(_ newFrame: NSRect = NSZeroRect) {
        if !NSEqualRects(newFrame, NSZeroRect) && isAnimating {
            NSAnimationContext.runAnimationGroup({ (context) -> Void in
                context.duration = 0.01
                self.animator().setFrame(newFrame, display: true)
            }, completionHandler: nil )
        }

        animationLock.lock()
        isAnimating = false
        animationLock.signal()
        animationLock.unlock()
        common.windowDidEndAnimation()
    }

    func setToFullScreen() {
        guard let targetFrame = targetScreen?.frame else { return }

        if #available(macOS 11.0, *) {
            styleMask = .borderless
            common.titleBar?.hide(0.0)
        } else {
            styleMask.insert(.fullScreen)
        }

        NSApp.presentationOptions = [.autoHideMenuBar, .autoHideDock]
        setFrame(targetFrame, display: true)
        endAnimation()
        isInFullscreen = true
        mpv?.setOption(fullscreen: isInFullscreen)
        common.windowSetToFullScreen()
    }

    func setToWindow() {
        guard let tScreen = targetScreen else { return }

        if #available(macOS 11.0, *) {
            styleMask = previousStyleMask
            common.titleBar?.hide(0.0)
        } else {
            styleMask.remove(.fullScreen)
        }

        let newFrame = calculateWindowPosition(for: tScreen, withoutBounds: targetScreen == screen)
        NSApp.presentationOptions = []
        setFrame(newFrame, display: true)
        endAnimation()
        isInFullscreen = false
        mpv?.setOption(fullscreen: isInFullscreen)
        common.windowSetToWindow()
    }

    func waitForAnimation() {
        animationLock.lock()
        while(isAnimating){
            animationLock.wait()
        }
        animationLock.unlock()
    }

    func getFsAnimationDuration(_ def: Double) -> Double {
        let duration = mpv?.macOpts.macos_fs_animation_duration ?? -1
        if duration < 0 {
            return def
        } else {
            return Double(duration)/1000
        }
    }

    func setOnTop(_ state: Bool, _ ontopLevel: Int) {
        if state {
            switch ontopLevel {
            case -1:
                level = .floating
            case -2:
                level = .statusBar + 1
            case -3:
                level = NSWindow.Level(Int(CGWindowLevelForKey(.desktopWindow)))
            default:
                level = NSWindow.Level(ontopLevel)
            }
            collectionBehavior.remove(.transient)
            collectionBehavior.insert(.managed)
        } else {
            level = .normal
        }
    }

    func setOnAllWorkspaces(_ state: Bool) {
        if state {
            collectionBehavior.insert(.canJoinAllSpaces)
        } else {
            collectionBehavior.remove(.canJoinAllSpaces)
        }
    }

    func setMinimized(_ stateWanted: Bool) {
        if isMiniaturized == stateWanted { return }

        if stateWanted {
            performMiniaturize(self)
        } else {
            deminiaturize(self)
        }
    }

    func setMaximized(_ stateWanted: Bool) {
        if isZoomed == stateWanted { return }

        zoom(self)
    }

    func updateMovableBackground(_ pos: NSPoint) {
        if !isInFullscreen {
            isMovableByWindowBackground = mpv?.canBeDraggedAt(pos) ?? true
        } else {
            isMovableByWindowBackground = false
        }
    }

    func updateFrame(_ rect: NSRect) {
        if rect != frame {
            unfsContentFrame = rect
            if !isInFullscreen {
                let cRect = frameRect(forContentRect: rect)
                setFrame(cRect, display: true)
                common.windowDidUpdateFrame()
            }
        }
    }

    func updateSize(_ size: NSSize) {
        if let currentSize = contentView?.frame.size, size != currentSize {
            let newContentFrame = centeredContentSize(for: frame, size: size)
            updateFrame(newContentFrame)
        }
    }

    override func setFrame(_ frameRect: NSRect, display flag: Bool) {
        if frameRect.width < minSize.width || frameRect.height < minSize.height {
            common.log.sendVerbose("tried to set too small window size: \(frameRect.size)")
            return
        }

        super.setFrame(frameRect, display: flag)

        if let size = unfsContentFrame?.size, keepAspect {
            contentAspectRatio = size
        }
    }

    func centeredContentSize(for rect: NSRect, size sz: NSSize) -> NSRect {
        let cRect = contentRect(forFrameRect: rect)
        let dx = (cRect.size.width  - sz.width)  / 2
        let dy = (cRect.size.height - sz.height) / 2
        return NSInsetRect(cRect, dx, dy)
    }

    func aspectFit(rect r: NSRect, in rTarget: NSRect) -> NSRect {
        var s = rTarget.width / r.width
        if r.height*s > rTarget.height {
            s = rTarget.height / r.height
        }
        let w = r.width * s
        let h = r.height * s
        return NSRect(x: rTarget.midX - w/2, y: rTarget.midY - h/2, width: w, height: h)
    }

    func calculateWindowPosition(for tScreen: NSScreen, withoutBounds: Bool) -> NSRect {
        guard let contentFrame = unfsContentFrame, let screen = unfScreen else {
            return frame
        }
        var newFrame = frameRect(forContentRect: contentFrame)
        let targetFrame = tScreen.frame
        let targetVisibleFrame = tScreen.visibleFrame
        let unfsScreenFrame = screen.frame
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
        if (isAnimating && !isInFullscreen) || (!isAnimating && isInFullscreen ||
            level == NSWindow.Level(Int(CGWindowLevelForKey(.desktopWindow))))
        {
            return frameRect
        }

        guard let ts: NSScreen = tScreen ?? screen ?? NSScreen.main else {
            return frameRect
        }
        var nf: NSRect = frameRect
        let of: NSRect = frame
        let vf: NSRect = (isAnimating ? (targetScreen ?? ts) : ts).visibleFrame
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

    @objc func setNormalWindowSize() { setWindowScale(1.0) }
    @objc func setHalfWindowSize()   { setWindowScale(0.5) }
    @objc func setDoubleWindowSize() { setWindowScale(2.0) }

    func setWindowScale(_ scale: Double) {
        mpv?.command("set window-scale \(scale)")
    }

    func addWindowScale(_ scale: Double) {
        if !isInFullscreen {
            mpv?.command("add window-scale \(scale)")
        }
    }

    func windowDidChangeScreen(_ notification: Notification) {
        if screen == nil {
            return
        }
        if !isAnimating && (currentScreen != screen) {
            previousScreen = screen
        }
        if currentScreen != screen {
            common.updateDisplaylink()
            common.windowDidChangeScreen()
        }
        currentScreen = screen
    }

    func windowDidChangeScreenProfile(_ notification: Notification) {
        common.windowDidChangeScreenProfile()
    }

    func windowDidChangeBackingProperties(_ notification: Notification) {
        common.windowDidChangeBackingProperties()
        common.flagEvents(VO_EVENT_DPI)
    }

    func windowWillStartLiveResize(_ notification: Notification) {
        common.windowWillStartLiveResize()
    }

    func windowDidEndLiveResize(_ notification: Notification) {
        common.windowDidEndLiveResize()
        mpv?.setOption(maximized: isZoomed)

        if let contentViewFrame = contentView?.frame,
               !isAnimating && !isInFullscreen
        {
            unfsContentFrame = convertToScreen(contentViewFrame)
        }
    }

    func windowDidResize(_ notification: Notification) {
        common.windowDidResize()
    }

    func windowShouldClose(_ sender: NSWindow) -> Bool {
        cocoa_put_key(MP_KEY_CLOSE_WIN)
        return false
    }

    func windowDidMiniaturize(_ notification: Notification) {
        mpv?.setOption(minimized: true)
    }

    func windowDidDeminiaturize(_ notification: Notification) {
        mpv?.setOption(minimized: false)
    }

    func windowDidResignKey(_ notification: Notification) {
        common.setCursorVisibility(true)
    }

    func windowDidBecomeKey(_ notification: Notification) {
        common.updateCursorVisibility()
    }

    func windowDidChangeOcclusionState(_ notification: Notification) {
        if occlusionState.contains(.visible) {
            common.windowDidChangeOcclusionState()
            common.updateCursorVisibility()
        }
    }

    func windowWillMove(_ notification: Notification) {
        isMoving = true
    }

    func windowDidMove(_ notification: Notification) {
        mpv?.setOption(maximized: isZoomed)
    }
}
