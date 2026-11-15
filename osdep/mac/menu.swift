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

extension Menu {
    class MenuItem: NSMenuItem {
        var config: Config?
    }

    enum `Type`: Comparable {
        case menu
        case menuWindow
        case menuServices
        case separator
        case item
        case itemNormalSize
        case itemHalfSize
        case itemDoubleSize
        case itemMinimize
        case itemZoom
    }

    struct Config {
        let name: String
        let key: String
        let modifiers: NSEvent.ModifierFlags
        let type: Type
        let action: Selector?
        let target: AnyObject?
        let command: String
        let url: String
        var configs: [Config]

        init(
            name: String = "",
            key: String = "",
            modifiers: NSEvent.ModifierFlags = .command,
            type: Type? = nil,
            action: Selector? = nil,
            target: AnyObject? = nil,
            command: String = "",
            url: String = "",
            configs: [Config] = []
        ) {
            self.name = name
            self.key = key
            self.modifiers = modifiers
            self.type = type ?? (configs.isEmpty ? .item : .menu)
            self.action = action
            self.target = target
            self.command = command
            self.url = url
            self.configs = configs
        }
    }
}

class Menu: NSObject {
    let input: InputHelper
    let option: OptionHelper?
    var dynamicMenuItems: [Type: [MenuItem]] = [:]

    init(input: InputHelper, option: OptionHelper?) {
        self.input = input
        self.option = option
        super.init()
    }

    func createMenu(parentMenu: NSMenu, configs: [Config]) {
        for config in configs {
            let item = createMenuItem(parentMenu: parentMenu, config: config)

            if config.type <= .menuServices {
                let menu = createSubmenu(for: config)
                item.submenu = menu
                createMenu(parentMenu: menu, configs: config.configs)
            }

            if config.type > Type.item {
                dynamicMenuItems[config.type] = (dynamicMenuItems[config.type] ?? []) + [item]
            }
        }
    }

    func createMenuItem(parentMenu: NSMenu, config: Config) -> MenuItem {
        var item = MenuItem(title: config.name, action: config.action,
              keyEquivalent: (option?.mac.macos_menu_shortcuts ?? true) ? config.key : "")
        item.config = config
        item.target = config.target
        item.keyEquivalentModifierMask = config.modifiers

        if config.type == .separator {
            item = MenuItem.separator() as? MenuItem ?? item
        }
        parentMenu.addItem(item)

        return item
    }

    func createSubmenu(for config: Config) -> NSMenu {
        return NSMenu(title: config.name)
    }

    func register(_ selector: Selector, key: Type) {
        for menuItem in dynamicMenuItems[key] ?? [] {
            menuItem.action = selector
        }
    }

    @objc func command(_ menuItem: MenuItem) {
        guard let menuConfig = menuItem.config else { return }
        input.command(menuConfig.command)
    }

    @objc func url(_ menuItem: MenuItem) {
        guard let menuConfig = menuItem.config,
              let url = URL(string: menuConfig.url) else { return }
        NSWorkspace.shared.open(url)
    }
}
