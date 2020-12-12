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
    unowned var common: Common
    var mpv: MPVHelper? { get { return common.mpv } }

    var systemBar: NSView? {
        get { return common.window?.standardWindowButton(.closeButton)?.superview }
    }
    static var height: CGFloat {
        get { return NSWindow.frameRect(forContentRect: CGRect.zero, styleMask: .titled).size.height }
    }
    var buttons: [NSButton] {
        get { return ([.closeButton, .miniaturizeButton, .zoomButton] as [NSWindow.ButtonType]).compactMap { common.window?.standardWindowButton($0) } }
    }

    override var material: NSVisualEffectView.Material {
        get { return super.material }
        set {
            super.material = newValue
            // fix for broken deprecated materials
            if material == .light || material == .dark {
                state = .active
            } else if #available(macOS 10.11, *),
                      material == .mediumLight || material == .ultraDark
            {
                state = .active
            } else {
                state = .followsWindowActiveState
            }

        }
    }

    init(frame: NSRect, window: NSWindow, common com: Common) {
        let f = NSMakeRect(0, frame.size.height - TitleBar.height,
                           frame.size.width, TitleBar.height)
        common = com
        super.init(frame: f)
        buttons.forEach { $0.isHidden = true }
        isHidden = true
        alphaValue = 0
        blendingMode = .withinWindow
        autoresizingMask = [.width, .minYMargin]
        systemBar?.alphaValue = 0
        state = .followsWindowActiveState
        wantsLayer = true

        window.contentView?.addSubview(self, positioned: .above, relativeTo: nil)
        window.titlebarAppearsTransparent = true
        window.styleMask.insert(.fullSizeContentView)
        set(appearance: Int(mpv?.macOpts.macos_title_bar_appearance ?? 0))
        set(material: Int(mpv?.macOpts.macos_title_bar_material ?? 0))
        set(color: mpv?.macOpts.macos_title_bar_color ?? "#00000000")
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
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
                window?.miniaturize(self)
            } else if action == "Maximize" {
                window?.zoom(self)
            }
        }

        common.window?.isMoving = false
    }

    func set(appearance: Any) {
        if appearance is Int {
            window?.appearance = appearanceFrom(string: String(appearance as? Int ?? 0))
        } else {
            window?.appearance = appearanceFrom(string: appearance as? String ?? "auto")
        }
    }

    func set(material: Any) {
        if material is Int {
            self.material = materialFrom(string: String(material as? Int ?? 0))
        } else {
            self.material = materialFrom(string: material as? String ?? "titlebar")
        }
    }

    func set(color: Any) {
        if color is String {
            layer?.backgroundColor = NSColor(hex: color as? String ?? "#00000000").cgColor
        } else {
            let col = color as? m_color ?? m_color(r: 0, g: 0, b: 0, a: 0)
            let red   = CGFloat(col.r)/255
            let green = CGFloat(col.g)/255
            let blue  = CGFloat(col.b)/255
            let alpha = CGFloat(col.a)/255
            layer?.backgroundColor = NSColor(calibratedRed: red, green: green,
                                             blue: blue, alpha: alpha).cgColor
        }
    }

    func show() {
        guard let window = common.window else { return }
        if !window.border && !window.isInFullscreen { return }
        let loc = common.view?.convert(window.mouseLocationOutsideOfEventStream, from: nil)

        buttons.forEach { $0.isHidden = false }
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = 0.20
            systemBar?.animator().alphaValue = 1
            if !window.isInFullscreen && !window.isAnimating {
                animator().alphaValue = 1
                isHidden = false
            }
        }, completionHandler: nil )

        if loc?.y ?? 0 > TitleBar.height {
            hideDelayed()
        } else {
            NSObject.cancelPreviousPerformRequests(withTarget: self, selector: #selector(hide), object: nil)
        }
    }

    @objc func hide(_ duration: TimeInterval = 0.20) {
        guard let window = common.window else { return }
        if window.isInFullscreen && !window.isAnimating {
            alphaValue = 0
            isHidden = true
            return
        }
        NSAnimationContext.runAnimationGroup({ (context) -> Void in
            context.duration = duration
            systemBar?.animator().alphaValue = 0
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

    func appearanceFrom(string: String) -> NSAppearance? {
        switch string {
        case "1", "aqua":
            return NSAppearance(named: .aqua)
        case "3", "vibrantLight":
            return NSAppearance(named: .vibrantLight)
        case "4", "vibrantDark":
            return NSAppearance(named: .vibrantDark)
        default: break
        }

        if #available(macOS 10.14, *) {
            switch string {
            case "2", "darkAqua":
                return NSAppearance(named: .darkAqua)
            case "5", "aquaHighContrast":
                return NSAppearance(named: .accessibilityHighContrastAqua)
            case "6", "darkAquaHighContrast":
                return NSAppearance(named: .accessibilityHighContrastDarkAqua)
            case "7", "vibrantLightHighContrast":
                return NSAppearance(named: .accessibilityHighContrastVibrantLight)
            case "8", "vibrantDarkHighContrast":
                return NSAppearance(named: .accessibilityHighContrastVibrantDark)
            case "0", "auto": fallthrough
            default:
#if HAVE_MACOS_10_14_FEATURES
                return nil
#else
                break
#endif
            }
        }

        let style = UserDefaults.standard.string(forKey: "AppleInterfaceStyle")
        return appearanceFrom(string: style == nil ? "aqua" : "vibrantDark")
    }

    func materialFrom(string: String) -> NSVisualEffectView.Material {
        switch string {
        case "1",  "selection": return .selection
        case "0",  "titlebar":  return .titlebar
        case "14", "dark":      return .dark
        case "15", "light":     return .light
        default:                break
        }

#if HAVE_MACOS_10_11_FEATURES
        if #available(macOS 10.11, *) {
            switch string {
            case "2,", "menu":          return .menu
            case "3",  "popover":       return .popover
            case "4",  "sidebar":       return .sidebar
            case "16", "mediumLight":   return .mediumLight
            case "17", "ultraDark":     return .ultraDark
            default:                    break
            }
        }
#endif
#if HAVE_MACOS_10_14_FEATURES
        if #available(macOS 10.14, *) {
            switch string {
            case "5,", "headerView":            return .headerView
            case "6",  "sheet":                 return .sheet
            case "7",  "windowBackground":      return .windowBackground
            case "8",  "hudWindow":             return .hudWindow
            case "9",  "fullScreen":            return .fullScreenUI
            case "10", "toolTip":               return .toolTip
            case "11", "contentBackground":     return .contentBackground
            case "12", "underWindowBackground": return .underWindowBackground
            case "13", "underPageBackground":   return .underPageBackground
            default:                            break
            }
        }
#endif

        return .titlebar
    }
}
