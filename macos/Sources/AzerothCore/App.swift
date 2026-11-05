import SwiftUI

@main
struct AzerothCoreApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .frame(width: 420)
        }
        .windowResizability(.contentSize)
    }
}
