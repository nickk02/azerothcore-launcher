import Foundation

/// Settings file shared in format with the Windows app: key=value lines.
struct Config {
    var wowPath = ""
    var realmAddress = ""
    var rememberLogin = false

    static var url: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        let dir = base.appendingPathComponent("AzerothCore", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("settings.ini")
    }

    static func load() -> Config {
        var cfg = Config()
        guard let text = try? String(contentsOf: url, encoding: .utf8) else { return cfg }
        for line in text.split(separator: "\n", omittingEmptySubsequences: true) {
            guard let eq = line.firstIndex(of: "=") else { continue }
            let key = String(line[..<eq])
            let value = String(line[line.index(after: eq)...])
            switch key {
            case "WowPath": cfg.wowPath = value
            case "RealmAddress": cfg.realmAddress = value
            case "CredentialVaultEnabled": cfg.rememberLogin = (value == "1")
            default: break
            }
        }
        return cfg
    }

    func save() {
        let text = "WowPath=\(wowPath)\nRealmAddress=\(realmAddress)\nCredentialVaultEnabled=\(rememberLogin ? "1" : "0")\n"
        try? text.write(to: Config.url, atomically: true, encoding: .utf8)
    }
}
