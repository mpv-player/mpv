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

class EventsView: NSView {

    weak var cocoaCB: CocoaCB!
    var mpv: MPVHelper! {
        get { return cocoaCB == nil ? nil : cocoaCB.mpv }
    }

    var tracker: NSTrackingArea?
    var hasMouseDown: Bool = false

    override var isFlipped: Bool { return true }
    override var acceptsFirstResponder: Bool { return true }


    init(cocoaCB ccb: CocoaCB) {
        cocoaCB = ccb
        super.init(frame: NSMakeRect(0, 0, 960, 480))
        autoresizingMask = [.viewWidthSizable, .viewHeightSizable]
        wantsBestResolutionOpenGLSurface = true
        register(forDraggedTypes: [NSFilenamesPboardType, NSURLPboardType])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func updateTrackingAreas() {
        if tracker != nil {
            removeTrackingArea(tracker!)
        }

        tracker = NSTrackingArea(rect: bounds,
            options: [.activeAlways, .mouseEnteredAndExited, .mouseMoved, .enabledDuringMouseDrag],
            owner: self, userInfo: nil)
        addTrackingArea(tracker!)

        if containsMouseLocation() {
            cocoa_put_key_with_modifiers(SWIFT_KEY_MOUSE_LEAVE, 0)
        }
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        guard let types = sender.draggingPasteboard().types else { return [] }
        if types.contains(NSFilenamesPboardType) || types.contains(NSURLPboardType) {
            return .copy
        }
        return []
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        let pb = sender.draggingPasteboard()
        guard let types = sender.draggingPasteboard().types else { return false }
        if types.contains(NSFilenamesPboardType) {
            if let files = pb.propertyList(forType: NSFilenamesPboardType) as? [Any] {
                EventsResponder.sharedInstance().handleFilesArray(files)
                return true
            }
        } else if types.contains(NSURLPboardType) {
            if let url = pb.propertyList(forType: NSURLPboardType) as? [Any] {
                EventsResponder.sharedInstance().handleFilesArray(url)
                return true
            }
        }
        return false
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
        if mpv.getBoolProperty("input-cursor") {
            cocoa_put_key_with_modifiers(SWIFT_KEY_MOUSE_ENTER, 0)
        }
    }

    override func mouseExited(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            cocoa_put_key_with_modifiers(SWIFT_KEY_MOUSE_LEAVE, 0)
        }
        cocoaCB.window.hideTitleBar()
    }

    override func mouseMoved(with event: NSEvent) {
        if mpv != nil && mpv.getBoolProperty("input-cursor") {
            signalMouseMovement(event)
        }
        cocoaCB.window.showTitleBar()
    }

    override func mouseDragged(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseMovement(event)
        }
    }

    override func mouseDown(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseDown(event)
        }
    }

    override func mouseUp(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseUp(event)
        }
        cocoaCB.window.isMoving = false
    }

    override func rightMouseDown(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseDown(event)
        }
    }

    override func rightMouseUp(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseUp(event)
        }
    }

    override func otherMouseDown(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseDown(event)
        }
    }

    override func otherMouseUp(with event: NSEvent) {
        if mpv.getBoolProperty("input-cursor") {
            signalMouseUp(event)
        }
    }

    func signalMouseDown(_ event: NSEvent) {
        signalMouseEvent(event, SWIFT_KEY_STATE_DOWN)
        if event.clickCount > 1 {
            signalMouseEvent(event, SWIFT_KEY_STATE_UP)
        }
    }

    func signalMouseUp(_ event: NSEvent) {
        signalMouseEvent(event, SWIFT_KEY_STATE_UP)
    }

    func signalMouseEvent(_ event: NSEvent, _ state: Int32) {
        hasMouseDown = state == SWIFT_KEY_STATE_DOWN
        let mpkey = getMpvButton(event)
        cocoa_put_key_with_modifiers((mpkey | state), Int32(event.modifierFlags.rawValue));
    }

    func signalMouseMovement(_ event: NSEvent) {
        var point = convert(event.locationInWindow, from: nil)
        point = convertToBacking(point)
        point.y = -point.y

        cocoaCB.window.updateMovableBackground(point)
        if !cocoaCB.window.isMoving {
            mpv.setMousePosition(point)
        }
    }

    func preciseScroll(_ event: NSEvent) {
        var delta: Double
        var cmd: Int32

        if fabs(event.deltaY) >= fabs(event.deltaX) {
            delta = Double(event.deltaY) * 0.1;
            cmd = delta > 0 ? SWIFT_WHEEL_UP : SWIFT_WHEEL_DOWN;
        } else {
            delta = Double(event.deltaX) * 0.1;
            cmd = delta > 0 ? SWIFT_WHEEL_RIGHT : SWIFT_WHEEL_LEFT;
        }

        mpv.putAxis(cmd, delta: fabs(delta))
    }

    override func scrollWheel(with event: NSEvent) {
        if !mpv.getBoolProperty("input-cursor") {
            return
        }

        if event.hasPreciseScrollingDeltas {
            preciseScroll(event)
        } else {
            let modifiers = event.modifierFlags
            let deltaX = modifiers.contains(.shift) ? event.scrollingDeltaY : event.scrollingDeltaX
            let deltaY = modifiers.contains(.shift) ? event.scrollingDeltaX : event.scrollingDeltaY
            var mpkey: Int32

            if fabs(deltaY) >= fabs(deltaX) {
                mpkey = deltaY > 0 ? SWIFT_WHEEL_UP : SWIFT_WHEEL_DOWN;
            } else {
                mpkey = deltaX > 0 ? SWIFT_WHEEL_RIGHT : SWIFT_WHEEL_LEFT;
            }

            cocoa_put_key_with_modifiers(mpkey, Int32(modifiers.rawValue))
        }
    }

    func containsMouseLocation() -> Bool {
        if cocoaCB == nil { return false }
        var topMargin: CGFloat = 0.0
        let menuBarHeight = NSApp.mainMenu!.menuBarHeight

        if cocoaCB.window.isInFullscreen && (menuBarHeight > 0) {
            topMargin = cocoaCB.window.titleBarHeight + 1 + menuBarHeight
        }

        guard var vF = window?.screen?.frame else { return false }
        vF.size.height -= topMargin

        let vFW = window!.convertFromScreen(vF)
        let vFV = convert(vFW, from: nil)
        let pt = convert(window!.mouseLocationOutsideOfEventStream, from: nil)

        var clippedBounds = bounds.intersection(vFV)
        if !cocoaCB.window.isInFullscreen {
            clippedBounds.origin.y += cocoaCB.window.titleBarHeight
            clippedBounds.size.height -= cocoaCB.window.titleBarHeight
        }
        return clippedBounds.contains(pt)
    }

    func canHideCursor() -> Bool {
        if cocoaCB.window == nil { return false }
        return !hasMouseDown && containsMouseLocation() && window!.isKeyWindow
    }

    func getMpvButton(_ event: NSEvent) -> Int32 {
        let buttonNumber = event.buttonNumber
        switch (buttonNumber) {
            case 0:  return SWIFT_MBTN_LEFT;
            case 1:  return SWIFT_MBTN_RIGHT;
            case 2:  return SWIFT_MBTN_MID;
            case 3:  return SWIFT_MBTN_BACK;
            case 4:  return SWIFT_MBTN_FORWARD;
            default: return SWIFT_MBTN9 + Int32(buttonNumber - 5);
        }
    }
}
