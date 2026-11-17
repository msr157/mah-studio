# MAH Studio

**Mayank After Hours Studio** — a fork of [OBS Studio](https://github.com/obsproject/obs-studio) with creator-first modules for multistream, operator docks, onboarding, and **built-in mobile camera** (Android / iOS over Wi‑Fi or USB).

> Upstream OBS is the engine. This fork adds MAH product layers. See [`mah/`](mah/) for product docs and roadmap.

## Why this fork exists

Stock OBS is excellent — and intentionally generic. MAH Studio targets a specific job:

1. Stream to **YouTube + Twitch + Kick** without fighting single-destination Settings  
2. Keep a **vertical chat / operator layout** locked  
3. Use your **phone as a camera** without depending on a separate virtual-webcam driver long-term (Iriun-class, integrated)  
4. Ship a **startup / onboarding** path that sets this up

## Repo map

| Path | Role |
|---|---|
| `plugins/mah-mobile-cam/` | OBS source: Wi‑Fi/USB mobile camera (v0 listen + placeholder frames) |
| `mah/` | Product docs, protocol, onboarding design, roadmap |
| `mobile/android`, `mobile/ios` | Companion app scaffolds |
| Upstream tree | `frontend/`, `libobs/`, `plugins/`, … unchanged OBS core |

Operator installer / dock presets (companion suite): https://github.com/msr157/mah-obs

## Honest status

**Usable today:** OBS fork + mobile-cam plugin skeleton + docs + `mah-obs` multistream/docks installer.  
**Not done overnight:** full phone video ingest, App Store/Play companions, polished Qt onboarding, signed installers.

That work is sequenced in [`mah/ROADMAP.md`](mah/ROADMAP.md).

## Quick links

- Protocol: [`mah/PROTOCOL.md`](mah/PROTOCOL.md)  
- Roadmap: [`mah/ROADMAP.md`](mah/ROADMAP.md)  
- Onboarding design: [`mah/ONBOARDING.md`](mah/ONBOARDING.md)

## Build

Follow upstream OBS build instructions for your OS, then build this tree. `mah-mobile-cam` is registered in `plugins/CMakeLists.txt`.

## License

GPL-2.0 (same as OBS Studio). Upstream copyright retained. MAH modules are GPL-2.0 to match in-tree requirements.

---

Based on OBS Studio by the OBS Project contributors.
