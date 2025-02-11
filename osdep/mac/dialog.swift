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

class Dialog {
    func open(directories: Bool, multiple: Bool) -> [String]? {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = directories
        panel.allowsMultipleSelection = multiple

        if panel.runModal() == .OK {
            return panel.urls.map { $0.path }
        }

        return nil
    }

    func openUrl() -> String? {
        let alert = NSAlert()
        alert.messageText = "Open URL"
        alert.icon = AppHub.shared.getIcon()
        alert.addButton(withTitle: "Ok")
        alert.addButton(withTitle: "Cancel")

        let input = NSTextField(frame: NSRect(x: 0, y: 0, width: 300, height: 24))
        input.placeholderString = "URL"
        alert.accessoryView = input

        DispatchQueue.main.asyncAfter(deadline: DispatchTime.now() + 0.1) {
            input.becomeFirstResponder()
        }

        if alert.runModal() == .alertFirstButtonReturn && input.stringValue.count > 0 {
            return input.stringValue
        }

        return nil
    }

    func alert(title: String, text: String) {
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = text
        alert.icon = AppHub.shared.getIcon()
        alert.addButton(withTitle: "Ok")
        alert.runModal()
    }
}
