# MAH Mobile Camera protocol (draft v0)

Companion app (Android/iOS) → MAH Studio desktop source.

## Transports

| Mode | Path |
|---|---|
| Wi‑Fi | TCP to desktop LAN IP, default port `28887` |
| USB | Same TCP framing over USB tether / AOA / usbmux port-forward |

## Handshake

Client connects and sends one UTF-8 line:

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

## Media (v1)

### `mjpeg`

Repeated:

```text
[uint32_be length][jpeg bytes]
```

### `h264` (v1.1)

```text
[uint32_be length][annex-b access unit]
```

## Security

- Pairing code required on first connect (LAN still trusted-ish)
- v2: optional TLS / token from QR

## Non-goals (v0)

- DRM camera bypass
- Silent background capture without OS permission UI
