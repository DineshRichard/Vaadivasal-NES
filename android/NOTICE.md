# Vaadivasal for Android - licences and credits

The Android app combines three things:

| Part | Where | Licence |
|---|---|---|
| **Vaadivasal** (the game: ROM, graphics, music, sound) | `../src`, `../assets`, `app/src/main/assets/vaadivasal.nes` | MIT, Copyright (c) 2025 Dinesh Richard - see `../LICENSE` |
| **FCEUmm** NES emulator core (libretro-fceumm) | `app/src/main/cpp/fceumm` | **GNU GPL v2** (source files say "version 2, or at your option any later version") - see `licenses/FCEUmm-GPL-2.0.txt` |
| **Android app glue** (touch controls, audio/video bridge, popups) | `app/src/main/java`, `app/src/main/cpp/native-lib.c` | GNU GPL v2 (it is linked into one program with FCEUmm) |

Because the app is a single program that includes the GPL-licensed emulator core, **the complete corresponding source
code of the Android app is published at https://github.com/DineshRichard/Vaadivasal-NES** (this `android/` folder plus
the game source). You are free to build, modify and redistribute the app under the terms of the GPL v2.

The game itself stays MIT-licensed: you may also use the game ROM and assets on their own under the MIT licence.

## Third-party components

* **FCEUmm / libretro-fceumm** - upstream https://github.com/libretro/libretro-fceumm , commit
  `7a542dab1e87679921962a9f056186eca425c0c2` (2026-09-26), included unmodified under
  `app/src/main/cpp/fceumm`. Based on FCE Ultra and contributions listed in `app/src/main/cpp/fceumm/Authors`.
* **libretro API header** (`libretro.h`, in `libretro-common`) - MIT licence, as stated in the header.
* **FamiStudio sound engine** (`src/famistudio_ca65.s`, inside the game ROM) - Copyright (c) 2019-2025 Mathieu Gauthier.
  Permissive licence: copying and distribution, with or without modification, are permitted provided the copyright
  notice is preserved (see the header of that file).
* **cc65** (used to build the ROM) - zlib licence; its output is not subject to it.

## How to rebuild

1. Game ROM: `tools/build.bat` (needs cc65) produces `build/Vaadivasal.nes`; copy it to
   `android/app/src/main/assets/vaadivasal.nes`.
2. Android app: install Android Studio (SDK 36, NDK 28.2) and run `./gradlew assembleRelease` in `android/`.

Nintendo, NES and Famicom are trademarks of Nintendo Co., Ltd. Vaadivasal is an independent homebrew project and is
not affiliated with or endorsed by Nintendo.
