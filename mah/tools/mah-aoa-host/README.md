# mah-aoa-host

PC-side **Android Open Accessory** bridge for MAH Cam.

```
Phone (UsbAccessory / MAH1)  ←USB bulk→  mah_aoa_host.py  ←TCP→  mah-mobile-cam :28887
```

## Run

```bash
pip install pyusb
python mah_aoa_host.py --port 28887
```

## Windows

After accessory switch (`18d1:2d00`), assign **WinUSB** with Zadig, then re-run.

## Simpler USB

Prefer **USB tethering** + TCP to the PC tether IP when possible.
