// swift-tools-version: 5.10
import PackageDescription

// The Rust core is linked as a static library that scripts/build-macos.sh builds into
// macos/build/lib. The UniFFI C header and Swift glue are generated there too.
let package = Package(
    name: "AzerothCore",
    platforms: [.macOS(.v14)],
    targets: [
        .target(name: "launcher_coreFFI", path: "Sources/LauncherCoreFFI", publicHeadersPath: "include"),
        .executableTarget(
            name: "AzerothCore",
            dependencies: ["launcher_coreFFI"],
            path: "Sources/AzerothCore",
            linkerSettings: [
                .unsafeFlags(["-L", "build/lib"]),
                .linkedLibrary("launcher_core"),
                .linkedFramework("Security"),
                .linkedFramework("SystemConfiguration"),
                .linkedFramework("CoreFoundation"),
            ]
        ),
    ]
)
