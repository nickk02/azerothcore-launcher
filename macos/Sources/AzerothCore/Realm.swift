import Foundation
import Network

enum Reachability { case unconfigured, online, unreachable }

enum Realm {
    /// "host" or "host:port". The port defaults to 3724.
    static func parse(_ address: String) -> (host: String, port: UInt16) {
        guard let colon = address.firstIndex(of: ":") else { return (address, 3724) }
        let host = String(address[..<colon])
        let port = UInt16(address[address.index(after: colon)...]) ?? 3724
        return (host, port)
    }

    /// Opens a TCP connection and closes it. No data is sent.
    static func check(_ address: String) async -> Reachability {
        let text = address.trimmingCharacters(in: .whitespaces)
        if text.isEmpty { return .unconfigured }
        let (host, port) = parse(text)
        guard !host.isEmpty, let nwPort = NWEndpoint.Port(rawValue: port) else { return .unreachable }

        return await withCheckedContinuation { continuation in
            let connection = NWConnection(host: NWEndpoint.Host(host), port: nwPort, using: .tcp)
            let lock = NSLock()
            var finished = false

            func finish(_ result: Reachability) {
                lock.lock()
                defer { lock.unlock() }
                if finished { return }
                finished = true
                connection.cancel()
                continuation.resume(returning: result)
            }

            connection.stateUpdateHandler = { state in
                switch state {
                case .ready: finish(.online)
                case .failed, .waiting: finish(.unreachable)
                default: break
                }
            }
            connection.start(queue: .global())
            DispatchQueue.global().asyncAfter(deadline: .now() + 5) { finish(.unreachable) }
        }
    }
}
