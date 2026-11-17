import AVFoundation
import UIKit

final class CameraStreamer: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    private let session = AVCaptureSession()
    private let queue = DispatchQueue(label: "mah.cam")
    private var protocolClient: MahProtocol?
    private var lastSent = CFAbsoluteTimeGetCurrent()
    private let targetFPS: Double = 15
    var onStatus: ((String) -> Void)?

    func start(host: String, port: Int, pairing: String) {
        queue.async { [weak self] in
            guard let self else { return }
            do {
                let client = MahProtocol(host: host, port: port, pairing: pairing)
                try client.connect()
                self.protocolClient = client
                DispatchQueue.main.async { self.onStatus?("Connected — streaming MJPEG") }
                self.configureSession()
                self.session.startRunning()
            } catch {
                DispatchQueue.main.async { self.onStatus?("Error: \(error.localizedDescription)") }
            }
        }
    }

    func stop() {
        queue.async { [weak self] in
            self?.session.stopRunning()
            self?.protocolClient?.close()
            self?.protocolClient = nil
            DispatchQueue.main.async { self?.onStatus?("Stopped") }
        }
    }

    private func configureSession() {
        session.beginConfiguration()
        session.sessionPreset = .hd1280x720

        guard let device = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .front),
              let input = try? AVCaptureDeviceInput(device: device),
              session.canAddInput(input) else { return }
        session.addInput(input)

        let output = AVCaptureVideoDataOutput()
        output.alwaysDiscardsLateVideoFrames = true
        output.setSampleBufferDelegate(self, queue: queue)
        guard session.canAddOutput(output) else { return }
        session.addOutput(output)
        session.commitConfiguration()
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        let now = CFAbsoluteTimeGetCurrent()
        if now - lastSent < 1.0 / targetFPS { return }
        lastSent = now

        guard let imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        let ci = CIImage(cvPixelBuffer: imageBuffer)
        let context = CIContext()
        guard let cg = context.createCGImage(ci, from: ci.extent) else { return }
        let ui = UIImage(cgImage: cg)
        guard let jpeg = ui.jpegData(compressionQuality: 0.7) else { return }

        do {
            try protocolClient?.sendJPEG(jpeg)
        } catch {
            DispatchQueue.main.async { self.onStatus?("Disconnected: \(error.localizedDescription)") }
            stop()
        }
    }
}
