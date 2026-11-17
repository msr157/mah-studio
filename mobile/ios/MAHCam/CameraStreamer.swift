import AVFoundation
import UIKit

final class CameraStreamer: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate, AVCaptureAudioDataOutputSampleBufferDelegate {
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
                let client = MahProtocol(host: host, port: port, pairing: pairing, withAudio: true)
                try client.connect()
                self.protocolClient = client
                DispatchQueue.main.async { self.onStatus?("Connected — video + mic") }
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

        if let device = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .front),
           let input = try? AVCaptureDeviceInput(device: device),
           session.canAddInput(input) {
            session.addInput(input)
        }

        if let mic = AVCaptureDevice.default(for: .audio),
           let micInput = try? AVCaptureDeviceInput(device: mic),
           session.canAddInput(micInput) {
            session.addInput(micInput)
        }

        let video = AVCaptureVideoDataOutput()
        video.alwaysDiscardsLateVideoFrames = true
        video.setSampleBufferDelegate(self, queue: queue)
        if session.canAddOutput(video) { session.addOutput(video) }

        let audio = AVCaptureAudioDataOutput()
        audio.setSampleBufferDelegate(self, queue: queue)
        if session.canAddOutput(audio) { session.addOutput(audio) }

        session.commitConfiguration()
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        if output is AVCaptureAudioDataOutput {
            sendAudio(sampleBuffer)
            return
        }

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

    private func sendAudio(_ sampleBuffer: CMSampleBuffer) {
        guard let block = CMSampleBufferGetDataBuffer(sampleBuffer) else { return }
        var length = 0
        var dataPointer: UnsafeMutablePointer<Int8>?
        CMBlockBufferGetDataPointer(block, atOffset: 0, lengthAtOffsetOut: nil, totalLengthOut: &length, dataPointerOut: &dataPointer)
        guard let dataPointer, length > 0 else { return }
        let data = Data(bytes: dataPointer, count: length)
        do {
            try protocolClient?.sendPCM(data)
        } catch {
            DispatchQueue.main.async { self.onStatus?("Disconnected: \(error.localizedDescription)") }
            stop()
        }
    }
}
