# MAH Studio roadmap

## v0 — Foundation (this push)

- [x] Public fork `msr157/mah-studio` from OBS Studio
- [x] `plugins/mah-mobile-cam` — OBS source, Wi‑Fi/USB props, TCP listen, placeholder video
- [x] Mobile companion stubs (`mobile/android`, `mobile/ios`)
- [x] Protocol draft for phone ↔ desktop
- [x] Product docs + companion suite link (`mah-obs`)

## v1 — Usable mobile cam

- [ ] Companion apps stream MJPEG or H.264 Annex-B over TCP
- [ ] Desktop plugin decodes → gs_texture
- [ ] Pairing code UI + LAN discovery (mDNS / QR)
- [ ] USB: Android AOA / iOS usbmux tunnel path
- [ ] Audio from phone mic (optional track)

## v1.5 — Onboarding

- [ ] First-run **MAH Onboarding** wizard (extends OBS AutoConfig)
  - Brand / language (Hinglish)
  - Connect mobile camera
  - Add YT / Twitch / Kick outputs
  - Apply loved dock layout from `mah-obs` presets
- [ ] Starter page dock (After Hours) on first launch

## v2 — Multistream + Multichat native

- [ ] In-tree Multistream dock (based on obs-multi-rtmp, branded)
- [ ] Multichat dock: Twitch IRC + YouTube Live Chat + Kick (read; send where allowed)
- [ ] “Send to selected platforms” toggles

## v3 — Polish

- [ ] Auto-update channel for MAH builds
- [ ] Signed Windows installer
- [ ] Vertical / dual-format helpers
- [ ] RackOps Focus Room relay presets
