import AVFoundation
import SwiftUI

struct ContentView: View {
    @State private var host = ""
    @State private var port = "28887"
    @State private var pairing = "MAH1"
    @State private var status = "Idle — same Wi‑Fi as Mac/PC, or USB tether"
    @State private var streaming = false
    @StateObject private var holder = StreamerHolder()

    var body: some View {
        ZStack {
            Color(red: 0.04, green: 0.05, blue: 0.08).ignoresSafeArea()
            VStack(alignment: .leading, spacing: 14) {
                Text("MAH CAM")
                    .font(.system(size: 28, weight: .bold, design: .rounded))
                    .foregroundStyle(Color(red: 0.90, green: 0.72, blue: 0.46))
                    .tracking(2)
                Text("iPhone camera → MAH Studio")
                    .font(.system(size: 13))
                    .foregroundStyle(Color(red: 0.58, green: 0.64, blue: 0.72))

                TextField("Desktop IP", text: $host)
                    .textFieldStyle(.roundedBorder)
                    .keyboardType(.decimalPad)
                HStack {
                    TextField("Port", text: $port)
                        .textFieldStyle(.roundedBorder)
                        .keyboardType(.numberPad)
                    TextField("Pairing", text: $pairing)
                        .textFieldStyle(.roundedBorder)
                        .textInputAutocapitalization(.characters)
                }

                Button("Find MAH Studio on LAN") {
                    status = "Searching…"
                    DispatchQueue.global().async {
                        if let h = MahProtocol.discover() {
                            DispatchQueue.main.async {
                                host = h.ip
                                port = String(h.port)
                                pairing = h.pairing
                                status = "Found \(h.ip):\(h.port)  code \(h.pairing)"
                            }
                        } else {
                            DispatchQueue.main.async {
                                status = "No desktop found — enter IP manually"
                            }
                        }
                    }
                }
                .buttonStyle(.bordered)

                Button(streaming ? "Stop" : "Start camera stream") {
                    toggle()
                }
                .buttonStyle(.borderedProminent)
                .tint(Color(red: 0.90, green: 0.72, blue: 0.46))

                Text(status)
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundStyle(Color(red: 0.58, green: 0.64, blue: 0.72))

                Spacer()
            }
            .padding(20)
        }
        .onAppear {
            holder.streamer.onStatus = { status = $0 }
        }
    }

    private func toggle() {
        if streaming {
            holder.streamer.stop()
            streaming = false
            return
        }
        guard !host.isEmpty, let p = Int(port) else {
            status = "Enter desktop IP / port"
            return
        }
        AVCapturePermission.request {
            holder.streamer.start(host: host, port: p, pairing: pairing.isEmpty ? "MAH1" : pairing)
            streaming = true
            status = "Starting…"
        }
    }
}

final class StreamerHolder: ObservableObject {
    let streamer = CameraStreamer()
}

enum AVCapturePermission {
    static func request(then: @escaping () -> Void) {
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            then()
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .video) { ok in
                if ok { DispatchQueue.main.async { then() } }
            }
        default:
            break
        }
    }
}
