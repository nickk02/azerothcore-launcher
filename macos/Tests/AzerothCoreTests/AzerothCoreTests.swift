import XCTest
@testable import AzerothCore

final class AzerothCoreTests: XCTestCase {
    func testParseAddress() {
        XCTAssertEqual(Realm.parse("logon.example.com:3724").host, "logon.example.com")
        XCTAssertEqual(Realm.parse("logon.example.com").port, 3724)
        XCTAssertEqual(Realm.parse("192.168.1.100:8085").port, 8085)
        XCTAssertEqual(Realm.parse("host:abc").port, 3724)
        XCTAssertEqual(Realm.parse("host:").port, 3724)
    }

    func testIsNewer() {
        XCTAssertTrue(Updater.isNewer("2026.08.07", than: "2026.08.06"))
        XCTAssertFalse(Updater.isNewer("2026.08.06", than: "2026.08.06"))
        XCTAssertTrue(Updater.isNewer("2026.8.10", than: "2026.8.9"))
        XCTAssertFalse(Updater.isNewer("2026.8", than: "2026.8.0"))
        XCTAssertTrue(Updater.isNewer("2026.8.1", than: "2026.8"))
        XCTAssertFalse(Updater.isNewer("2026.08.06", than: "0.0.0-dev"))
        XCTAssertFalse(Updater.isNewer("", than: "2026.08.06"))
        XCTAssertFalse(Updater.isNewer("v2026.08.07", than: "2026.08.06"))
        XCTAssertTrue(Updater.isNewer("2026.08.06", than: "0.0.0"))
    }

    func testRealmlistWriting() throws {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: dir.appendingPathComponent("Data/enUS"), withIntermediateDirectories: true)
        Play.writeRealmlist(wowFolder: dir, address: "logon.example.com")
        let main = try String(contentsOf: dir.appendingPathComponent("WTF/realmlist.wtf"), encoding: .utf8)
        let locale = try String(contentsOf: dir.appendingPathComponent("Data/enUS/realmlist.wtf"), encoding: .utf8)
        XCTAssertEqual(main, "set realmlist logon.example.com\n")
        XCTAssertEqual(locale, main)
    }
}
