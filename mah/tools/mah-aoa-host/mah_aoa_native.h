/*
 * Native WinUSB / libusb AOA host scaffold (future C/C++ binary).
 * Current usable path: mah_aoa_host.py (pyusb).
 *
 * Planned binary: mah-aoa-host.exe
 *  1) Find Android device supporting AOA GET_PROTOCOL (51)
 *  2) SEND_STRING manufacturer/model (MayankAfterHours / MAHCam)
 *  3) START accessory mode
 *  4) Re-open 18d1:2d00 bulk endpoints
 *  5) Bridge to TCP 127.0.0.1:28887 (MAH1)
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAH_AOA_TCP_PORT 28887
#define MAH_AOA_MANUFACTURER "MayankAfterHours"
#define MAH_AOA_MODEL "MAHCam"

/* Returns 0 on success. Implementation TBD (libusb-1.0). */
int mah_aoa_bridge_run(const char *tcp_host, int tcp_port);

#ifdef __cplusplus
}
#endif
