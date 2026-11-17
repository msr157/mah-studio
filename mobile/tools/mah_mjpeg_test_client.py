#!/usr/bin/env python3
"""Desktop test client for mah-mobile-cam (no phone required).

Sends a synthetic MJPEG stream with the MAH1 handshake.

  python mah_mjpeg_test_client.py --host 127.0.0.1 --port 28887 --pairing MAH1
"""

from __future__ import annotations

import argparse
import io
import socket
import struct
import time

try:
    from PIL import Image, ImageDraw
except ImportError as e:
    raise SystemExit("Install Pillow: pip install Pillow") from e


def frame_jpeg(frame_i: int, w: int, h: int, quality: int = 70) -> bytes:
    img = Image.new("RGB", (w, h), (12, 16, 20))
    d = ImageDraw.Draw(img)
    y = int((frame_i * 8) % max(h - 40, 1))
    d.rectangle([0, y, w, y + 24], fill=(229, 184, 117))
    d.text((24, 24), f"MAH test frame {frame_i}", fill=(248, 250, 252))
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=quality)
    return buf.getvalue()


def main() -> None:
    ap = argparse.ArgumentParser(description="MAH1 MJPEG test client")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=28887)
    ap.add_argument("--pairing", default="MAH1")
    ap.add_argument("--width", type=int, default=1280)
    ap.add_argument("--height", type=int, default=720)
    ap.add_argument("--fps", type=float, default=15.0)
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    hello = f"MAH1 {args.pairing} {args.width}x{args.height} mjpeg\n".encode()
    sock.sendall(hello)
    reply = b""
    while b"\n" not in reply:
        chunk = sock.recv(64)
        if not chunk:
            raise SystemExit("closed during handshake")
        reply += chunk
    print("handshake:", reply.decode(errors="replace").strip())
    if not reply.startswith(b"MAH1 OK"):
        raise SystemExit("rejected")

    i = 0
    period = 1.0 / max(args.fps, 1.0)
    try:
        while True:
            t0 = time.perf_counter()
            jpeg = frame_jpeg(i, args.width, args.height)
            sock.sendall(struct.pack(">I", len(jpeg)) + jpeg)
            i += 1
            if i % 30 == 0:
                print(f"sent {i} frames, last jpeg={len(jpeg)} bytes")
            dt = time.perf_counter() - t0
            time.sleep(max(0.0, period - dt))
    except KeyboardInterrupt:
        print("stop")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
