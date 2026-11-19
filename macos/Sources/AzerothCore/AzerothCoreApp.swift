import SwiftUI
import UniformTypeIdentifiers

@main
struct AzerothCoreApp: App {
    @State private var model = LauncherModel()
    @State private var choosing = false

    var body: some Scene {
        Window("AzerothCore", id: "main") {
            ContentView(choosing: $choosing)
                .environment(model)
                .task { model.start() }
        }
        .windowResizability(.contentSize)
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("Choose Wow.exe…") { choosing = true }.keyboardShortcut("o")
            }
            CommandGroup(after: .appInfo) {
                Button("Check for Updates…") { Task { await model.checkForUpdate(manual: true) } }
            }
            CommandGroup(replacing: .help) {
                Button("Open Art Folder") {
                    let dir = URL(fileURLWithPath: artDir())
                    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
                    NSWorkspace.shared.open(dir)
                }
            }
        }
    }
}
