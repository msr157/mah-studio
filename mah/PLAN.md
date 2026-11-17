# MAH Studio — research plan & sequencing

Last updated: 2026-11-18  
Goal: Iriun-class mobile cam + multistream + multichat, honestly scoped.

---

## Already shipped (do not redo)

| Area | Status |
|---|---|
| OBS fork `msr157/mah-studio` | Live |
| `mah-mobile-cam` MJPEG + PCM + H.264 + discovery | Code on `master` |
| Android / iOS companions (Wi‑Fi / USB tether) | Source in repo |
| `mah-multistream` (obs-multi-rtmp based) | In-tree dock |
| `mah-multichat` Twitch IRC | In-tree dock |
| MahOnboarding first-run wizard | Hooked |
| mah-obs installer suite | Separate repo |

---

## Research findings (before more code)

### 1) YouTube Live Chat (official)

- **Read:** `GET youtube/v3/liveChat/messages?liveChatId=&part=snippet,authorDetails`
- **Send:** `POST` same resource with OAuth scope `youtube` / `youtube.force-ssl`
- **Needs from you:** Google Cloud project → enable YouTube Data API v3 → API key (public read) + OAuth client (send)
- **Flow:** resolve `liveChatId` from `liveBroadcasts.list` (`mine=true`) or paste ID while live
- **Quota:** list costs ~5 units; respect `pollingIntervalMillis`
- **OBS note:** keep browser dock as fallback; native client for merged feed

### 2) Kick chat (unofficial read)

- **Read:** Pusher WS `wss://ws-us2.pusher.com/app/32cbd69e4b950bf97679?...`
- Subscribe `chatrooms.{chatroomId}.v2` (public, no auth)
- Resolve ID: `GET https://kick.com/api/v2/channels/{slug}` → `chatroom.id` (Cloudflare may block bare clients → User-Agent + retry)
- Event: `App\Events\ChatMessageEvent` (data double-encoded JSON)
- **Send:** no stable public desktop API; keep “open Kick / browser dock” for reply (honest)
- **Risk:** Kick can rotate Pusher key; document fallback

### 3) AOA (Android Open Accessory)

- PC must be **USB host**, phone becomes accessory (`0x18D1` / `0x2D00`)
- AOA is **bulk pipe only** — not UVC; we reuse **MAH1 framing** over the accessory FD
- Architecture: `mah-aoa-host` (PC) switches phone to accessory → bridges bulk ↔ `127.0.0.1:28887` → existing `mah-mobile-cam` TCP server
- Android app: `UsbManager.openAccessory` → same MAH1 client as Wi‑Fi
- **Windows:** needs libusb/WinUSB driver story; ship Python/`pyusb` helper first, then native later
- iOS: AOA N/A → keep usbmux forward scripts

### 4) Play / TestFlight (not pure code)

| Need | Owner |
|---|---|
| Google Play Console + signing keystore | You |
| Apple Developer + certificates | You |
| CI: debug APK / unsigned AAB / Xcode archive scripts | Us (repo) |
| Privacy policy URL, store screenshots | You + us drafts |

We **cannot** publish to stores without your accounts. We **can** ship build scripts + listing checklist.

### 5) Gaps vs “best OBS / Iriun-class”

| Gap | Priority | Blocker |
|---|---|---|
| Kick read in multichat | P0 | None (unofficial) |
| YouTube read/send client | P0 | API key / OAuth from you for live test |
| Merged feed + send-to-selected | P1 | After 3 readers work |
| AOA host bridge + Android accessory path | P1 | Windows USB driver UX |
| H.264 encode on phone (Battery) | P1 | MediaCodec / VT |
| QR pairing in onboarding | P2 | None |
| Auto-apply mah-obs layout | P2 | Path to presets |
| CI Windows OBS build artifact | P2 | GitHub Actions + deps cache |
| Signed installer / auto-update | P3 | Cert + release channel |
| RackOps Focus Room presets | P3 | Precision node online |
| Vertical canvas helpers | P3 | Product design |

---

## Sequenced work (code after this plan)

### Phase A — Multichat complete (this session)

1. Kick Pusher client (read) + slug → chatroom resolve  
2. YouTube Live Chat client (poll read + OAuth bearer send)  
3. Unified compose: “send to Twitch / YT / Kick*” with Kick = open/external until API exists  
4. Docs: how to create Google API key / paste liveChatId  

### Phase B — AOA path

1. Protocol note: MAH1 over AOA bulk = same bytes as TCP  
2. Android `AoaSession` (UsbAccessory → streams)  
3. PC `mah/tools/mah-aoa-host` Python bridge (pyusb) localhost:28887  
4. Desktop property “USB AOA” points at bridge docs  

### Phase C — Store / CI (needs your keys later)

1. Android `assembleRelease` + unsigned AAB workflow  
2. Fastlane / Play upload stub (secrets in GH)  
3. iOS archive instructions + TestFlight checklist  
4. Store listing copy draft  

### Phase D — Product polish

1. Phone H.264 encode option  
2. QR pairing  
3. Layout preset apply from onboarding  
4. Windows CI build of mah plugins  

---

## Decisions locked

- Multistream stays plugin-based (not patch Settings→Stream)  
- Kick **send** stays browser until Kick offers a supported API  
- AOA reuses MAH1 (no second media protocol)  
- Secrets (stream keys, OAuth, API keys) never committed  

## What we need from you (when ready)

1. Google Cloud OAuth client ID + API key (YouTube)  
2. Play Console / Apple Developer access (or CI secrets)  
3. Confirm Kick channel slug + Twitch nick for live tests  
4. Whether Windows code-signing cert exists for installer  
