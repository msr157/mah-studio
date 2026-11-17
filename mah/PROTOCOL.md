# MAH Mobile Camera protocol (v1.2)

Companion app (Android / iOS / test client) → MAH Studio `mah_mobile_cam` source.

## Transports

| Mode | Path |
|---|---|
| Wi‑Fi | TCP to desktop LAN IP, default port `28887` |
| USB tether | Same TCP — Android USB tethering / phone hotspot edge |
| iOS usbmux | `mobile/tools/ios-usbmux-forward.*` → same TCP on forwarded port |
| AOA | PC `mah/tools/mah-aoa-host` bridges USB bulk ↔ TCP; phone `AoaSession` speaks MAH1 |

## Discovery

UDP `28888`:

```text
MAH1 DISCOVER\n
→ MAH1 HOST <ip> <tcp-port> <pairing>\n
```

## Handshake

```text
MAH1 <pairing> <width>x<height> <codec>\n
```

| Codec | Meaning |
|---|---|
| `mjpeg` | JPEG video |
| `mjpeg+pcm` | JPEG + mic PCM |
| `h264` | H.264 Annex-B video (needs FFmpeg at build) |
| `h264+pcm` | H.264 + mic PCM |

Server: `MAH1 OK\n` or `MAH1 ERR <reason>`.

## Media

```text
[uint8 type][uint32_be length][payload]
```

| type | payload |
|---|---|
| `'V'` | JPEG |
| `'H'` | H.264 Annex-B access unit |
| `'A'` | PCM s16le mono @ 48000 Hz |

Legacy length-prefixed JPEG (no type byte) still accepted.
