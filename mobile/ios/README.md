# MAH Cam — iOS companion

Streams the iPhone/iPad camera to **MAH Studio** over Wi‑Fi (USB via usbmux port-forward later).

## Status

Scaffold only. Target stack:

- SwiftUI + AVFoundation
- Background mode: `audio` / VoIP-adjacent patterns only where App Store allows; prefer foreground streaming
- TCP client implementing `mah/PROTOCOL.md`
- Local Network permission + Bonjour discovery (optional)

## Note vs Iriun

Iriun installs a desktop virtual camera driver. MAH Cam talks **directly to OBS** as a source plugin — fewer drivers, tighter integration, more work on the protocol side.
