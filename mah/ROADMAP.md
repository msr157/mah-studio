# MAH Studio roadmap

## v0 — Foundation

- [x] Public fork `msr157/mah-studio` from OBS Studio
- [x] `plugins/mah-mobile-cam` scaffold
- [x] Protocol draft + mobile stubs
- [x] Product docs + companion suite link (`mah-obs`)

## v1 — Usable mobile cam (this push)

- [x] Desktop: MAH1 handshake + pairing check
- [x] Desktop: length-prefixed **MJPEG ingest** → OBS async video (`stb_image`)
- [x] Desktop: UDP LAN discovery (`28888`)
- [x] Desktop: Wi‑Fi + USB-tether connection modes (same TCP path)
- [x] Android companion: CameraX + foreground service + MAH1 MJPEG + discover
- [x] iOS companion: AVFoundation + MAH1 MJPEG + discover (open in Xcode)
- [x] Python test client (`mobile/tools/mah_mjpeg_test_client.py`)
- [ ] Signed Play / TestFlight builds
- [ ] H.264 Annex-B decode path
- [ ] Phone mic audio track
- [ ] Native AOA / usbmux accessory tunnel

## v1.5 — Onboarding

- [ ] First-run **MAH Onboarding** wizard (extends OBS AutoConfig)
  - Brand / language (Hinglish)
  - Connect mobile camera (pairing + QR)
  - Add YT / Twitch / Kick outputs
  - Apply loved dock preset from `mah-obs`
- [ ] Starter page dock (After Hours) on first launch

## v2 — Multistream + Multichat native

- [ ] In-tree Multistream dock (obs-multi-rtmp branded)
- [ ] Multichat dock: Twitch + YouTube + Kick
- [ ] “Send to selected platforms” toggles

## v3 — Polish

- [ ] Auto-update channel for MAH builds
- [ ] Signed Windows installer
- [ ] Vertical / dual-format helpers
- [ ] RackOps Focus Room relay presets
