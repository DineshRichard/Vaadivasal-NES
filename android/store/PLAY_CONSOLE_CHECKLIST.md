# Play Console checklist - Vaadivasal

Everything here is based on what the app really does (verified from the built APK: **0 permissions**, no third-party
libraries, no network code). Answer the forms yourself in Play Console - these are suggested answers.
Google changes its forms and rules from time to time, so read each page as you go.

## 0. Before you start
- [ ] Push this repo to GitHub, then enable **Settings -> Pages -> Deploy from a branch -> `main` / `/docs`**.
      The privacy policy will then be live at
      `https://dineshrichard.github.io/Vaadivasal-NES/privacy-policy.html` (check the link opens before submitting).
- [ ] Back up `android/vaadivasal-upload.jks` + `android/keystore.properties` somewhere safe (they are git-ignored).
- [ ] Have a Play developer account with identity verification complete.

## 1. Create the app
- App name: **Vaadivasal - Tamil 8-Bit Game** (see `listing_en.md`) - Default language: English (add Tamil as a
  translation using `listing_ta.md`)
- App or game: **Game** - Free
- Accept the declarations. When asked, **enrol in Play App Signing** (default) and upload `app-release.aab`; your
  `vaadivasal-upload.jks` is the *upload* key.

## 2. App content forms (suggested answers)
| Form | Suggested answer |
|---|---|
| Privacy policy | the GitHub Pages URL above |
| Ads | **No ads** |
| App access | All functionality available without sign-in or special access |
| Content rating (IARC questionnaire) | Category *Game*. Answer honestly: no gambling, no user-generated content, no chat, no location sharing, no purchases. Violence: the game is a pixel-art bull-taming sport - nobody and nothing is shown being injured and there is no blood; if a question asks about violence involving animals or fantasy violence, read it carefully and answer as it applies. Expect a low rating (everyone / PEGI 3-7 range) |
| Target audience | Suggest **13+** (or 18+ not needed). Choosing under-13 pulls in the Families policy requirements, which you don't need |
| Data safety | **No data collected, no data shared.** The app has no permissions and no network access. Encryption/deletion questions do not apply |
| Advertising ID | **No** (the app doesn't use it) |
| Government / financial / health / news app | **No** to all |
| Data deletion | Not applicable (no data) |

## 3. Store listing
- Short + full description: `listing_en.md` (English), `listing_ta.md` (Tamil - get a Tamil speaker to proofread)
- App icon (512x512): `play_icon_512.png`
- Feature graphic (1024x500): `feature_graphic_1024x500.png`
- Phone screenshots (1080x2160, 2:1): `screenshots/phone_1.png` ... `phone_6.png`
- Category: **Games -> Arcade**; tags e.g. retro, pixel art, arcade, offline
- Contact: your public email address, website `https://github.com/DineshRichard/Vaadivasal-NES`

## 4. Release
1. **Testing -> Internal testing -> Create release**: upload `android/app/build/outputs/bundle/release/app-release.aab`
   (build it with `./gradlew bundleRelease`). Add yourself as a tester and install from the Play link.
2. Check the pre-launch report Play generates for you.
3. If your developer account is a **new personal account**, Google may require a **closed test with at least 12
   testers for 14 days** before you can request production access - check *Dashboard* in Play Console for your
   account's current requirement and start this early, it is the longest wait.
4. When allowed: **Production -> Create release** with the same bundle, then submit for review.

## 5. After release
- Keep every release signed with the same upload key; bump `versionCode` in `android/app/build.gradle` for each
  new upload.
- The app is GPL v2 (it includes the GPL-licensed FCEUmm core): keep `android/NOTICE.md` and the source on GitHub in
  step with what you publish.
