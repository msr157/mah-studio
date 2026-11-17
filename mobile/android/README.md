# MAH Cam — Android

Streams the phone camera to **MAH Studio** (`mah_mobile_cam`) over Wi‑Fi or USB tether.

## Features (v1)

- CameraX preview + ImageAnalysis → JPEG
- MAH1 TCP handshake + length-prefixed MJPEG
- LAN discovery (UDP broadcast)
- Foreground service while streaming
- Pairing code

## Open in Android Studio

1. Open `mobile/android` as a Gradle project.
2. Let AGP sync (SDK 35).
3. Run on a device on the **same Wi‑Fi** as the PC (or enable **USB tethering** and use the PC’s tether IP).

## Pairing flow

1. Build & run MAH Studio with `mah-mobile-cam`.
2. Add source **MAH Mobile Camera** — note **LAN IP**, **port** (`28887`), **pairing**.
3. On phone: tap **Find MAH Studio on LAN**, or type IP + pairing.
4. **Start camera stream**.

## USB

Enable USB tethering on Android. PC gets an interface (often `192.168.42.x` / `192.168.137.x`). Put that PC IP in the app — protocol is identical to Wi‑Fi.

## Package

`in.mayankafterhours.mahcam`
