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
    var option: OptionHelper { return common.option }

    var systemBar: NSView? { return common.window?.standardWindowButton(.closeButton)?.superview }
    static var height: CGFloat {
        return NSWindow.frameRect(forContentRect: CGRect.zero, styleMask: .titled).size.height
    }
    var buttons: [NSButton] {
        return ([.closeButton, .miniaturizeButton, .zoomButton] as [NSWindow.ButtonType]).compactMap { common.window?.standardWindowButton($0) }
    }

    override var material: NSVisualEffectView.Material {
        get { return super.material }
        set {
            super.material = newValue
            // fix for broken deprecated materials
            if material == .light || material == .dark || material == .mediumLight || material == .ultraDark {
                state = .active
            } else {
                state = .followsWindowActiveState
            }
        }
    }

    init(frame: NSRect, window: NSWindow, common com: Common) {
        let f = NSRect(x: 0, y: frame.size.height - TitleBar.height, width: frame.size.width, height: TitleBar.height + 1)
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
        set(appearance: option.mac.macos_title_bar_appearance)
        set(material: option.mac.macos_title_bar_material)
        set(color: option.mac.macos_title_bar_color)
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

    func set(appearance: Int32) {
        window?.appearance = { switch Int(appearance) {
            case MAC_APPEAR_AQUA: return NSAppearance(named: .aqua)
            case MAC_APPEAR_DARK_AQUA: return NSAppearance(named: .darkAqua)
            case MAC_APPEAR_VIBRANT_LIGHT: return NSAppearance(named: .vibrantLight)
            case MAC_APPEAR_VIBRANT_DARK: return NSAppearance(named: .vibrantDark)
            case MAC_APPEAR_AQUA_HC: return NSAppearance(named: .accessibilityHighContrastAqua)
            case MAC_APPEAR_DARK_AQUA_HC: return NSAppearance(named: .accessibilityHighContrastDarkAqua)
            case MAC_APPEAR_VIBRANT_LIGHT_HC: return NSAppearance(named: .accessibilityHighContrastVibrantLight)
            case MAC_APPEAR_VIBRANT_DARK_HC: return NSAppearance(named: .accessibilityHighContrastVibrantDark)
            case MAC_APPEAR_AUTO: return nil
            default: return nil
            }
        }()
    }

    func set(material: Int32) {
        self.material = { switch Int(material) {
            case MAC_MAT_TITLEBAR: return .titlebar
            case MAC_MAT_SELECTION: return .selection
            case MAC_MAT_MENU: return .menu
            case MAC_MAT_POPOVER: return .popover
            case MAC_MAT_SIDEBAR: return .sidebar
            case MAC_MAT_HEADER_VIEW: return .headerView
            case MAC_MAT_SHEET: return .sheet
            case MAC_MAT_WINDOW_BACKGROUND: return .windowBackground
            case MAC_MAT_HUD_WINDOW: return .hudWindow
            case MAC_MAT_FULL_SCREEN: return .fullScreenUI
            case MAC_MAT_TOOL_TIP: return .toolTip
            case MAC_MAT_CONTENT_BACKGROUND: return .contentBackground
            case MAC_MAT_UNDER_WINDOW_BACKGROUND: return .underWindowBackground
            case MAC_MAT_UNDER_PAGE_BACKGROUND: return .underPageBackground
            case MAC_MAT_DARK: return .dark
            case MAC_MAT_LIGHT: return .light
            case MAC_MAT_MEDIUM_LIGHT: return .mediumLight
            case MAC_MAT_ULTRA_DARK: return .ultraDark
            default: return .titlebar
            }
        }()
    }

    func set(color: m_color) {
        layer?.backgroundColor = NSColor(calibratedRed: CGFloat(color.r)/255,
                                         green: CGFloat(color.g)/255,
                                         blue: CGFloat(color.b)/255,
                                         alpha: CGFloat(color.a)/255).cgColor
    }

    func show() {
        guard let window = common.window else { return }
        if !window.border && !window.isInFullscreen { return }
        let loc = common.view?.convert(window.mouseLocationOutsideOfEventStream, from: nil)

        buttons.forEach { $0.isHidden = false }
        NSAnimationContext.runAnimationGroup({ (context) in
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
        NSAnimationContext.runAnimationGroup({ (context) in
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
}
