# Mayank After Hours Studio (`mah-studio`)

Fork of [OBS Studio](https://github.com/obsproject/obs-studio) evolved into **MAH Studio**:

- Multistream (YouTube + Twitch + Kick) via [mah-obs](https://github.com/msr157/mah-obs)
- Operator docks / locked layouts
- First-run onboarding (spec → Qt next)
- **First-class mobile camera** (Android / iOS over Wi‑Fi or USB tether)

## Status (honest)

| Area | Now |
|---|---|
| Fork | `msr157/mah-studio` |
| Mobile source | `mah_mobile_cam` — MAH1 handshake, MJPEG decode, UDP discover, USB-tether mode |
| Android | Full Gradle app under `mobile/android` (CameraX + FG service) |
| iOS | Swift sources under `mobile/ios/MAHCam` (open in Xcode) |
| Test without phone | `mobile/tools/mah_mjpeg_test_client.py` |
| Onboarding wizard | Spec in `ONBOARDING.md` — Qt pages next |
| Multistream | `mah-obs` suite today; in-tree dock later |

## Quick test (desktop)

1. Build OBS with `mah-mobile-cam` enabled.
2. Add source **MAH Mobile Camera** (pairing default `MAH1`, port `28887`).
3. On the same machine:

```bash
pip install Pillow
python mobile/tools/mah_mjpeg_test_client.py --host 127.0.0.1 --pairing MAH1
```

You should see amber test frames in the source.

## Phone

See `mobile/android/README.md` and `mobile/ios/README.md`.  
Protocol: `PROTOCOL.md`.

## Build

Same as upstream OBS. `mah-mobile-cam` is registered in `plugins/CMakeLists.txt`.

## License

OBS core: GPL-2.0. MAH paths under `mah/`, `mobile/`, `plugins/mah-*`: GPL-2.0 to match in-tree plugins.
