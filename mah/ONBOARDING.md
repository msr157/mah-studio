# MAH Onboarding wizard (design)

OBS already has `frontend/wizards/AutoConfig*`. MAH extends first-run:

1. **Welcome** — Mayank After Hours / MAH Studio branding  
2. **Language** — Hinglish / English  
3. **Mobile camera** — show pairing code + QR; wait for companion `MAH1 OK`  
4. **Destinations** — guide to Multistream dock (3 Custom RTMP targets)  
5. **Layout** — apply loved dock preset from `mah-obs`  
6. **Done** — open Starting Soon scene pack tip  

## Hook point (upstream)

`frontend/widgets/OBSBasic.cpp` FirstRun currently queues `on_autoConfigure_triggered`.  
MAH will queue `MahOnboarding` **before or after** AutoConfig based on a `General/MahOnboardingDone` flag.

Implementation lands in `frontend/wizards/MahOnboarding*.{hpp,cpp,ui}` in a follow-up commit once the mobile handshake is demoable.
