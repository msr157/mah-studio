# MAH Cam — iOS

Streams iPhone / iPad camera to **MAH Studio** over Wi‑Fi (USB tether / usbmux later).

## Features (v1)

- AVFoundation capture → JPEG
- MAH1 TCP + MJPEG framing
- LAN UDP discovery
- Pairing code
- SwiftUI shell (`MAHCam/`)

## Open in Xcode

1. File → New → Project → App (SwiftUI, iOS 16+).
2. Replace / add sources from `MAHCam/`:
   - `MAHCamApp.swift`
   - `ContentView.swift`
   - `MahProtocol.swift`
   - `CameraStreamer.swift`
   - Merge keys from `Info.plist` (camera + local network).
3. Run on a physical device (simulator has no useful camera for this).

## Pairing flow

Same as Android: add **MAH Mobile Camera** in OBS → note IP / port / pairing → Find or type → Start.

## vs Iriun

Iriun uses a desktop virtual camera driver. MAH Cam talks **directly to OBS** as a source — fewer drivers, tighter integration.
