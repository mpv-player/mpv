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

class MenuContext: Menu {
    weak var common: Common! = nil
    let menu = NSMenu(title: "Context")
    // always keep shortcuts enabled for the context menu, the option is only
    // meant for the menu bar so shortcuts there can be rebound
    override var shortcutsEnabled: Bool { return true }

    init(common com: Common) {
        common = com
        super.init(input: com.input ?? InputHelper(), option: com.option)
    }

    func update(_ menuArray: [Any?]) {
        DispatchQueue.main.async {
            self.menu.removeAllItems()
            self.createMenu(parentMenu: self.menu, configs: self.map(menu: menuArray))
        }
    }

    func show() {
        DispatchQueue.main.async {
            guard let view = self.common.view, let window = self.common.window else { return }

            let pt = view.convert(window.mouseLocationOutsideOfEventStream, from: nil)
            self.input.put(key: MP_INPUT_RELEASE_ALL)
            self.menu.popUp(positioning: nil, at: pt, in: view)
        }
    }

    func map(menu: [Any?]) -> [Config] {
        var configs: [Config] = []

        for element in menu {
            guard let item = element as? [String: Any?] else { continue }

            let state = (item["state"] as? [String]) ?? []
            if state.contains("hidden") { continue }

            let type = item["type"] as? String
            if type == "separator" {
                configs.append(Config(type: .separator))
                continue
            }

            let title = (item["title"] as? String)?
                .components(separatedBy: "&&")
                .map { $0.replacingOccurrences(of: "&", with: "") }
                .joined(separator: "&") ?? ""
            let cmd = item["cmd"] as? String ?? ""
            let submenu = (item["submenu"] as? [Any?]).map { map(menu: $0) } ?? []

            configs.append(Config(
                name: title,
                action: type == "submenu" || cmd.isEmpty ? nil : #selector(command(_:)),
                target: self,
                command: cmd,
                configs: submenu
            ))
        }

        return configs
    }
}
