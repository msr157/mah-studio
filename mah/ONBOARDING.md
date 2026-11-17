# MAH Onboarding wizard (design)

OBS already has `frontend/wizards/AutoConfig*`. MAH extends first-run:

1. **Welcome** — Mayank After Hours / MAH Studio branding  
2. **Language** — Hinglish / English  
3. **Mobile camera** — show pairing code + LAN IP; wait for companion `MAH1 OK`  
4. **Destinations** — guide to Multistream dock (3 Custom RTMP targets)  
5. **Layout** — apply loved dock preset from `mah-obs`  
6. **Done** — open starter dock (`mah/starter/index.html`)

## Starter page (available now)

File dock URL (local):

```text
file:///D:/Codes/mah-studio/mah/starter/index.html
```

Or copy into an OBS Custom Browser Dock named **MAH Starter**.

## Hook point (upstream)

`frontend/widgets/OBSBasic.cpp` FirstRun currently queues `on_autoConfigure_triggered`.  
MAH will queue `MahOnboarding` based on `General/MahOnboardingDone`.

Qt implementation: `frontend/wizards/MahOnboarding*.{hpp,cpp,ui}` — next milestone after mobile cam is demoable on a built OBS binary.
