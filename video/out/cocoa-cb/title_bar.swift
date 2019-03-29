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

class TitleBar: NSVisualEffectView {

    weak var cocoaCB: CocoaCB! = nil
    var mpv: MPVHelper! {
        get { return cocoaCB == nil ? nil : cocoaCB.mpv }
    }

    var systemBar: NSView {
        get { return (cocoaCB.window.standardWindowButton(.closeButton)?.superview)! }
    }
    static var height: CGFloat {
        get { return NSWindow.frameRect(forContentRect: CGRect.zero, styleMask: .titled).size.height }
    }
    var buttons: [NSButton] {
        get { return ([.closeButton, .miniaturizeButton, .zoomButton] as [NSWindowButton]).flatMap { cocoaCB.window.standardWindowButton($0) } }
    }

    convenience init(frame: NSRect, window: NSWindow, cocoaCB ccb: CocoaCB) {
        let f = NSMakeRect(0, frame.size.height - TitleBar.height,
                           frame.size.width, TitleBar.height)
        self.init(frame: f)
        cocoaCB = ccb
        alphaValue = 0
        blendingMode = .withinWindow
        autoresizingMask = [.viewWidthSizable, .viewMinYMargin]
        systemBar.alphaValue = 0

        window.contentView!.addSubview(self, positioned: .above, relativeTo: nil)
        window.titlebarAppearsTransparent = true
        window.styleMask.insert(.fullSizeContentView)
        setStyle(Int(mpv.macOpts!.macos_title_bar_style))
    }

    // catch these events so they are not propagated to the underlying view
    override func mouseDown(with event: NSEvent) { }

    override func mouseUp(with event: NSEvent) {
        if event.clickCount > 1 {
            let def = UserDefaults.standard
            var action = def.string(forKey: "AppleActionOnDoubleClick")

            // macOS 10.10 and earlier
            if action == nil {
                action = def.bool(forKey: "AppleMiniaturizeOnDoubleClick") == true ?
                    "Minimize" : "Maximize"
            }

            if action == "Minimize" {
                cocoaCB.window.miniaturize(self)
            } else if action == "Maximize" {
                cocoaCB.window.zoom(self)
            }
        }
    }

    func setStyle(_ style: Any) {
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
            cocoaCB.window.appearance = NSAppearance(named: NSAppearanceNameVibrantLight)
            material = .titlebar
            state = .followsWindowActiveState
        case "light":
            cocoaCB.window.appearance = NSAppearance(named: NSAppearanceNameVibrantLight)
            material = .light
            state = .active
        case "ultradark":
            cocoaCB.window.appearance = NSAppearance(named: NSAppearanceNameVibrantDark)
            material = .titlebar
            state = .followsWindowActiveState
        case "dark": fallthrough
        default:
            cocoaCB.window.appearance = NSAppearance(named: NSAppearanceNameVibrantDark)
            material = .dark
            state = .active
        }
    }

    func show() {
        if (!cocoaCB.window.border && !cocoaCB.window.isInFullscreen) { return }
        let loc = cocoaCB.view.convert(cocoaCB.window.mouseLocationOutsideOfEventStream, from: nil)

        buttons.forEach { $0.isHidden = false }
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = 0.20
            systemBar.animator().alphaValue = 1
            if !cocoaCB.window.isInFullscreen && !cocoaCB.window.isAnimating {
                animator().alphaValue = 1
                isHidden = false
            }
        }, completionHandler: nil )

        if loc.y > TitleBar.height {
            hideDelayed()
        } else {
            NSObject.cancelPreviousPerformRequests(withTarget: self, selector: #selector(hide), object: nil)
        }
    }

    func hide() {
        if cocoaCB.window.isInFullscreen && !cocoaCB.window.isAnimating {
            alphaValue = 0
            isHidden = true
            return
        }
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = 0.20
            systemBar.animator().alphaValue = 0
            animator().alphaValue = 0
        }, completionHandler: {
            self.buttons.forEach { $0.isHidden = true }
            self.isHidden = true
        })
    }

    func hideDelayed() {
        NSObject.cancelPreviousPerformRequests(withTarget: self,
                                                 selector: #selector(hide),
                                                   object: nil)
        perform(#selector(hide), with: nil, afterDelay: 0.5)
    }
}
