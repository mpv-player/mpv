#!/usr/bin/env swift
// Creates mpv app bundle from binary.

import Foundation

// MARK: - process helpers

struct RuntimeError: Error, CustomStringConvertible {
    let message: String
    init(_ message: String) { self.message = message }
    var description: String { message }
}

func checkOutput(_ cmd: String, _ args: [String], suppressStderr: Bool = false) throws -> String {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    process.arguments = [cmd] + args
    let outPipe = Pipe()
    process.standardOutput = outPipe
    if suppressStderr {
        process.standardError = FileHandle.nullDevice
    }
    try process.run()
    let data = outPipe.fileHandleForReading.readDataToEndOfFile()
    process.waitUntilExit()
    if process.terminationStatus != 0 {
        throw RuntimeError("\(cmd) \(args.joined(separator: " ")) failed")
    }
    return String(data: data, encoding: .utf8) ?? ""
}

@discardableResult
func call(_ cmd: String, _ args: [String], suppressStderr: Bool = false, suppressStdout: Bool = false) -> Int32 {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    process.arguments = [cmd] + args
    if suppressStderr {
        process.standardError = FileHandle.nullDevice
    }
    if suppressStdout {
        process.standardOutput = FileHandle.nullDevice
    }
    do {
        try process.run()
    } catch {
        return -1
    }
    process.waitUntilExit()
    return process.terminationStatus
}

// MARK: - path helpers

func pathJoin(_ a: String, _ b: String) -> String {
    if b.hasPrefix("/") { return b }
    if a.isEmpty { return b }
    if a.hasSuffix("/") { return a + b }
    return a + "/" + b
}

func normalizePath(_ path: String) -> String {
    let isAbsolute = path.hasPrefix("/")
    var components: [String] = []
    for part in path.split(separator: "/") {
        if part == "." { continue }
        if part == ".." {
            if let last = components.last, last != ".." {
                components.removeLast()
            } else if !isAbsolute {
                components.append("..")
            }
            continue
        }
        components.append(String(part))
    }
    let joined = components.joined(separator: "/")
    if joined.isEmpty {
        return isAbsolute ? "/" : "."
    }
    return (isAbsolute ? "/" : "") + joined
}

func absolutePath(_ path: String) -> String {
    if path.hasPrefix("/") { return normalizePath(path) }
    return normalizePath(pathJoin(FileManager.default.currentDirectoryPath, path))
}

// Copies the file the symlink points to, not the symlink itself
func copyFollowingSymlinks(from src: String, to dst: String) throws {
    let data = try Data(contentsOf: URL(fileURLWithPath: src))
    try data.write(to: URL(fileURLWithPath: dst))
    if let perms = try? FileManager.default.attributesOfItem(atPath: src)[.posixPermissions] {
        try? FileManager.default.setAttributes([.posixPermissions: perms], ofItemAtPath: dst)
    }
}

func basename(_ path: String) -> String {
    return (path as NSString).lastPathComponent
}

func dirname(_ path: String) -> String {
    return (path as NSString).deletingLastPathComponent
}

// MARK: - dylib_unhell

func isUserLib(_ libname: String) -> Bool {
    let base = basename(libname)
    return !libname.hasPrefix("/System") &&
           !libname.hasPrefix("/usr/lib/") &&
           !libname.hasPrefix("@executable_path") &&
           !base.contains("libobjc.") &&
           !base.contains("libSystem.") &&
           !base.contains("libc.") &&
           !base.contains("libgcc.") &&
           base != "Python" &&
           !base.contains("libswift")
}

// dylibs list their own LC_ID_DYLIB as the first `otool -L` entry; executables
// have none. Matching it exactly (via `otool -D`) avoids excluding real
// dependencies whose path merely happens to contain the binary's basename.
func otoolSelfId(_ objfile: String) throws -> String? {
    let output = try checkOutput("otool", ["-D", objfile])
    let lines = output.split(separator: "\n").map(String.init)
    guard lines.count >= 2 else { return nil }
    return lines[1].trimmingCharacters(in: .whitespaces)
}

func resolveLibPath(_ objfile: String, _ lib: String, _ rpaths: [String]) throws -> String {
    if FileManager.default.fileExists(atPath: lib) {
        return lib
    }

    if lib.hasPrefix("@rpath/") {
        let rel = String(lib.dropFirst("@rpath/".count))
        for rpath in rpaths {
            let libPath = pathJoin(rpath, rel)
            if FileManager.default.fileExists(atPath: libPath) {
                return libPath
            }
        }
    } else if lib.hasPrefix("@loader_path/") {
        let rel = String(lib.dropFirst("@loader_path/".count))
        // mirrors the python join(objfile, rel) + normpath, whose ".."
        // collapses against objfile's own basename to land in its directory
        let libPath = normalizePath(pathJoin(objfile, rel))
        if FileManager.default.fileExists(atPath: libPath) {
            return libPath
        }
    }

    throw RuntimeError("Could not resolve library: \(lib)")
}

func otoolLibs(_ objfile: String, _ rpaths: [String]) throws -> (Set<String>, Set<String>) {
    let output = try checkOutput("otool", ["-L", objfile])
    let selfId = try otoolSelfId(objfile)
    var libs = Set<String>()
    for line in output.split(separator: "\n", omittingEmptySubsequences: false) {
        guard line.hasPrefix("\t") else { continue }
        guard let lib = line.trimmingCharacters(in: .whitespaces)
            .split(separator: " ").first.map(String.init) else { continue }
        if lib == selfId { continue }
        if isUserLib(lib) {
            libs.insert(lib)
        }
    }

    var resolved = Set<String>()
    var relative = Set<String>()
    for lib in libs {
        let libPath = try resolveLibPath(objfile, lib, rpaths)
        resolved.insert(libPath)
        if libPath != lib {
            relative.insert(lib)
        }
    }
    return (resolved, relative)
}

func iterRpaths(_ objfile: String) throws -> [String] {
    let output = try checkOutput("otool", ["-l", objfile])
    var rpaths: [String] = []
    for line in output.split(separator: "\n") {
        let trimmed = line.trimmingCharacters(in: .whitespaces)
        guard trimmed.hasPrefix("path "), let offsetRange = trimmed.range(of: " (offset ") else { continue }
        let start = trimmed.index(trimmed.startIndex, offsetBy: 5)
        rpaths.append(String(trimmed[start..<offsetRange.lowerBound]))
    }
    return rpaths
}

func getRpaths(_ objfile: String) throws -> [String] {
    let loaderPath = dirname(objfile)
    return try iterRpaths(objfile).map { rpath in
        guard let range = rpath.range(of: "@loader_path") else { return normalizePath(rpath) }
        return normalizePath(rpath.replacingCharacters(in: range, with: loaderPath))
    }
}

func getRpathsDevTools(_ binary: String) throws -> [String] {
    return try iterRpaths(binary).filter { $0.contains("Xcode") || $0.contains("CommandLineTools") }
}

func checkVulkanMaxVersion(_ version: String) -> Bool {
    return call("pkg-config", ["vulkan", "--max-version=\(version)"], suppressStderr: true, suppressStdout: true) == 0
}

func getHomebrewPrefix() -> String {
    if let output = try? checkOutput("brew", ["--prefix"], suppressStderr: true) {
        let trimmed = output.trimmingCharacters(in: .whitespacesAndNewlines)
        if !trimmed.isEmpty { return trimmed }
    }
    return "/opt/homebrew"
}

func installNameToolChange(_ old: String, _ new: String, _ objfile: String) {
    call("install_name_tool", ["-change", old, new, objfile], suppressStderr: true)
}

func installNameToolId(_ name: String, _ objfile: String) {
    call("install_name_tool", ["-id", name, objfile], suppressStderr: true)
}

func installNameToolAddRpath(_ rpath: String, _ binary: String) {
    call("install_name_tool", ["-add_rpath", rpath, binary])
}

func installNameToolDeleteRpath(_ rpath: String, _ binary: String) {
    call("install_name_tool", ["-delete_rpath", rpath, binary])
}

func gatherLibraries(
    _ objfile: String,
    result: inout [String: Set<String>],
    resultRelative: inout Set<String>,
    rpaths: [String]
) throws {
    let allRpaths = try getRpaths(objfile) + rpaths
    let (libsList, libsRelative) = try otoolLibs(objfile, allRpaths)
    result[objfile] = libsList
    resultRelative.formUnion(libsRelative)

    for lib in libsList where result[lib] == nil {
        try gatherLibraries(lib, result: &result, resultRelative: &resultRelative, rpaths: allRpaths)
    }
}

func libPath(_ binary: String) -> String {
    return pathJoin(dirname(binary), "lib")
}

func resourcesPath(_ binary: String) -> String {
    return pathJoin(dirname(binary), "../Resources")
}

func libName(_ lib: String) -> String {
    return pathJoin("@executable_path", pathJoin("lib", basename(lib)))
}

func processLibraries(_ libsDict: [String: Set<String>], _ libsDyn: Set<String>, _ binary: String) throws {
    var libsSet = Set(libsDict.keys)
    libsSet.remove(binary)

    let libDir = libPath(binary)

    for src in libsSet {
        let name = libName(src)
        let dst = pathJoin(libDir, basename(src))

        if FileManager.default.fileExists(atPath: dst) {
            try FileManager.default.removeItem(atPath: dst)
        }
        print(">>> copying \(basename(src)) from \(src)")
        try copyFollowingSymlinks(from: src, to: dst)
        try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: dst)
        installNameToolId(name, dst)

        if libsDict[binary]?.contains(src) == true {
            installNameToolChange(src, name, binary)
        }

        for p in libsSet where libsDict[src]?.contains(p) == true {
            installNameToolChange(p, libName(p), dst)
        }

        for lib in libsDyn {
            installNameToolChange(lib, libName(lib), dst)
        }
    }

    for lib in libsDyn {
        installNameToolChange(lib, libName(lib), binary)
    }
}

func processSwiftLibraries(_ binary: String) throws {
    let swiftStdlibTool = try checkOutput("xcrun", ["--find", "swift-stdlib-tool"])
        .trimmingCharacters(in: .whitespacesAndNewlines)
    let swiftLibPath = absolutePath(pathJoin(swiftStdlibTool, "../../lib/swift-5.0/macosx"))

    var args = [
        "--copy", "--platform", "macosx",
        "--scan-executable", binary, "--destination", libPath(binary)
    ]

    if FileManager.default.fileExists(atPath: swiftLibPath) {
        args += ["--source-libraries", swiftLibPath]
    }

    _ = try checkOutput(swiftStdlibTool, args)

    print(">> setting additional rpath for swift libraries")
    installNameToolAddRpath("@executable_path/lib", binary)
}

func processVulkanLoader(
    _ binary: String,
    _ loaderName: String,
    _ loaderRelativeFolder: String,
    _ libraryNode: String
) throws {
    let homebrewPrefix = getHomebrewPrefix()
    let home = NSHomeDirectory()
    let loaderSystemSearchFolders = [
        pathJoin(home, pathJoin(".config", loaderRelativeFolder)),
        pathJoin("/etc/xdg", loaderRelativeFolder),
        pathJoin("/usr/local/etc", loaderRelativeFolder),
        pathJoin("/etc", loaderRelativeFolder),
        pathJoin(home, pathJoin(".local/share", loaderRelativeFolder)),
        pathJoin("/usr/local/share", loaderRelativeFolder),
        pathJoin("/usr/share", loaderRelativeFolder),
        pathJoin(homebrewPrefix, pathJoin("etc", loaderRelativeFolder)),
        pathJoin(homebrewPrefix, pathJoin("share", loaderRelativeFolder))
    ]

    var loaderSystemFolder = ""
    var loaderSystemPath = ""
    for folder in loaderSystemSearchFolders where FileManager.default.fileExists(atPath: folder) {
        loaderSystemFolder = folder
        let candidate = pathJoin(folder, loaderName)
        if FileManager.default.fileExists(atPath: candidate) {
            loaderSystemPath = candidate
            break
        }
    }

    if loaderSystemFolder.isEmpty {
        print(">>> could not find loader folder " + loaderRelativeFolder)
        return
    }
    if loaderSystemPath.isEmpty {
        print(">>> could not find loader " + loaderName)
        return
    }

    let loaderBundleFolder = pathJoin(resourcesPath(binary), loaderRelativeFolder)
    let loaderBundlePath = pathJoin(loaderBundleFolder, loaderName)
    let libraryRelativeFolder = "../../../Frameworks/"

    if !FileManager.default.fileExists(atPath: loaderBundleFolder) {
        try FileManager.default.createDirectory(atPath: loaderBundleFolder, withIntermediateDirectories: true)
    }

    let loaderData = try Data(contentsOf: URL(fileURLWithPath: loaderSystemPath))
    guard var loaderJson = try JSONSerialization.jsonObject(with: loaderData) as? [String: Any],
          var node = loaderJson[libraryNode] as? [String: Any],
          let libraryPathValue = node["library_path"] as? String
    else {
        print(">>> could not parse loader json " + loaderName)
        return
    }

    let librarySystemPath = pathJoin(loaderSystemFolder, libraryPathValue)
    if !FileManager.default.fileExists(atPath: librarySystemPath) {
        print(">>> could not find loader library " + librarySystemPath)
        return
    }

    print(">>> modifying and writing loader json " + loaderName)
    let loaderLibraryName = basename(librarySystemPath)
    node["library_path"] = pathJoin(libraryRelativeFolder, loaderLibraryName)
    loaderJson[libraryNode] = node
    let outData = try JSONSerialization.data(withJSONObject: loaderJson, options: [.prettyPrinted])
    try outData.write(to: URL(fileURLWithPath: loaderBundlePath))

    print(">>> copying loader library " + loaderLibraryName)
    let frameworkBundleFolder = pathJoin(loaderBundleFolder, libraryRelativeFolder)
    if !FileManager.default.fileExists(atPath: frameworkBundleFolder) {
        try FileManager.default.createDirectory(atPath: frameworkBundleFolder, withIntermediateDirectories: true)
    }
    let libraryTargetPath = pathJoin(frameworkBundleFolder, loaderLibraryName)
    if FileManager.default.fileExists(atPath: libraryTargetPath) {
        try FileManager.default.removeItem(atPath: libraryTargetPath)
    }
    try copyFollowingSymlinks(from: librarySystemPath, to: libraryTargetPath)
}

func removeDevToolsRpaths(_ binary: String) throws {
    for path in try getRpathsDevTools(binary) {
        installNameToolDeleteRpath(path, binary)
    }
}

func dylibUnhell(_ binaryArg: String) throws {
    let binary = absolutePath(binaryArg)
    let ldir = libPath(binary)
    if !FileManager.default.fileExists(atPath: ldir) {
        try FileManager.default.createDirectory(atPath: ldir, withIntermediateDirectories: true)
    }

    print(">> gathering all linked libraries")
    var libs: [String: Set<String>] = [:]
    var libsRel: Set<String> = []
    try gatherLibraries(binary, result: &libs, resultRelative: &libsRel, rpaths: [])

    print(">> copying and processing all linked libraries")
    try processLibraries(libs, libsRel, binary)

    print(">> removing rpath definitions towards dev tools")
    try removeDevToolsRpaths(binary)

    print(">> copying and processing swift libraries")
    try processSwiftLibraries(binary)

    print(">> copying and processing vulkan loader")
    try processVulkanLoader(binary, "MoltenVK_icd.json", "vulkan/icd.d", "ICD")
    if checkVulkanMaxVersion("1.3.261.1") {
        try processVulkanLoader(
            binary, "VkLayer_khronos_synchronization2.json", "vulkan/explicit_layer.d", "layer"
        )
    }
}

// MARK: - osxbundle

func bundlePath(_ binaryName: String) -> String {
    return "\(binaryName).app"
}

func bundleNameOf(_ binaryName: String) -> String {
    return basename(bundlePath(binaryName))
}

func targetPlist(_ binaryName: String) -> String {
    return pathJoin(bundlePath(binaryName), "Contents/Info.plist")
}

func targetDirectory(_ binaryName: String) -> String {
    return pathJoin(bundlePath(binaryName), "Contents/MacOS")
}

func targetBinary(_ binaryName: String) -> String {
    return pathJoin(targetDirectory(binaryName), basename(binaryName))
}

func copyBundle(_ binaryName: String, _ srcPath: String) throws {
    let dst = bundlePath(binaryName)
    var isDir: ObjCBool = false
    if FileManager.default.fileExists(atPath: dst, isDirectory: &isDir), isDir.boolValue {
        try FileManager.default.removeItem(atPath: dst)
    }
    let src = pathJoin(pathJoin(srcPath, "TOOLS/osxbundle"), bundleNameOf(binaryName))
    try FileManager.default.copyItem(atPath: src, toPath: dst)
}

func copyBinary(_ binaryName: String) throws {
    let dst = targetBinary(binaryName)
    if FileManager.default.fileExists(atPath: dst) {
        try FileManager.default.removeItem(atPath: dst)
    }
    try FileManager.default.copyItem(atPath: binaryName, toPath: dst)
}

func applyPlistTemplate(_ plistFile: String, _ version: String, _ category: String) throws {
    print(">> setting bundle category to " + category)
    let content = try String(contentsOfFile: plistFile, encoding: .utf8)
    let newContent = content
        .replacingOccurrences(of: "${VERSION}", with: version)
        .replacingOccurrences(of: "${CATEGORY}", with: category)
    try newContent.write(toFile: plistFile, atomically: true, encoding: .utf8)
}

func signBundle(_ binaryName: String) throws {
    for signDir in ["Contents/Frameworks", "Contents/MacOS"] {
        let resolvedDir = pathJoin(bundlePath(binaryName), signDir)
        guard let enumerator = FileManager.default.enumerator(atPath: resolvedDir) else { continue }
        for case let relPath as String in enumerator {
            let fullPath = pathJoin(resolvedDir, relPath)
            var isDirectory: ObjCBool = false
            if FileManager.default.fileExists(atPath: fullPath, isDirectory: &isDirectory), !isDirectory.boolValue {
                call("codesign", ["--force", "-s", "-", fullPath])
            }
        }
    }
    call("codesign", ["--force", "-s", "-", bundlePath(binaryName)])
}

func bundleVersion(_ buildPath: String) -> String {
    let versionHPath = pathJoin(buildPath, "common/version.h")
    guard let content = try? String(contentsOfFile: versionHPath, encoding: .utf8) else {
        return "UNKNOWN"
    }
    guard let regex = try? NSRegularExpression(pattern: "#define\\s+VERSION\\s+\"v(.+)\""),
          let match = regex.firstMatch(in: content, range: NSRange(content.startIndex..., in: content)),
          let range = Range(match.range(at: 1), in: content)
    else {
        return "UNKNOWN"
    }
    return String(content[range])
}

// MARK: - CLI

func printUsageAndExit(_ message: String) -> Never {
    let usage = "usage: macos-bundle.swift [-s|--skip-deps] [-c|--category video|games] binary [src_path]\n"
    FileHandle.standardError.write(Data(usage.utf8))
    if !message.isEmpty {
        FileHandle.standardError.write(Data((message + "\n").utf8))
    }
    exit(2)
}

var deps = true
var category = "video"
var positional: [String] = []

let args = Array(CommandLine.arguments.dropFirst())
var i = 0
while i < args.count {
    let arg = args[i]
    switch arg {
    case "-s", "--skip-deps":
        deps = false
    case "-c", "--category":
        i += 1
        guard i < args.count else { printUsageAndExit("option \(arg) requires an argument") }
        category = args[i]
    default:
        if arg.hasPrefix("--category=") {
            category = String(arg.dropFirst("--category=".count))
        } else if arg.hasPrefix("-") && arg != "-" {
            printUsageAndExit("unknown option: \(arg)")
        } else {
            positional.append(arg)
        }
    }
    i += 1
}

guard ["video", "games"].contains(category) else {
    printUsageAndExit("invalid category: \(category) (choose from 'video', 'games')")
}

guard positional.count == 1 || positional.count == 2 else {
    printUsageAndExit("incorrect number of arguments")
}

let binaryName = positional[0]
let buildPath = dirname(binaryName)
let srcPath = positional.count > 1 ? positional[1] : "."

do {
    let version = bundleVersion(buildPath).trimmingCharacters(in: .whitespacesAndNewlines)

    print("Creating macOS application bundle (version: \(version))...")
    print("> copying bundle skeleton")
    try copyBundle(binaryName, srcPath)
    print("> copying binary")
    try copyBinary(binaryName)
    print("> generating Info.plist")
    try applyPlistTemplate(targetPlist(binaryName), version, category)

    if deps {
        print("> bundling dependencies")
        try dylibUnhell(targetBinary(binaryName))
    }

    print("> signing bundle with ad-hoc pseudo identity")
    try signBundle(binaryName)

    print("done.")
} catch {
    FileHandle.standardError.write(Data("error: \(error)\n".utf8))
    exit(1)
}
