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
import Foundation

enum ContextMenu {
    enum ItemType {
        case separator, submenu, empty
    }
    struct ItemState: OptionSet {
        let rawValue: Int
        static let checked = ItemState(rawValue: 1 << 0)
        static let disabled = ItemState(rawValue: 1 << 1)
        static let hidden = ItemState(rawValue: 1 << 2)
    }
    struct Item {
        var type = ItemType.empty
        var title: String = ""
        var cmd: String?
        var shortcut: String?
        var state = ItemState()
        var submenu: [Item] = []
    }
    private static func unescapeMnemonic(_ s: String) -> String {
        var out = String()
        out.reserveCapacity(s.count)
        var i = s.startIndex
        while i < s.endIndex {
            if s[i] == "&" {
                let j = s.index(after: i)
                if j < s.endIndex, s[j] == "&" {
                    out.append("&")  // "&&" -> "&"
                    i = s.index(after: j)
                    continue
                }
                i = j  // drop "&"
                continue
            }
            out.append(s[i])
            i = s.index(after: i)
        }
        return out
    }
    static func buildMenu(_ items: [Item], target: AnyObject?, action: Selector?) -> NSMenu {
        let menu = NSMenu()
        menu.autoenablesItems = false
        for i in items {
            if i.state.contains(.hidden) {
                continue
            }
            if i.type == .separator {
                menu.addItem(.separator())
                continue
            }
            if i.title.isEmpty {
                continue
            }
            let item = NSMenuItem()
            if i.state.contains(.checked) {
                item.state = .on
            }
            if i.type == .submenu {
                item.submenu = buildMenu(i.submenu, target: target, action: action)
                item.isEnabled = !i.state.contains(.disabled) && !i.submenu.isEmpty
            } else {
                if let cmd = i.cmd {
                    item.isEnabled =
                        !i.state.contains(.disabled) && !cmd.isEmpty
                        && cmd != "ignore" && !cmd.starts(with: "#")
                } else {
                    item.isEnabled = false
                }
            }
            item.title = i.title
            item.action = action
            item.target = target
            item.representedObject = i.cmd
            menu.addItem(item)
        }
        return menu
    }
    private static func buildState(_ node: mpv_node) -> ItemState {
        var state = ItemState()
        guard let list = node.u.list else {
            return state
        }
        for i in 0..<Int(list.pointee.num) {
            let value = list.pointee.values[i]
            if value.format != MPV_FORMAT_STRING {
                continue
            }
            if strcmp(value.u.string, "hidden") == 0 {
                state.insert(.hidden)
            } else if strcmp(value.u.string, "checked") == 0 {
                state.insert(.checked)
            } else if strcmp(value.u.string, "disabled") == 0 {
                state.insert(.disabled)
            }
        }
        return state
    }
    static func buildNodeItems(_ node: mpv_node) -> [Item] {
        var items: [Item] = []
        guard node.format == MPV_FORMAT_NODE_ARRAY,
            let list = node.u.list
        else {
            return items
        }
        for i in 0..<Int(list.pointee.num) {
            let value = list.pointee.values[i]
            guard value.format == MPV_FORMAT_NODE_MAP, let list = value.u.list else {
                continue
            }
            var item = Item()
            for j in 0..<Int(list.pointee.num) {
                let key = list.pointee.keys[j]
                let value = list.pointee.values[j]
                switch value.format {
                case MPV_FORMAT_STRING:
                    if strcmp(key, "title") == 0 {
                        item.title = unescapeMnemonic(String(cString: value.u.string))
                    } else if strcmp(key, "cmd") == 0 {
                        item.cmd = String(cString: value.u.string)
                    } else if strcmp(key, "type") == 0 {
                        if value.u.string.pointee == 0 {
                            item.type = ItemType.empty
                        } else if strcmp(value.u.string, "separator") == 0 {
                            item.type = ItemType.separator
                        } else if strcmp(value.u.string, "submenu") == 0 {
                            item.type = ItemType.submenu
                        }
                    } else if strcmp(key, "shortcut") == 0 {
                        item.shortcut = String(cString: value.u.string)
                    }
                case MPV_FORMAT_NODE_ARRAY:
                    if strcmp(key, "state") == 0 {
                        item.state = buildState(value)
                    } else if strcmp(key, "submenu") == 0 {
                        item.submenu = buildNodeItems(value)
                    }
                default:
                    break
                }
            }
            items.append(item)
        }
        return items
    }
}
