import AppKit
import SwiftUI

struct ContentView: View {
    @State private var wowPath = ""
    @State private var realm = ""
    @State private var account = ""
    @State private var password = ""
    @State private var remember = false
    @State private var reachability = Reachability.unconfigured
    @State private var message = ""
    @State private var update = UpdateInfo()
    @State private var loaded = false

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("AzerothCore").font(.largeTitle.bold())

            if update.available {
                HStack {
                    Text("Version \(update.latest) is available.")
                    Spacer()
                    Button("Update") { Task { message = await Updater.install(update) ?? "" } }
                }
                .padding(8)
                .background(.quaternary, in: RoundedRectangle(cornerRadius: 6))
            }

            Form {
                HStack {
                    TextField("WoW client", text: $wowPath, prompt: Text("Path to Wow.exe"))
                    Button("Browse…", action: browse)
                }
                TextField("Realm address", text: $realm, prompt: Text("logon.example.com"))
                LabeledContent("Status") { Text(statusText).foregroundStyle(statusColor) }
                TextField("Account", text: $account)
                SecureField("Password", text: $password)
                Toggle("Remember me", isOn: $remember)
            }
            .formStyle(.grouped)

            HStack {
                Text(message).foregroundStyle(.red).lineLimit(3)
                Spacer()
                Button("Play", action: play)
                    .keyboardShortcut(.defaultAction)
                    .controlSize(.large)
            }

            Text("Version \(Updater.currentVersion)").font(.caption).foregroundStyle(.secondary)
        }
        .padding(20)
        .onAppear(perform: load)
        .task(id: realm) {
            guard loaded else { return }
            try? await Task.sleep(nanoseconds: 500_000_000)   // wait until typing stops
            if Task.isCancelled { return }
            reachability = await Realm.check(realm)
        }
        .task { update = await Updater.check() }
    }

    private var statusText: String {
        switch reachability {
        case .unconfigured: return "No realm configured"
        case .online: return "Online"
        case .unreachable: return "Unreachable"
        }
    }

    private var statusColor: Color {
        switch reachability {
        case .online: return .green
        case .unreachable: return .red
        case .unconfigured: return .secondary
        }
    }

    private func load() {
        let cfg = Config.load()
        wowPath = cfg.wowPath
        realm = cfg.realmAddress
        remember = cfg.rememberLogin
        if cfg.rememberLogin, let saved = Keychain.get() {
            account = saved.account
            password = saved.password
        }
        loaded = true
    }

    private func browse() {
        let panel = NSOpenPanel()
        panel.message = "Choose Wow.exe from your 3.3.5a client folder"
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        if panel.runModal() == .OK, let url = panel.url { wowPath = url.path }
    }

    private func play() {
        message = ""
        var cfg = Config()
        cfg.wowPath = wowPath
        cfg.realmAddress = realm.trimmingCharacters(in: .whitespaces)
        cfg.rememberLogin = remember
        cfg.save()

        if remember && !account.isEmpty {
            Keychain.store(account: account, password: password)
        } else {
            Keychain.clear()
        }

        do {
            try Play.launch(wowPath: cfg.wowPath, realmAddress: cfg.realmAddress)
        } catch {
            message = error.localizedDescription
        }
    }
}
