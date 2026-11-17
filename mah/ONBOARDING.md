# MAH Onboarding

## Implemented

`frontend/wizards/MahOnboarding.{hpp,cpp}` — Qt wizard pages:

1. Welcome (After Hours branding)
2. Mobile camera (pairing / MAH Cam / USB tether)
3. Multistream + chat (`mah-obs` + docks)
4. Done → starter dock tip

**First-run hook:** `OBSBasic.cpp` queues `on_mahOnboarding_triggered` when `General/MahOnboardingDone` is unset, then the normal AutoConfig wizard.

Flag is set when the user finishes MahOnboarding.

## Starter + help docks (file URLs)

- `mah/starter/index.html`
- `mah/docks/multichat-help.html`

Add via **View → Docks → Custom Browser Docks** (or mah-obs installer).

## Still polishing

- QR code with `ip:port:pairing`
- Auto-apply loved layout from mah-obs presets
- Hinglish locale strings
