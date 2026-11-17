# Play / TestFlight checklist

We cannot publish without your developer accounts. Prep in-repo:

## Android (Play)

- [ ] Create app `in.mayankafterhours.mahcam` in Play Console
- [ ] Generate upload keystore locally (never commit)
- [ ] Privacy policy URL (camera + mic)
- [ ] Screenshots: connect screen, streaming notification
- [ ] Data safety form: camera, mic, local network
- [ ] CI: `.github/workflows/android-cam.yml` builds debug APK on push
- [ ] Optional: Fastlane `supply` with `PLAY_JSON` secret

## iOS (TestFlight)

- [ ] Apple Developer membership
- [ ] Bundle id `in.mayankafterhours.mahcam`
- [ ] Camera + mic + local network usage strings (Info.plist already)
- [ ] `xcodegen` → archive → upload
- [ ] TestFlight internal group

## What stays in git

- Source + unsigned CI artifacts
- Listing copy drafts
- **Not** keystores, API JSON, provisioning profiles
