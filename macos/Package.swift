// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "AzerothCore",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(name: "AzerothCore"),
        .testTarget(name: "AzerothCoreTests", dependencies: ["AzerothCore"]),
    ]
)
