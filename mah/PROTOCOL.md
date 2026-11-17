# MAH Mobile Camera protocol (v1.1)

Companion app (Android / iOS / test client) → MAH Studio `mah_mobile_cam` source.

## Transports

| Mode | Path |
|---|---|
| Wi‑Fi | TCP to desktop LAN IP, default port `28887` |
| USB tether | Same TCP — enable USB tethering; use the PC IP shown in the source |
| USB AOA / usbmux | Planned — same framing over accessory / port-forward |

## Discovery (optional)

UDP broadcast to port `28888`:

```text
MAH1 DISCOVER\n
```

Desktop replies:

```text
MAH1 HOST <ip> <tcp-port> <pairing>\n
```

## Handshake

```text
MAH1 <pairing> <width>x<height> <codec>\n
```

Codecs:

| Codec | Meaning |
|---|---|
| `mjpeg` | Video only |
| `mjpeg+pcm` | Video + mic (PCM s16le mono 48 kHz) |

Example:

```text
MAH1 MAH1 1280x720 mjpeg+pcm\n
```

Server:

```text
MAH1 OK\n
```

or `MAH1 ERR <reason>` (`bad-handshake`, `bad-pairing`, `codec-unsupported`).

## Media (v1.1)

Typed frames (preferred):

```text
[uint8 type][uint32_be length][payload]
```

| type | payload |
|---|---|
| `'V'` | JPEG bytes |
| `'A'` | PCM s16le mono @ 48000 Hz |

Legacy v1 (length-prefixed JPEG with no type byte) is still accepted by the desktop plugin.

### `h264` (reserved)

Annex-B access units — not accepted until decoder lands.

## Security

- Pairing code required (source properties; regenerate button)
- LAN trusted-ish; v2 optional TLS / QR token

## Non-goals

- DRM camera bypass
- Silent capture without OS permission UI
