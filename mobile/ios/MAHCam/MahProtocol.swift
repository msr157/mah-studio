import Foundation

/// MAH1 client — see mah/PROTOCOL.md
/// Media: [UInt8 "V"|"A"][UInt32 BE length][payload]
final class MahProtocol {
    static let discoverPort: UInt16 = 28888

    struct HostInfo {
        let ip: String
        let port: Int
        let pairing: String
    }

    private var input: InputStream?
    private var output: OutputStream?
    private let host: String
    private let port: Int
    private let pairing: String
    private let width: Int
    private let height: Int
    private let withAudio: Bool

    init(host: String, port: Int, pairing: String, width: Int = 1280, height: Int = 720, withAudio: Bool = true) {
        self.host = host
        self.port = port
        self.pairing = pairing
        self.width = width
        self.height = height
        self.withAudio = withAudio
    }

    func connect() throws {
        var readStream: Unmanaged<CFReadStream>?
        var writeStream: Unmanaged<CFWriteStream>?
        CFStreamCreatePairWithSocketToHost(nil, host as CFString, UInt32(port), &readStream, &writeStream)
        guard let r = readStream?.takeRetainedValue(), let w = writeStream?.takeRetainedValue() else {
            throw NSError(domain: "MAH", code: 1, userInfo: [NSLocalizedDescriptionKey: "Stream open failed"])
        }
        let input = r as InputStream
        let output = w as OutputStream
        input.open()
        output.open()

        let codec = withAudio ? "mjpeg+pcm" : "mjpeg"
        let hello = "MAH1 \(pairing) \(width)x\(height) \(codec)\n"
        guard let data = hello.data(using: .utf8) else { throw NSError(domain: "MAH", code: 2) }
        _ = data.withUnsafeBytes { output.write($0.bindMemory(to: UInt8.self).baseAddress!, maxLength: data.count) }

        guard let reply = readLine(from: input), reply.hasPrefix("MAH1 OK") else {
            input.close()
            output.close()
            throw NSError(domain: "MAH", code: 3, userInfo: [NSLocalizedDescriptionKey: "Handshake failed"])
        }

        self.input = input
        self.output = output
    }

    func sendJPEG(_ jpeg: Data) throws { try sendTyped(0x56 /* V */, jpeg) }
    func sendPCM(_ pcm: Data) throws { try sendTyped(0x41 /* A */, pcm) }

    private func sendTyped(_ type: UInt8, _ payload: Data) throws {
        guard let output else { throw NSError(domain: "MAH", code: 4) }
        var t = type
        _ = withUnsafeBytes(of: &t) { output.write($0.bindMemory(to: UInt8.self).baseAddress!, maxLength: 1) }
        var len = UInt32(payload.count).bigEndian
        _ = withUnsafeBytes(of: &len) { output.write($0.bindMemory(to: UInt8.self).baseAddress!, maxLength: 4) }
        _ = payload.withUnsafeBytes { output.write($0.bindMemory(to: UInt8.self).baseAddress!, maxLength: payload.count) }
    }

    func close() {
        input?.close()
        output?.close()
        input = nil
        output = nil
    }

    static func discover(timeout: TimeInterval = 2.5) -> HostInfo? {
        let sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard sock >= 0 else { return nil }
        defer { Darwin.close(sock) }

        var broadcast: Int32 = 1
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, socklen_t(MemoryLayout.size(ofValue: broadcast)))

        var tv = timeval(tv_sec: Int(timeout), tv_usec: 0)
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, socklen_t(MemoryLayout.size(ofValue: tv)))

        var addr = sockaddr_in()
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = discoverPort.bigEndian
        addr.sin_addr.s_addr = inet_addr("255.255.255.255")

        let payload = Array("MAH1 DISCOVER\n".utf8)
        _ = payload.withUnsafeBufferPointer { buf in
            withUnsafePointer(to: &addr) {
                $0.withMemoryRebound(to: sockaddr.self, capacity: 1) { sa in
                    sendto(sock, buf.baseAddress, buf.count, 0, sa, socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }

        var buf = [UInt8](repeating: 0, count: 256)
        let n = recv(sock, &buf, buf.count, 0)
        guard n > 0 else { return nil }
        let text = String(bytes: buf.prefix(Int(n)), encoding: .utf8)?
            .trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let parts = text.split(separator: " ")
        guard parts.count >= 5, parts[0] == "MAH1", parts[1] == "HOST",
              let port = Int(parts[3]) else { return nil }
        return HostInfo(ip: String(parts[2]), port: port, pairing: String(parts[4]))
    }

    private func readLine(from input: InputStream, max: Int = 256) -> String? {
        var chars: [UInt8] = []
        var byte: UInt8 = 0
        while chars.count < max {
            let n = input.read(&byte, maxLength: 1)
            if n <= 0 { return chars.isEmpty ? nil : String(bytes: chars, encoding: .utf8) }
            if byte == UInt8(ascii: "\n") { break }
            if byte != UInt8(ascii: "\r") { chars.append(byte) }
        }
        return String(bytes: chars, encoding: .utf8)
    }
}
