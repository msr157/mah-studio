# MAH Mobile Camera protocol (v1.3)

**Best path (no app):** phone browser → `http://LAN:28889/?p=PAIRING` → Allow camera → WebSocket MAH1.

Native companion (Android / iOS / test client) still speaks the same MAH1 framing over TCP.

## Transports

| Mode | Path |
|---|---|
| **Web (preferred)** | HTTP page + WebSocket on port `28889` (same MAH1 frames) |
| Wi‑Fi native | TCP to desktop LAN IP, default port `28887` |
| USB tether | Same TCP — Android USB tethering / phone hotspot edge |
| iOS usbmux | `mobile/tools/ios-usbmux-forward.*` → same TCP on forwarded port |
| AOA | PC `mah/tools/mah-aoa-host` bridges USB bulk ↔ TCP; phone `AoaSession` speaks MAH1 |

## Web companion

1. OBS source shows **Phone URL**: `http://<lan-ip>:28889/?p=<pairing>`
2. Phone (same Wi‑Fi / USB tether) opens URL in Safari/Chrome
3. Tap **Allow camera & start** → browser sends JPEG frames over `/ws`
4. Keep the tab open while streaming

WebSocket: text hello `MAH1 <pairing> <WxH> mjpeg` + newline, then binary frames identical to TCP media (`V`/`A`/`H` + length + payload).

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
