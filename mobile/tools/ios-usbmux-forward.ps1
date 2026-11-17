# Windows helper note for iOS USB (usbmux).
# Use pymobiledevice3 or iTunes/Apple Devices + a userspace forwarder.
# Preferred on Windows today: Wi‑Fi or USB tether from Android.
param([int]$Port = 28887)
Write-Host "MAH Cam iOS USB: install pymobiledevice3, then forward TCP $Port."
Write-Host "  python -m pymobiledevice3 usbmux forward $Port $Port"
Write-Host "Then point MAH Cam at 127.0.0.1:$Port on the phone side via the forward path,"
Write-Host "or use Wi‑Fi discovery (UDP 28888) which needs no USB tunnel."
