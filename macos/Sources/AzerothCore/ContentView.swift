import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @Environment(LauncherModel.self) private var model
    @Binding var choosing: Bool
    private let hero = Art.image("hero", maxPixel: 1600)
    private let logo = Art.image("logo", maxPixel: 600)

    var body: some View {
        @Bindable var model = model
        HStack(spacing: 0) {
            artPanel
            Form {
                if let update = model.update {
                    Section {
                        LabeledContent("Version \(update.latest) is available") {
                            Button(model.installing ? "Updating…" : "Update") { model.installUpdate() }.disabled(model.installing)
                        }
                    }
                }
                Section("Realm") {
                    TextField("Address", text: $model.realm, prompt: Text("logon.example.com or wss://play.example.com"))
                        .autocorrectionDisabled()
                    LabeledContent("Status") {
                        Label(model.status.text, systemImage: "circle.fill")
                            .foregroundStyle(Color(nsColor: model.status.color))
                    }
                }
                Section("Account") {
                    TextField("Name", text: $model.account).autocorrectionDisabled()
                    SecureField("Password", text: $model.password)
                    Toggle("Remember me", isOn: $model.remember)
                }
                Section("Game") {
                    LabeledContent("Wow.exe") {
                        HStack {
                            Text(model.wowPath.isEmpty ? "Not set" : model.wowPath)
                                .lineLimit(1).truncationMode(.head).foregroundStyle(.secondary)
                            Button("Browse…") { choosing = true }
                        }
                    }
                    if let engine = model.wine.engine {
                        LabeledContent("Runs with", value: engine)
                    } else {
                        Label(model.wine.help, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.orange)
                    }
                }
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Button { model.play() } label: { Text("Play").frame(maxWidth: .infinity) }
                            .buttonStyle(.borderedProminent).controlSize(.large)
                            .keyboardShortcut(.defaultAction).disabled(!model.canPlay)
                        if let busy = model.busy {
                            HStack(spacing: 8) { ProgressView().controlSize(.small); Text(busy).font(.callout).foregroundStyle(.secondary) }
                        }
                        if let error = model.error {
                            Text(error).font(.callout).foregroundStyle(.red).textSelection(.enabled)
                        }
                    }
                }
            }
            .formStyle(.grouped)
            .frame(width: 440)
        }
        .frame(height: 560)
        .fileImporter(isPresented: $choosing, allowedContentTypes: [UTType(filenameExtension: "exe") ?? .data]) { result in
            if case .success(let url) = result { model.choose(url) }
        }
        .alert("AzerothCore is up to date", isPresented: $model.upToDateNotice) { Button("OK") {} } message: { Text("Version \(model.version)") }
        .task(id: model.realm) {
            try? await Task.sleep(for: .milliseconds(400))
            if !Task.isCancelled { await model.checkRealm() }
        }
    }

    private var artPanel: some View {
        ZStack(alignment: .bottom) {
            if let hero {
                Image(nsImage: hero).resizable().scaledToFill().frame(width: 360, height: 560).clipped()
            } else {
                Rectangle().fill(.quaternary).frame(width: 360, height: 560)
            }
            LinearGradient(colors: [.clear, .black.opacity(0.55)], startPoint: .center, endPoint: .bottom)
            VStack(spacing: 6) {
                if let logo { Image(nsImage: logo).resizable().scaledToFit().frame(height: 72) }
                Text("Version \(model.version)").font(.caption).foregroundStyle(.white.opacity(0.75))
            }.padding(.bottom, 20)
        }
        .frame(width: 360, height: 560)
    }
}
