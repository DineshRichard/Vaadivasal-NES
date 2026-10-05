# Vaadivasal (வாடிவாசல்) - Android app

Plays the Tamil 8-bit game *Vaadivasal* on Android phones: the game ROM runs inside the FCEUmm NES emulator core, with
on-screen touch controls, a pause popup system and Help / Credits screens.

* Portrait, touch D-pad + A / B / Select / Start (gamepads work too)
* No ads, no analytics, no network access, no permissions - see [Privacy policy](../docs/privacy-policy.md)
* Game state is kept when you switch apps or take a call

## Build

```
cd android
./gradlew assembleDebug        # debug APK
./gradlew bundleRelease        # Play Store bundle (needs keystore.properties, see below)
```

Release signing reads `keystore.properties` (git-ignored):

```
storeFile=vaadivasal-upload.jks
storePassword=...
keyAlias=upload
keyPassword=...
```

## Test bot

`bot/` contains an automated tester that plays the real ROM in the same emulator core and checks every case in the
test-case sheet, plus Android-level checks driven through adb. See [`bot/TEST_REPORT.md`](bot/TEST_REPORT.md).

```
bot/build_and_run.sh        # ROM tests (needs the emulator running)
python bot/app_checks.py    # installed-app checks
```

## Store listing material

`store/` has the Play Store icon, feature graphic, screenshots and the English / Tamil listing text.

## Licence

GPL v2 for the app (it includes the GPL-licensed FCEUmm core); MIT for the game itself. Details and credits in
[`NOTICE.md`](NOTICE.md).
