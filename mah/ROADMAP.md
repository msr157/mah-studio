# MAH Studio roadmap

## v0 — Foundation

- [x] Public fork `msr157/mah-studio` from OBS Studio
- [x] `plugins/mah-mobile-cam` scaffold
- [x] Protocol draft + mobile stubs
- [x] Product docs + companion suite link (`mah-obs`)

## v1 — Usable mobile cam

- [x] Desktop: MAH1 handshake + pairing + UDP discovery
- [x] Desktop: MJPEG ingest → OBS async video
- [x] Desktop: phone mic PCM → OBS audio (`mjpeg+pcm`)
- [x] Wi‑Fi + USB-tether modes (same TCP)
- [x] Android companion (CameraX + mic + discover)
- [x] iOS companion sources + XcodeGen `project.yml`
- [x] Python test client
- [x] Starter dock + multichat help dock HTML
- [x] First-run **MahOnboarding** Qt wizard (hooked before AutoConfig)
- [ ] Signed Play / TestFlight builds
- [ ] H.264 Annex-B decode path
- [ ] Native AOA / usbmux accessory tunnel

## v1.5 — Onboarding polish

- [x] Starter page dock (`mah/starter`)
- [ ] QR pairing on wizard page
- [ ] Apply loved dock preset from `mah-obs` automatically
- [ ] Hinglish string table

## v2 — Multistream + Multichat native

- [ ] In-tree Multistream dock (obs-multi-rtmp branded)
- [ ] Multichat dock: Twitch + YouTube + Kick
- [ ] “Send to selected platforms” toggles
- [x] Interim: `mah/docks` + [mah-obs](https://github.com/msr157/mah-obs) suite

## v3 — Polish

- [ ] Auto-update channel for MAH builds
- [ ] Signed Windows installer
- [ ] Vertical / dual-format helpers
- [ ] RackOps Focus Room relay presets
