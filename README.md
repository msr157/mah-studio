# MAH Studio

**Mayank After Hours Studio** — fork of [OBS Studio](https://github.com/obsproject/obs-studio) with creator-first modules: multistream workflow, operator docks, first-run onboarding, and **built-in mobile camera** (Android / iOS over Wi‑Fi or USB tether).

Upstream OBS is the engine. MAH product layers live under [`mah/`](mah/), [`mobile/`](mobile/), and [`plugins/mah-mobile-cam/`](plugins/mah-mobile-cam/).

## Repo map

| Path | Role |
|---|---|
| `plugins/mah-mobile-cam/` | OBS source: MAH1 MJPEG + PCM mic, LAN discovery |
| `frontend/wizards/MahOnboarding*` | First-run setup wizard (before AutoConfig) |
| `mah/` | Protocol, roadmap, starter + dock HTML |
| `mobile/android` | MAH Cam Android app (Gradle) |
| `mobile/ios` | MAH Cam iOS sources + XcodeGen spec |
| `mobile/tools/` | Desktop MJPEG test client (no phone) |

Companion suite (docks lock + multi-rtmp installer for stock OBS): https://github.com/msr157/mah-obs

## Status (honest)

| Done on GitHub | Still later |
|---|---|
| MJPEG video + mic PCM over Wi‑Fi/USB tether | Play / TestFlight signed builds |
| Android + iOS companion source | H.264 path, AOA/usbmux |
| First-run MahOnboarding wizard | Native in-tree multistream/multichat |
| Starter + multichat help docks | Signed MAH Windows installer |

## Quick links

- Protocol: [`mah/PROTOCOL.md`](mah/PROTOCOL.md)
- Roadmap: [`mah/ROADMAP.md`](mah/ROADMAP.md)
- Docks: [`mah/docks/`](mah/docks/)

## Build

Follow upstream OBS build docs for your OS, then build this tree. `mah-mobile-cam` is registered in `plugins/CMakeLists.txt`.

Without building OBS, you can still open `mobile/android` in Android Studio / `mobile/ios` via XcodeGen.

## License

GPL-2.0 (same as OBS Studio). Upstream copyright retained.

---

Based on OBS Studio by the OBS Project contributors.
