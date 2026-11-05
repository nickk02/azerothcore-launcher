import AppKit
import CryptoKit
import Foundation

struct UpdateInfo {
    var latest = ""
    var installerURL: URL?
    var checksumURL: URL?
    var available = false
}

enum Updater {
    private static let api = URL(string: "https://api.github.com/repos/nickk02/azerothcore-launcher/releases/latest")!
    private static let installerName = "AzerothCore.dmg"
    private static let checksumName = "AzerothCore.dmg.sha256"

    static var currentVersion: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.0.0"
    }

    /// Numeric compare. A missing component is 0. Any non-integer part gives false.
    static func isNewer(_ candidate: String, than current: String) -> Bool {
        func parts(_ text: String) -> [Int]? {
            let pieces = text.split(separator: ".", omittingEmptySubsequences: false)
            if pieces.isEmpty { return nil }
            var numbers: [Int] = []
            for piece in pieces {
                guard !piece.isEmpty, piece.allSatisfy({ $0.isASCII && $0.isNumber }), let n = Int(piece)
                else { return nil }
                numbers.append(n)
            }
            return numbers
        }
        guard let a = parts(candidate), let b = parts(current) else { return false }
        for i in 0..<max(a.count, b.count) {
            let left = i < a.count ? a[i] : 0
            let right = i < b.count ? b[i] : 0
            if left != right { return left > right }
        }
        return false
    }

    static func check() async -> UpdateInfo {
        var info = UpdateInfo()
        struct Release: Decodable {
            struct Asset: Decodable { let name: String; let browser_download_url: URL }
            let tag_name: String
            let assets: [Asset]
        }
        var request = URLRequest(url: api)
        request.setValue("azerothcore-launcher", forHTTPHeaderField: "User-Agent")
        guard let (data, _) = try? await URLSession.shared.data(for: request),
              let release = try? JSONDecoder().decode(Release.self, from: data)
        else { return info }

        var tag = release.tag_name
        if tag.hasPrefix("v") || tag.hasPrefix("V") { tag.removeFirst() }
        info.latest = tag
        info.installerURL = release.assets.first { $0.name == installerName }?.browser_download_url
        info.checksumURL = release.assets.first { $0.name == checksumName }?.browser_download_url
        // Both assets are required, so an update is never installed unverified.
        guard info.installerURL != nil, info.checksumURL != nil else { return info }
        info.available = isNewer(tag, than: currentVersion)
        return info
    }

    /// Downloads the DMG, checks its SHA-256, and opens it. Returns an error message on failure.
    static func install(_ info: UpdateInfo) async -> String? {
        guard let installerURL = info.installerURL, let checksumURL = info.checksumURL else {
            return "The release has no installer."
        }
        do {
            let (sumData, _) = try await URLSession.shared.data(from: checksumURL)
            let expected = (String(data: sumData, encoding: .utf8) ?? "")
                .split(whereSeparator: { $0.isWhitespace }).first.map { $0.lowercased() } ?? ""
            guard expected.count == 64 else { return "The checksum file is not valid." }

            let (dmg, _) = try await URLSession.shared.data(from: installerURL)
            let actual = SHA256.hash(data: dmg).map { String(format: "%02x", $0) }.joined()
            guard actual == expected else { return "The download did not match its checksum." }

            // Written only after the hash matches.
            let target = FileManager.default.temporaryDirectory
                .appendingPathComponent("AzerothCore-\(info.latest).dmg")
            try? FileManager.default.removeItem(at: target)
            try dmg.write(to: target)
            NSWorkspace.shared.open(target)
            return nil
        } catch {
            return "Update failed: \(error.localizedDescription)"
        }
    }
}
