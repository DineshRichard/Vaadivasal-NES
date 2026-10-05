#!/bin/bash
# Builds the test bot with the Android NDK (x86_64) and runs it on the emulator via adb.
set -e
export MSYS_NO_PATHCONV=1  # keep adb from rewriting /data/local/tmp
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
SDK="${ANDROID_SDK:-/c/Users/Admin/AppData/Local/Android/Sdk}"
CC="$SDK/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/x86_64-linux-android24-clang"
ADB="$SDK/platform-tools/adb.exe"
cd "$HERE/.."        # android/ - relative paths below avoid the space in the folder name
CORE="app/src/main/cpp/fceumm/src"
LRC="$CORE/drivers/libretro/libretro-common"
OUT="bot/out"; mkdir -p "$OUT"
HW="$(cygpath -m "$HERE")"; RW="$(cygpath -m "$ROOT")"   # Windows paths for adb.exe

SRC=( bot/bot.c $CORE/boards/*.c $CORE/input/*.c
  $CORE/drivers/libretro/libretro.c $CORE/drivers/libretro/libretro_dipswitch.c
  $CORE/{cart,cheat,crc32,fceu-endian,fceu-memory,fceu,fds,fds_apu,file,filter,general,input,md5,nsf,palette,ppu,sound,state,video,vsuni,ines,unif,x6502}.c
  $CORE/ntsc/nes_ntsc.c
  $LRC/streams/memory_stream.c $LRC/compat/compat_posix_string.c $LRC/compat/compat_snprintf.c
  $LRC/compat/compat_strcasestr.c $LRC/compat/compat_strl.c $LRC/compat/fopen_utf8.c
  $LRC/encodings/encoding_utf.c $LRC/file/file_path.c $LRC/file/file_path_io.c
  $LRC/streams/file_stream.c $LRC/streams/file_stream_transforms.c $LRC/string/stdstring.c
  $LRC/time/rtime.c $LRC/vfs/vfs_implementation.c )

"$CC" -O2 -w -D__LIBRETRO__ -DPATH_MAX=1024 -DFCEU_VERSION_NUMERIC=9900 -DFRONTEND_SUPPORTS_RGB565 \
  -DHAVE_NTSC_FILTER -DPSS_STYLE=1 \
  -I"$CORE/drivers/libretro" -I"$LRC/include" -I"$CORE" -I"$CORE/input" -I"$CORE/boards" -I"$CORE/ntsc" \
  "${SRC[@]}" -o "$OUT/vaadivasal_bot" -lm

"$ADB" push "$HW/out/vaadivasal_bot" /data/local/tmp/vaadivasal_bot >/dev/null
"$ADB" push "${ROMDIR:-$RW/build}/Vaadivasal.nes" /data/local/tmp/Vaadivasal.nes >/dev/null
"$ADB" push "${ROMDIR:-$RW/build}/labels.txt" /data/local/tmp/labels.txt >/dev/null
"$ADB" shell chmod +x /data/local/tmp/vaadivasal_bot
"$ADB" shell "cd /data/local/tmp && ./vaadivasal_bot Vaadivasal.nes labels.txt report.md $BOT_ARGS" | tee "$HERE/out/run.log"
"$ADB" pull /data/local/tmp/report.md "$HW/out/report.md" >/dev/null
