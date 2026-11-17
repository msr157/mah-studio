# MAH Studio

**Mayank After Hours Studio** — historical fork of [OBS Studio](https://github.com/obsproject/obs-studio).

> **Product shipping:** [mah-obs](https://github.com/msr157/mah-obs) + **stock OBS**. Fork CI/builds here are **not** maintained for the live MAH product (phone cam, multistream suite). Sources remain for reference and mobile companions.

## Modules

| Path | Role |
|---|---|
| `plugins/mah-mobile-cam` | Phone camera — MJPEG / H.264 + mic, Wi‑Fi / USB |
| `plugins/mah-multistream` | YouTube + Twitch + Kick outputs (obs-multi-rtmp based) |
| `plugins/mah-multichat` | Twitch IRC dock + YT/Kick helpers |
| `frontend/wizards/MahOnboarding*` | First-run setup |
| `mobile/android`, `mobile/ios` | MAH Cam companions |
| `mah/` | Protocol, roadmap, starter/docks HTML |

Suite installer for stock OBS: https://github.com/msr157/mah-obs

## Honest status

**In repo now:** mobile cam (MJPEG+H.264+PCM), multistream dock, multichat (Twitch native), onboarding, USB helper scripts.  
**Still later:** store-signed apps, full YT/Kick native chat APIs, AOA accessory mode, signed MAH installer.

## License

GPL-2.0 (OBS). `mah-multistream` includes code based on [obs-multi-rtmp](https://github.com/sorayuki/obs-multi-rtmp) (GPL-2.0) — see `plugins/mah-multistream/NOTICE.md`.
