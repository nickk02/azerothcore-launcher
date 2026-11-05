import Foundation

/// Writes the realmlist and starts the Windows 3.3.5a client through Wine.
enum Play {
    struct Failure: LocalizedError {
        let errorDescription: String?
    }

    static func isValidWowExe(_ path: String) -> Bool {
        var isDirectory: ObjCBool = false
        guard FileManager.default.fileExists(atPath: path, isDirectory: &isDirectory), !isDirectory.boolValue
        else { return false }
        return URL(fileURLWithPath: path).lastPathComponent.lowercased() == "wow.exe"
    }

    static func writeRealmlist(wowFolder: URL, address: String) {
        let line = "set realmlist \(address)\n"
        let fm = FileManager.default

        let wtf = wowFolder.appendingPathComponent("WTF", isDirectory: true)
        try? fm.createDirectory(at: wtf, withIntermediateDirectories: true)
        try? line.write(to: wtf.appendingPathComponent("realmlist.wtf"), atomically: true, encoding: .utf8)

        let data = wowFolder.appendingPathComponent("Data", isDirectory: true)
        let entries = (try? fm.contentsOfDirectory(at: data, includingPropertiesForKeys: [.isDirectoryKey])) ?? []
        for entry in entries where (try? entry.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true {
            try? line.write(to: entry.appendingPathComponent("realmlist.wtf"), atomically: true, encoding: .utf8)
        }
    }

    static func findWine() -> URL? {
        let candidates = [
            "/opt/homebrew/bin/wine64", "/opt/homebrew/bin/wine",
            "/usr/local/bin/wine64", "/usr/local/bin/wine",
            "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine",
        ]
        return candidates.first { FileManager.default.isExecutableFile(atPath: $0) }
            .map { URL(fileURLWithPath: $0) }
    }

    static func launch(wowPath: String, realmAddress: String) throws {
        guard isValidWowExe(wowPath) else {
            throw Failure(errorDescription: "That isn't a 3.3.5a Wow.exe.")
        }
        let exe = URL(fileURLWithPath: wowPath)
        let folder = exe.deletingLastPathComponent()

        if !realmAddress.isEmpty {
            writeRealmlist(wowFolder: folder, address: realmAddress)
        }

        guard let wine = findWine() else {
            throw Failure(errorDescription: "Wine was not found. Install it (for example with Homebrew) or CrossOver, then try again.")
        }
        let process = Process()
        process.executableURL = wine
        process.arguments = [exe.lastPathComponent]
        process.currentDirectoryURL = folder
        do {
            try process.run()
        } catch {
            throw Failure(errorDescription: "Failed to launch WoW: \(error.localizedDescription)")
        }
    }
}
