package `in`.mayankafterhours.mahcam

/**
 * USB path for MAH Cam (v1):
 *
 * Prefer **USB tethering** (Settings → Hotspot & tethering → USB tethering).
 * The phone and PC share a link-local subnet; enter the PC IP shown in
 * MAH Mobile Camera source properties. Protocol is identical to Wi‑Fi.
 *
 * AOA (Android Open Accessory) host mode is reserved for a later milestone —
 * same MAH1 framing over an accessory socket once the desktop helper lands.
 */
object UsbTetherHelper {
    const val NOTES = "Enable USB tethering, then use the PC tether IP (often 192.168.42.x / 192.168.137.x)."
}
