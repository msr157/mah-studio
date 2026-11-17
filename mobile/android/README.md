# MAH Cam — Android companion

Streams the phone camera to **MAH Studio** (`mah_mobile_cam` source) over Wi‑Fi or USB.

## Status

Scaffold only. Target stack:

- Kotlin + CameraX
- Foreground service while streaming
- TCP client implementing `mah/PROTOCOL.md`
- QR / pairing code UI
- USB: prefer Wi‑Fi over USB tether first; AOA later

## Dev sketch

```text
app/
  src/main/java/in/rackops/mahcam/
    MainActivity.kt
    StreamService.kt
    MahProtocol.kt
```

Package id proposal: `in.mayankafterhours.mahcam`
