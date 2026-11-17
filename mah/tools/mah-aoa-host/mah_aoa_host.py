#!/usr/bin/env python3
"""
MAH AOA host bridge (experimental).

Puts a compatible Android phone into accessory mode and bridges USB bulk
I/O ↔ TCP localhost (default 127.0.0.1:28887) where mah-mobile-cam listens.

Requires:
  pip install pyusb
  Windows: Zadig WinUSB driver on the Google accessory interface after switch
  Linux: udev rules for 18d1:2d00

This is the host half of AOA — the phone app opens UsbAccessory and speaks MAH1.
"""

from __future__ import annotations

import argparse
import socket
import sys
import threading
import time

try:
    import usb.core
    import usb.util
except ImportError:
    raise SystemExit("pip install pyusb") from None

GOOGLE_VID = 0x18D1
ACCESSORY_PIDS = {0x2D00, 0x2D01}
AOA_GET_PROTOCOL = 51
AOA_SEND_STRING = 52
AOA_START = 53

STRINGS = [
    (0, "MayankAfterHours"),
    (1, "MAHCam"),
    (2, "MAH Studio mobile camera"),
    (3, "1.0"),
    (4, "https://github.com/msr157/mah-studio"),
    (5, "MAH1"),
]


def find_device():
    for d in usb.core.find(find_all=True):
        if d.idVendor == GOOGLE_VID and d.idProduct in ACCESSORY_PIDS:
            return d, True
        try:
            buf = d.ctrl_transfer(0xC0, AOA_GET_PROTOCOL, 0, 0, 2, timeout=1000)
            ver = int(buf[0]) | (int(buf[1]) << 8)
            if ver >= 1:
                return d, False
        except Exception:
            continue
    return None, False


def switch_to_accessory(dev) -> None:
    for idx, s in STRINGS:
        data = (s + "\x00").encode("utf-8")
        dev.ctrl_transfer(0x40, AOA_SEND_STRING, 0, idx, data, timeout=1000)
    dev.ctrl_transfer(0x40, AOA_START, 0, 0, None, timeout=1000)
    time.sleep(1.5)


def bridge(dev, host: str, port: int) -> None:
    cfg = dev.get_active_configuration()
    intf = cfg[(0, 0)]
    ep_out = usb.util.find_descriptor(
        intf,
        custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT,
    )
    ep_in = usb.util.find_descriptor(
        intf,
        custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN,
    )
    if not ep_in or not ep_out:
        raise RuntimeError("bulk endpoints missing — claim WinUSB via Zadig after accessory switch")

    sock = socket.create_connection((host, port), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"bridged USB ↔ {host}:{port}")

    stop = threading.Event()

    def usb_to_tcp():
        while not stop.is_set():
            try:
                data = ep_in.read(16384, timeout=1000)
                if data:
                    sock.sendall(bytes(data))
            except usb.core.USBTimeoutError:
                continue
            except Exception as e:
                print("usb→tcp:", e)
                stop.set()
                break

    def tcp_to_usb():
        sock.settimeout(1.0)
        while not stop.is_set():
            try:
                data = sock.recv(16384)
                if not data:
                    stop.set()
                    break
                ep_out.write(data)
            except socket.timeout:
                continue
            except Exception as e:
                print("tcp→usb:", e)
                stop.set()
                break

    t1 = threading.Thread(target=usb_to_tcp, daemon=True)
    t2 = threading.Thread(target=tcp_to_usb, daemon=True)
    t1.start()
    t2.start()
    try:
        while not stop.is_set():
            time.sleep(0.2)
    except KeyboardInterrupt:
        stop.set()
    sock.close()


def main() -> None:
    ap = argparse.ArgumentParser(description="MAH AOA host ↔ TCP bridge")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=28887)
    args = ap.parse_args()

    print("Looking for Android device…")
    for _ in range(10):
        dev, is_acc = find_device()
        if dev is None:
            time.sleep(0.5)
            continue
        if not is_acc:
            print("Switching to accessory mode…")
            try:
                switch_to_accessory(dev)
            except Exception as e:
                print("switch failed:", e, file=sys.stderr)
                print("On Windows: install WinUSB (Zadig) after reconnect.", file=sys.stderr)
                return
            time.sleep(2)
            continue
        print(f"Accessory {dev.idVendor:04x}:{dev.idProduct:04x}")
        try:
            usb.util.claim_interface(dev, 0)
        except Exception:
            pass
        bridge(dev, args.host, args.port)
        return
    print("No AOA-capable device found. Plug in USB + enable file transfer, retry.")


if __name__ == "__main__":
    main()
