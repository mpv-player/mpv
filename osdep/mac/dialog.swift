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
import UniformTypeIdentifiers

class Dialog: NSObject, NSWindowDelegate, NSOpenSavePanelDelegate {
    var option: OptionHelper?

    @objc override init() {
        super.init()
    }

    init(_ option: OptionHelper? = nil) {
        self.option = option
    }

    func openFiles(title: String? = nil, path: URL? = nil) -> [String]? {
         let types: [UTType] = (TypeHelper.toStringArray(option?.root.video_exts) +
            TypeHelper.toStringArray(option?.root.audio_exts) +
            TypeHelper.toStringArray(option?.root.image_exts) +
            TypeHelper.toStringArray(option?.root.archive_exts) +
            TypeHelper.toStringArray(option?.root.playlist_exts)).compactMap { UTType(filenameExtension: $0) }
        return open(title: title, path: path, types: types)
    }

    func openPlaylist(title: String? = nil, path: URL? = nil) -> String? {
        let types: [UTType] = TypeHelper.toStringArray(option?.root.playlist_exts).compactMap { UTType(filenameExtension: $0) }
        return open(title: title, path: path, directories: false, multiple: false, types: types)?.first
    }

    func open(title: String? = nil, path: URL? = nil, files: Bool = true,
              directories: Bool = true, multiple: Bool = true, types: [UTType] = []) -> [String]? {
        let panel = NSOpenPanel()
        panel.title = title ?? panel.title
        panel.directoryURL = path
        panel.canChooseFiles = files
        panel.canChooseDirectories = directories
        panel.allowsMultipleSelection = multiple
        panel.allowedContentTypes = types
        panel.delegate = self

        if panel.runModal() == .OK {
            return panel.urls.map { $0.path }
        }

        return nil
    }

    func save(title: String? = nil, name: String? = nil, path: URL? = nil) -> [String]? {
        let panel = NSSavePanel()
        panel.title = title ?? panel.title
        panel.nameFieldStringValue = name ?? panel.nameFieldStringValue
        panel.directoryURL = path
        panel.delegate = self

        if panel.runModal() == .OK {
            if let url = panel.url {
                return [url.path]
            }
        }

        return nil
    }

    @objc func openDialog(_ tallocCtx: UnsafeMutableRawPointer,
                          params: UnsafePointer<mp_file_dialog_params>) -> UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>? {
        let conf = params.pointee
        let isSave = Bool(conf.flags.rawValue & MP_FILE_DIALOG_SAVE.rawValue)
        let directories = Bool(conf.flags.rawValue & MP_FILE_DIALOG_DIRECTORY.rawValue)
        let multiple = Bool(conf.flags.rawValue & MP_FILE_DIALOG_MULTIPLE.rawValue)
        let title = conf.title != nil ? String(cString: conf.title) : nil
        let path = conf.initial_dir != nil ? URL(fileURLWithPath: String(cString: conf.initial_dir)): nil
        let name = conf.initial_selection != nil ? String(cString: conf.initial_selection) : nil
        var types: [UTType] = []

        if var filters = conf.filters {
            while filters.pointee.name != nil, let exts = filters.pointee.extensions {
                types += TypeHelper.toStringArray(exts).compactMap { UTType(filenameExtension: $0) }
                filters += 1
            }
        }

        var paths: [String]?
        DispatchQueue.main.sync {
            paths = isSave ? save(title: title, name: name, path: path) :
                open(title: title, path: path, files: !directories, directories: directories, multiple: multiple, types: types)
        }

        guard let files = paths else { return nil }

        var retCount: Int32 = 0
        var ret: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?
        for file in files {
            let item = ta_xstrdup(tallocCtx, file)
            app_bridge_tarray_append(tallocCtx, &ret, &retCount, item)
            ta_steal_(ret, item)
        }

        if retCount > 0 {
            app_bridge_tarray_append(tallocCtx, &ret, &retCount, nil)
        }

        return ret
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
        alert.window.initialFirstResponder = input

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

    func windowDidBecomeKey(_ notification: Notification) {
        AppHub.shared.input.put(key: MP_INPUT_RELEASE_ALL)
    }
}
