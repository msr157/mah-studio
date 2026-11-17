# MAH Onboarding

## Implemented

`frontend/wizards/MahOnboarding.{hpp,cpp}` — Qt wizard pages:

1. Welcome (After Hours branding)
2. **Phone camera (no app)** — QR / URL → `http://LAN:28889/?p=PAIR` → Allow → live
3. Multistream + chat (`mah-obs` + docks)
4. Loved layout apply
5. Done → starter dock tip

**Optimistic phone flow**

1. Add source **MAH Mobile Camera**
2. Open the **Phone URL** from source properties (or scan onboarding QR)
3. Tap **Allow camera & start**
4. Keep the browser tab open

Native MAH Cam (TCP `28887`) remains optional for USB / H.264.

**First-run hook:** `OBSBasic.cpp` queues `on_mahOnboarding_triggered` when `General/MahOnboardingDone` is unset, then the normal AutoConfig wizard.

Flag is set when the user finishes MahOnboarding.

## Starter + help docks (file URLs)

- `mah/starter/index.html`
- `mah/docks/multichat-help.html`

Add via **View → Docks → Custom Browser Docks** (or mah-obs installer).
