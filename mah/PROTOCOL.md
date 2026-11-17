# MAH Mobile Camera protocol (v1)

Companion app (Android / iOS / test client) → MAH Studio `mah_mobile_cam` source.

## Transports

| Mode | Path |
|---|---|
| Wi‑Fi | TCP to desktop LAN IP, default port `28887` |
| USB tether | Same TCP framing — enable USB tethering; phone and PC share a link-local / tether subnet; enter the PC IP shown in the source |
| USB AOA / usbmux | Planned (v1.x) — same framing over accessory / port-forward |

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

Client connects (TCP) and sends one UTF-8 line:

```text
MAH1 <pairing> <width>x<height> <codec>\n
```

Example:

```text
MAH1 MAH1 1280x720 mjpeg\n
```

Server replies:

```text
MAH1 OK\n
```

or

```text
MAH1 ERR <reason>\n
```

Reasons: `bad-handshake`, `bad-pairing`, `codec-unsupported`.

## Media (v1)

### `mjpeg` (required)

Repeated:

```text
[uint32_be length][jpeg bytes]
```

Desktop decodes JPEG → BGRA → OBS async video.

### `h264` (v1.1 — reserved)

```text
[uint32_be length][annex-b access unit]
```

Not accepted by desktop until decoder lands.

## Security

- Pairing code required (shown in source properties; regenerate button)
- LAN is trusted-ish; v2 adds optional TLS / QR token

## Non-goals

- DRM camera bypass
- Silent background capture without OS permission UI
- Replacing every virtual-webcam driver edge case on day one
