import AppKit
import Observation

enum Status {
    case unconfigured, checking, online, unreachable

    var text: String {
        switch self {
        case .unconfigured: "No realm set"
        case .checking: "Checking…"
        case .online: "Online"
        case .unreachable: "Unreachable"
        }
    }
    var color: NSColor {
        switch self {
        case .online: .systemGreen
        case .unreachable: .systemRed
        default: .secondaryLabelColor
        }
    }
}

/// The project's own realm, offered as a preset in the realm menu.
let presetRealm = "wss://play.azerothcore.net"

@MainActor
@Observable
final class LauncherModel {
    var wowPath = ""
    var realm = ""
    /// True when the realm menu is on "Custom" and the address is typed by hand.
    var customRealm = false
    var account = ""
    var password = ""
    var remember = false
    var status: Status = .unconfigured
    var error: String?
    var busy: String?
    var wine = WineStatus(engine: nil, prefixReady: false, help: "")
    var update: UpdateStatus?
    var installing = false
    var upToDateNotice = false

    @ObservationIgnored private let launcher = Launcher()

    var version: String { currentVersion() }
    var canPlay: Bool { busy == nil && !wowPath.isEmpty && !realm.trimmingCharacters(in: .whitespaces).isEmpty && wine.engine != nil }

    func start() {
        let s = loadSettings()
        wowPath = s.wowPath
        let saved = s.realmAddress.trimmingCharacters(in: .whitespaces)
        customRealm = !saved.isEmpty && saved != presetRealm
        realm = saved.isEmpty ? presetRealm : s.realmAddress
        remember = s.rememberLogin
        // Only the account name is filled in. The password never goes into a visible control.
        if remember, let login = savedLogin() { account = login.account }
        refreshWine()
        Task { await checkForUpdate(manual: false) }
    }

    /// Called when the realm menu changes. Matches the Windows launcher: switching to Custom clears the preset, switching back restores it.
    func realmMenuChanged() {
        if customRealm {
            if realm == presetRealm { realm = "" }
        } else {
            realm = presetRealm
        }
        save()
    }

    func refreshWine() {
        Task {
            let w = await Task.detached { wineStatus() }.value
            wine = w
        }
    }

    func save() {
        do {
            try saveSettings(settings: Settings(wowPath: wowPath, realmAddress: realm.trimmingCharacters(in: .whitespaces), rememberLogin: remember))
        } catch { self.error = describe(error) }
    }

    func choose(_ url: URL) {
        if isValidWowExe(path: url.path) {
            wowPath = url.path
            error = nil
            save()
        } else {
            error = "That isn't a 3.3.5a Wow.exe. Pick the Wow.exe next to the Data folder of your client."
        }
    }

    func checkRealm() async {
        let address = realm.trimmingCharacters(in: .whitespaces)
        if address.isEmpty { status = .unconfigured; return }
        status = .checking
        let result = await Task.detached { AzerothCore.checkRealm(address: address) }.value
        guard address == realm.trimmingCharacters(in: .whitespaces) else { return }
        switch result {
        case .online: status = .online
        case .unreachable: status = .unreachable
        case .unconfigured: status = .unconfigured
        }
    }

    func play() {
        error = nil
        if remember, !account.isEmpty, !password.isEmpty { try? storeLogin(account: account, password: password) }
        if !remember { clearLogin() }
        save()
        let (path, address, launcher) = (wowPath, realm.trimmingCharacters(in: .whitespaces), self.launcher)
        busy = wine.prefixReady ? "Starting WoW…" : "Setting up Wine. The first run takes about a minute."
        Task {
            let result: Result<Void, Error> = await Task.detached {
                Result { try launcher.play(wowPath: path, realmAddress: address) }
            }.value
            busy = nil
            refreshWine()
            if case .failure(let e) = result { error = describe(e) }
        }
    }

    func checkForUpdate(manual: Bool) async {
        let info = await Task.detached { checkUpdate() }.value
        update = info.available ? info : nil
        if manual && !info.available { upToDateNotice = true }
    }

    func installUpdate() {
        guard let info = update else { return }
        installing = true
        Task {
            let result: Result<Void, Error> = await Task.detached { Result { try AzerothCore.installUpdate(info: info) } }.value
            installing = false
            if case .failure(let e) = result { error = "The update failed to verify or install. \(describe(e))" }
        }
    }

    private func describe(_ e: Error) -> String {
        if case LauncherError.Message(let text) = e { return text }
        return "\(e)"
    }
}
