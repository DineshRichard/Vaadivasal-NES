# Vaadivasal - automated test report

Produced by the test bot in this folder:

* `bot.c` - runs the real ROM headlessly in the same FCEUmm core the Android app uses, plays it with scripted /
  AI input and checks every ID from *Jallikattu game - Test cases.csv* by reading the game's own RAM variables, sprite
  buffer, framebuffer and audio. Run it with `build_and_run.sh` (needs the emulator running).
* `app_checks.py` - drives the installed Android app through adb and compares screenshots (touch controls, popups,
  pause-on-popup, background/foreground, rotation, 60 s of random touching).

Statuses: **PASS**; **DEVIATION** = works, but the number differs from the CSV; **FAIL** = defect.

## What was found and changed
1. **E02 - pause broke the sound effects (fixed in `src/main.s`).** Pausing wrote 0 to the APU enable register, which
   clears the hardware length counters; FamiStudio only reloads them when a note's period changes, so after any pause
   the hit sound had 35% of its normal energy. Pause now mutes by volume (pulse/noise `$30`, triangle `$80`) and leaves the
   channels enabled; hit sound after 1 / 10 / 300 pauses is 90% / 112% / 105% of a never-paused run.
2. **App lost the game in the background (fixed in the Android app).** The emulator was torn down with the screen
   surface, so any app switch restarted at the title. The core now lives for the activity's lifetime.
3. **P04 / P05 deviations (no change).** The player stops at x 5..228 and y 45..144; the CSV says 6..228 and 46..152.

## Caveats
* The full-playthrough bot caught only 6 of 25 bulls unassisted; for the other 19 it ended the chase by writing the
  bull's chase timer in RAM (reported in F04). Catch / score / speed / win logic is verified for all 25 bulls; human-level
  dodging is not.
* Real NES / Famiclone / PC (E05), and how the dodge phase feels to a human at score 20+ (D02), need manual play.
* START at Game Over / Win goes to the title first, so restarting takes two presses (CSV says one).

## 1. ROM before the fix

| ID | Result | Observed |
|---|---|---|
| F01 | PASS | boots to title (state=10); title music playing (song=0, audio=491861150); START begins game (state=7 lives=3 score=0); start jingle (song=1) |
| F02 | PASS | reached GAME OVER after 1140 frames (lives=0); SONG_GAMEOVER plays (audio=359110072) |
| F03 | PASS | START at Game Over -> title -> START restarts (lives=3 score=0). Note: needs 2 presses (returns to title first) |
| F04 | PASS | Win screen reached at score 25 (state=11); bot caught 6 of 25 bulls unassisted, 19 needed a RAM nudge to end the chase; SONG_WIN plays (audio=215542708) |
| F05 | PASS | START at Win -> title -> START restarts (lives=3 score=0). Note: needs 2 presses |
| P01 | PASS | LEFT x120 -> 110; legs animate through 3 frames while walking; RIGHT moves +5; UP moves -6; DOWN moves +6 |
| P02 | PASS | idle: walking=0 anim_frame=0 immediately |
| P03 | PASS | diagonal UP+LEFT: dx=-8 dy=-8 |
| P04 | DEVIATION | X stops at [5..228] and does not go past (CSV says exactly 6..228) |
| P05 | DEVIATION | Y stops at [45..144] and does not go past (CSV says exactly 46..152; WALL_MIN_Y/MAX_Y in constants.inc are 46/144) |
| P06 | PASS | moving RIGHT: 24 dust sprites near feet, left-foot dust=1, idle dust=0 |
| P07 | PASS | moving LEFT: 24 dust sprites near feet, right-foot dust=1 |
| B01 | PASS | spawn wait ended -> CHASE after 0 frames; bull closes in on the player (no retreat beyond 1px jitter): dist 128 -> 50 |
| B02 | PASS | bull_y=120 vs player feet target=120 |
| B03 | PASS | bull facing flag follows horizontal direction; bull sprite flip bit: facing left 99 ok/0 wrong, facing right 103 ok/0 wrong |
| B04 | PASS | bull stayed within x[31..187] y[64..143] (limits 16..200 / 72..216); across the whole game bull stayed in x[22..187] y[64..133] |
| B05 | PASS | bull kicks up exactly 2 dust sprites (front and back) on 89/180 chase frames (blinks), never 1 or 3+ |
| C01 | PASS | CHASE -> TIRED after 300 frames (~5.0s) |
| C02 | PASS | touching tired bull opens QTE after 6 frames; button prompt sprites visible (4) |
| C03 | PASS | 5/5 correct presses accepted; green check shown on 5/5; SONG_QTE_OK on presses 1-4 (4/4), SONG_CATCH on 5th (song=7); score 0 -> 1 after 5 QTE |
| C04 | PASS | wrong button: feedback state=1 song=5 cross sprites=1; bull escapes, score unchanged (0) |
| C05 | PASS | no input: failed after 61 frames (limit 60), cross sprites 3; bull escapes after timeout |
| C06 | PASS | bull runs away (y 139 -> 199), dust sprites seen 64; new bull spawns after escape |
| C07 | PASS | (checked below); no dust while bull is tired/static and player idle (0 seen); no dust during QTE (0 seen) |
| V01 | PASS | shake_timer set to 30 (= 0.5s) on hit; screen shakes: 2 distinct background frames over 29 shaking frames |
| V02 | PASS | hit flashing while facing RIGHT: 4 palette steps, player pixels changed 120 times |
| V03 | PASS | hit flashing while facing LEFT: 4 palette steps, player pixels changed 120 times |
| V04 | PASS | score 0 -> 1, HUD digits redrawn 0 frames later |
| V05 | PASS | lives 3 -> 2, heart redraw requested; heart removed from HUD 0 frames after hit |
| U01 | PASS | paused: state=9 ppu_mask=0F gray=100% (was 46%), sprites bit OFF; sprite layer disabled while paused |
| U02 | PASS | resumed: state=7 ppu_mask=1E gray=46% |
| U03 | PASS | paused: audio silent (peak sample 1 of 32767), music pointer frozen (9223 -> 9223); resume: music continues (9223 -> 9224 -> 9227, not restarted from 921D), audio energy 118403644 |
| U04 | PASS | shake timer 25, frozen while paused (25 -> 25, picture steady), resumes (14) |
| D01 | PASS | bull speed value at score 0/5/10/15/20 = 64/74/84/94/104 |
| D02 | PASS | score 24: bull speed 112 (cap 140), QTE 36 frames/button (min 25); catch phase completable at that speed. Whether the dodge phase is comfortable for a human at this speed needs manual play |
| D03 | PASS | QTE time per button 0/5/10/15/20/24 = 60/55/50/45/40/36 frames |
| E01 | PASS | mashed random buttons for 36000 frames: invalid states=0, frozen seconds=0, states visited mask=86B1; mashing non-START buttons on title keeps the title screen (state=10); mashing non-START buttons on Game Over keeps the screen (state=4) |
| E02 | FAIL | hit sound after 1 pause toggle(s): energy 6256884 = 35% of a never-paused run; hit sound after 10 pause toggle(s): energy 6257096 = 35% of a never-paused run; hit sound after 300 pause toggle(s): energy 6248570 = 35% of a never-paused run |
| E03 | PASS | top-left corner for 2 min: 52 state changes, 10 hits absorbed, bull out-of-bounds frames=0, player pos (6,46); top-right corner for 2 min: 52 state changes, 10 hits absorbed, bull out-of-bounds frames=0, player pos (228,46); bottom-left corner for 2 min: 52 state changes, 10 hits absorbed, bull out-of-bounds frames=0, player pos (6,144) |
| E04 | PASS | Konami code accepted, cheat_active=1; cheat on: took 5 hits standing still, lives=3, state=5 (no game over); whole run with cheat on: 38 hits, lives never dropped below 3 |
| E05 | PASS | emulator (this core) verified: ROM is NROM/mapper 0 with iNES header valid; real NES / Famiclone / PC need manual testing |

**PASS 38 / FAIL 1 / NOT RUN 0**


## 2. Final ROM (with the fix) - this is what ships in the app

| ID | Result | Observed |
|---|---|---|
| F01 | PASS | boots to title (state=10); title music playing (song=0, audio=491733946); START begins game (state=7 lives=3 score=0); start jingle (song=1) |
| F02 | PASS | reached GAME OVER after 1140 frames (lives=0); SONG_GAMEOVER plays (audio=359124540) |
| F03 | PASS | START at Game Over -> title -> START restarts (lives=3 score=0). Note: needs 2 presses (returns to title first) |
| F04 | PASS | Win screen reached at score 25 (state=11); bot caught 6 of 25 bulls unassisted, 19 needed a RAM nudge to end the chase; SONG_WIN plays (audio=215072530) |
| F05 | PASS | START at Win -> title -> START restarts (lives=3 score=0). Note: needs 2 presses |
| P01 | PASS | LEFT x120 -> 110; legs animate through 3 frames while walking; RIGHT moves +5; UP moves -6; DOWN moves +6 |
| P02 | PASS | idle: walking=0 anim_frame=0 immediately |
| P03 | PASS | diagonal UP+LEFT: dx=-8 dy=-8 |
| P04 | DEVIATION | X stops at [5..228] and does not go past (CSV says exactly 6..228) |
| P05 | DEVIATION | Y stops at [45..144] and does not go past (CSV says exactly 46..152; WALL_MIN_Y/MAX_Y in constants.inc are 46/144) |
| P06 | PASS | moving RIGHT: 24 dust sprites near feet, left-foot dust=1, idle dust=0 |
| P07 | PASS | moving LEFT: 24 dust sprites near feet, right-foot dust=1 |
| B01 | PASS | spawn wait ended -> CHASE after 0 frames; bull closes in on the player (no retreat beyond 1px jitter): dist 128 -> 50 |
| B02 | PASS | bull_y=120 vs player feet target=120 |
| B03 | PASS | bull facing flag follows horizontal direction; bull sprite flip bit: facing left 99 ok/0 wrong, facing right 103 ok/0 wrong |
| B04 | PASS | bull stayed within x[31..187] y[64..143] (limits 16..200 / 72..216); across the whole game bull stayed in x[22..187] y[64..133] |
| B05 | PASS | bull kicks up exactly 2 dust sprites (front and back) on 89/180 chase frames (blinks), never 1 or 3+ |
| C01 | PASS | CHASE -> TIRED after 300 frames (~5.0s) |
| C02 | PASS | touching tired bull opens QTE after 6 frames; button prompt sprites visible (4) |
| C03 | PASS | 5/5 correct presses accepted; green check shown on 5/5; SONG_QTE_OK on presses 1-4 (4/4), SONG_CATCH on 5th (song=7); score 0 -> 1 after 5 QTE |
| C04 | PASS | wrong button: feedback state=1 song=5 cross sprites=1; bull escapes, score unchanged (0) |
| C05 | PASS | no input: failed after 61 frames (limit 60), cross sprites 3; bull escapes after timeout |
| C06 | PASS | bull runs away (y 139 -> 199), dust sprites seen 64; new bull spawns after escape |
| C07 | PASS | (checked below); no dust while bull is tired/static and player idle (0 seen); no dust during QTE (0 seen) |
| V01 | PASS | shake_timer set to 30 (= 0.5s) on hit; screen shakes: 2 distinct background frames over 29 shaking frames |
| V02 | PASS | hit flashing while facing RIGHT: 4 palette steps, player pixels changed 120 times |
| V03 | PASS | hit flashing while facing LEFT: 4 palette steps, player pixels changed 120 times |
| V04 | PASS | score 0 -> 1, HUD digits redrawn 0 frames later |
| V05 | PASS | lives 3 -> 2, heart redraw requested; heart removed from HUD 0 frames after hit |
| U01 | PASS | paused: state=9 ppu_mask=0F gray=100% (was 46%), sprites bit OFF; sprite layer disabled while paused |
| U02 | PASS | resumed: state=7 ppu_mask=1E gray=46% |
| U03 | PASS | paused: audio silent (peak sample 2 of 32767), music pointer frozen (922E -> 922E); resume: music continues (922E -> 922F -> 9232, not restarted from 9228), audio energy 120983416 |
| U04 | PASS | shake timer 25, frozen while paused (25 -> 25, picture steady), resumes (14) |
| D01 | PASS | bull speed value at score 0/5/10/15/20 = 64/74/84/94/104 |
| D02 | PASS | score 24: bull speed 112 (cap 140), QTE 36 frames/button (min 25); catch phase completable at that speed. Whether the dodge phase is comfortable for a human at this speed needs manual play |
| D03 | PASS | QTE time per button 0/5/10/15/20/24 = 60/55/50/45/40/36 frames |
| E01 | PASS | mashed random buttons for 36000 frames: invalid states=0, frozen seconds=0, states visited mask=86B1; mashing non-START buttons on title keeps the title screen (state=10); mashing non-START buttons on Game Over keeps the screen (state=4) |
| E02 | PASS | 300 pause toggles: bad PPU masks=0 bad states=0, ends unpaused, mask=1E; hit sound after 1 pause toggle(s): energy 16000864 = 90% of a never-paused run; hit sound after 10 pause toggle(s): energy 19911984 = 112% of a never-paused run; hit sound after 300 pause toggle(s): energy 18665472 = 105% of a never-paused run |
| E03 | PASS | top-left corner for 2 min: 52 state changes, 10 hits absorbed, bull out-of-bounds frames=0, player pos (6,46); top-right corner for 2 min: 52 state changes, 10 hits absorbed, bull out-of-bounds frames=0, player pos (228,46); bottom-left corner for 2 min: 52 state changes, 10 hits absorbed, bull out-of-bounds frames=0, player pos (6,144) |
| E04 | PASS | Konami code accepted, cheat_active=1; cheat on: took 5 hits standing still, lives=3, state=5 (no game over); whole run with cheat on: 38 hits, lives never dropped below 3 |
| E05 | PASS | emulator (this core) verified: ROM is NROM/mapper 0 with iNES header valid; real NES / Famiclone / PC need manual testing |

**PASS 39 / FAIL 0 / NOT RUN 0**


## 3. Android app checks (final build)

```
device 1080x2400
== A1 launch ==
  [ok  ] app process running after launch
  [ok  ] no crash in the log
== A2 orientation ==
  [ok  ] portrait layout (1080x2400)
== A3 title screen -> START (touch) -> game ==
  [ok  ] touching START replaced the title with the arena (91% of the picture changed)
== A4 D-pad moves the player ==
  [ok  ] holding LEFT moved the player (4.9% of picture changed)
  [ok  ] holding RIGHT moved the player back (4.8% of picture changed)
  [ok  ] D-pad shows the pressed state while held (1.15% of control area changed)
== A5 Help / Credits popups ==
  [ok  ] Help popup appears (60% of screen changed)
  [ok  ] Help popup closes with Back
  [ok  ] Credits popup appears (61% changed)
  [ok  ] Credits shows different text from Help
== A6 popup pauses the game ==
  [ok  ] picture frozen while a popup is open (0.00% changed)
  [ok  ] game running again after closing the popup (2.37% changed)
== A7 background / foreground keeps the game ==
  [ok  ] process survives going to the background
  [ok  ] no crash after returning
  [ok  ] still in the arena, not back at the title screen (90% differs from the title)
  [ok  ] picture looks like where the player left it (3% differs)
  [ok  ] game is running after returning
== A8 rotation ==
  [ok  ] app alive after rotating the device and back
  [ok  ] returns to portrait layout
== A9 two-finger touch (D-pad + A) ==
  [ok  ] simultaneous touches handled without crash
== A10 stability: 60 seconds of random touches ==
  [ok  ] survived 189 random touches

APP CHECKS: pass=22 fail=0
```
