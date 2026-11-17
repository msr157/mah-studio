# Mayank After Hours Studio (`mah-studio`)

This repository is a **fork of [OBS Studio](https://github.com/obsproject/obs-studio)** evolved into **MAH Studio** — a creator build focused on:

- Multistream (YouTube + Twitch + Kick)
- Operator docks / locked layouts
- First-run onboarding wizard
- **First-class mobile camera** (Android / iOS over Wi‑Fi or USB) — Iriun-class capability built in

Upstream OBS remains the engine. MAH modules live under `plugins/mah-*`, `mah/`, and `mobile/`.

## Honest status

| Area | Now | Next |
|---|---|---|
| Fork published | Yes — `msr157/mah-studio` | Track upstream OBS |
| Mobile camera plugin | Source registered + TCP listen + placeholder frames | H.264/JPEG ingest, pairing UI, USB accessory |
| Android / iOS apps | Project stubs + protocol draft | Ship companion APK/IPA |
| Onboarding wizard | Spec + hook points documented | Qt pages wired into FirstRun |
| Multistream | Bundled via [mah-obs](https://github.com/msr157/mah-obs) suite / multi-rtmp | Optional in-tree dock branding |

Building a “best OBS” is a **product**, not a weekend paste. This fork is the honest foundation.

## Companion suite

Operator presets + installer (docks / layout lock / multi-rtmp):  
https://github.com/msr157/mah-obs

## Build (developer)

Same as upstream OBS. See upstream docs, then enable `mah-mobile-cam` (on by default in this fork’s plugin list).

```bash
# high level — follow OBS Windows build guide for deps
cmake --preset windows-x64
cmake --build --preset windows-x64
```

## License

OBS Studio core: GPL-2.0 (upstream copyright retained).  
MAH-authored files under `mah/`, `mobile/`, `plugins/mah-*`: GPL-2.0 to match the host (required for in-tree plugins).
