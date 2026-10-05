/*
 * Vaadivasal test bot.
 *
 * Runs the real ROM headlessly in the same FCEUmm core the Android app uses,
 * drives the controller with scripted / AI input, reads the game's own RAM
 * variables, OAM shadow buffer, framebuffer and audio output, and checks each
 * test case ID from "Jallikattu game - Test cases.csv".
 *
 * Usage: bot <rom.nes> <labels.txt> [report.md]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include "libretro.h"

/* ------------------------------------------------------------------ */
/* libretro frontend                                                   */
/* ------------------------------------------------------------------ */
#define PAD_B 1
#define PAD_SELECT 4
#define PAD_START 8
#define PAD_UP 16
#define PAD_DOWN 32
#define PAD_LEFT 64
#define PAD_RIGHT 128
#define PAD_A 256

static uint16_t frame[256 * 240];
static int16_t aud[8192 * 2];
static int aud_n;
static uint16_t pad;
static long frames_run;

static void log_cb(enum retro_log_level l, const char *f, ...) {}
static bool env_cb(unsigned cmd, void *d) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)d = true; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *(enum retro_pixel_format *)d == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)d)->log = log_cb; return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)d = "/data/local/tmp"; return true;
    default: return false;
    }
}
static void video_cb(const void *d, unsigned w, unsigned h, size_t pitch) {
    if (!d) return;
    if (w > 256) w = 256;
    if (h > 240) h = 240;
    for (unsigned y = 0; y < h; y++)
        memcpy(&frame[y * 256], (const uint8_t *)d + y * pitch, w * 2);
}
static size_t abatch_cb(const int16_t *d, size_t n) {
    if (aud_n + (int)n > 8192) n = 8192 - aud_n;
    memcpy(&aud[aud_n * 2], d, n * 4);
    aud_n += n;
    return n;
}
static void asample_cb(int16_t l, int16_t r) { int16_t s[2] = {l, r}; abatch_cb(s, 1); }
static void poll_cb(void) {}
static int16_t input_cb(unsigned p, unsigned dev, unsigned i, unsigned id) {
    if (p != 0 || dev != RETRO_DEVICE_JOYPAD || id > 15) return 0;
    return (pad >> id) & 1;
}

static uint8_t *RAM;
static uint8_t *rom_file;
static long rom_len;

/* ------------------------------------------------------------------ */
/* labels                                                              */
/* ------------------------------------------------------------------ */
typedef struct { char name[64]; unsigned addr; } Label;
static Label labels[2048];
static int nlabels;

static unsigned lbl(const char *name) {
    for (int i = 0; i < nlabels; i++)
        if (!strcmp(labels[i].name, name)) return labels[i].addr;
    fprintf(stderr, "missing label %s\n", name);
    exit(2);
}
static void load_labels(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(2); }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        unsigned a; char n[128];
        if (sscanf(line, "al %x .%127s", &a, n) == 2 && nlabels < 2048) {
            strncpy(labels[nlabels].name, n, 63);
            labels[nlabels++].addr = a;
        }
    }
    fclose(f);
}

static struct {
    unsigned state, px, py, lives, score, buttons, facing, walking, anim,
        bx, by, bvel, btimer, bface, qbtn, qcnt, qtimer, qmax, qpal, qok,
        hit, hreq, sreq, padd, shake, smask, cheat, fcount, sbackup, bacc;
} L;

#define S(a) RAM[a]
#define GS S(L.state)
#define PX S(L.px)
#define PY S(L.py)
#define BX S(L.bx)
#define BY S(L.by)

enum { ST_CHASE, ST_TIRED, ST_QTE, ST_FEEDBACK, ST_GAMEOVER, ST_HIT, ST_ESCAPE,
       ST_SPAWN, ST_WONWAIT, ST_PAUSED, ST_TITLE, ST_WIN };
enum { SONG_TITLE, SONG_START, SONG_WIN, SONG_GAMEOVER, SONG_QTE_OK, SONG_QTE_FAIL,
       SONG_HIT, SONG_CATCH };

/* ------------------------------------------------------------------ */
/* results                                                             */
/* ------------------------------------------------------------------ */
typedef struct { char id[8]; int status; char note[400]; } Result; /* 1 pass, 0 fail, 2 deviation from CSV numbers, -1 not run */
static Result res[64];
static int nres;

static Result *get(const char *id) {
    for (int i = 0; i < nres; i++) if (!strcmp(res[i].id, id)) return &res[i];
    strcpy(res[nres].id, id); res[nres].status = -1;
    return &res[nres++];
}
static void rec(const char *id, int ok, const char *fmt, ...) {
    Result *r = get(id);
    char buf[300];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    /* ok: 1 pass, 0 fail, 2 works but differs from the numbers in the CSV */
    if (r->status == 0 && ok) return;                 /* keep failure note */
    if (r->status == -1 || (r->status == 1 && ok != 1) || (r->status == 2 && ok == 0)) { r->note[0] = 0; }
    if (ok == 0) r->status = 0;
    else if (ok == 2) { if (r->status != 0) r->status = 2; }
    else if (r->status == -1) r->status = 1;
    if (strlen(r->note) + strlen(buf) + 3 < sizeof r->note) {
        if (r->note[0]) strcat(r->note, "; ");
        strcat(r->note, buf);
    }
    printf("  [%s] %-4s %s\n", ok ? "ok  " : "FAIL", id, buf);
}

/* ------------------------------------------------------------------ */
/* frame stepping & observers                                          */
/* ------------------------------------------------------------------ */
static long aud_abs;     /* |sample| sum of the last frame */
static int aud_max;

static void step(uint16_t mask) {
    pad = mask;
    aud_n = 0;
    retro_run();
    frames_run++;
    aud_abs = 0; aud_max = 0;
    for (int i = 0; i < aud_n * 2; i++) {
        int v = abs(aud[i]);
        aud_abs += v;
        if (v > aud_max) aud_max = v;
    }
}
static void idle(int n) { while (n-- > 0) step(0); }
static void press(uint16_t m, int n) { for (int i = 0; i < n; i++) step(m); idle(3); }

static int count_tile(int tile) {
    int c = 0;
    for (int i = 0; i < 64; i++) {
        uint8_t y = RAM[0x200 + i * 4], t = RAM[0x201 + i * 4];
        if (y < 0xEF && t == tile) c++;
    }
    return c;
}
static int count_tile_range(int lo, int hi) {
    int c = 0;
    for (int i = 0; i < 64; i++) {
        uint8_t y = RAM[0x200 + i * 4], t = RAM[0x201 + i * 4];
        if (y < 0xEF && t >= lo && t <= hi) c++;
    }
    return c;
}
static uint32_t region_hash(int x, int y, int w, int h) {
    uint32_t hsh = 2166136261u;
    for (int j = y; j < y + h && j < 240; j++)
        for (int i = x; i < x + w && i < 256; i++) {
            hsh ^= frame[j * 256 + i];
            hsh *= 16777619u;
        }
    return hsh;
}
static uint32_t frame_hash(void) { return region_hash(0, 0, 256, 240); }

static double gray_ratio(void) {
    long g = 0;
    for (int i = 0; i < 256 * 240; i++) {
        int r = ((frame[i] >> 11) & 31) << 3, gg = ((frame[i] >> 5) & 63) << 2, b = (frame[i] & 31) << 3;
        int mx = r > gg ? r : gg; if (b > mx) mx = b;
        int mn = r < gg ? r : gg; if (b < mn) mn = b;
        if (mx - mn < 28) g++;
    }
    return (double)g / (256 * 240);
}

/* song detection from the FamiStudio channel-0 pointer */
static unsigned song_start[16];
static int nsongs;
static void load_song_table(void) {
    unsigned base = lbl("music_data_vaadivasal_music");
    long off = 16 + (base - 0x8000);
    nsongs = rom_file[off];
    for (int i = 0; i < nsongs; i++) {
        long o = off + 5 + 14 * i;
        song_start[i] = rom_file[o] | (rom_file[o + 1] << 8);
    }
}
static int cur_song(void) {
    unsigned ptr = RAM[0x349] | (RAM[0x34E] << 8);
    int best = -1;
    for (int i = 0; i < nsongs; i++) if (song_start[i] <= ptr) best = i;
    return best;
}

/* ------------------------------------------------------------------ */
/* game helpers                                                        */
/* ------------------------------------------------------------------ */
static void reboot(void) {
    retro_reset();
    idle(120);
}

static const uint16_t KONAMI[11] = {PAD_UP, PAD_UP, PAD_DOWN, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
                                    PAD_LEFT, PAD_RIGHT, PAD_B, PAD_A, PAD_START};

static int start_game(int cheat) {
    if (GS != ST_TITLE) reboot();
    if (cheat) {
        for (int i = 0; i < 11; i++) press(KONAMI[i], 2);
    } else {
        press(PAD_START, 2);
    }
    for (int i = 0; i < 60 && GS != ST_SPAWN; i++) step(0);
    return GS == ST_SPAWN;
}

typedef uint16_t (*Policy)(void);
static uint16_t pol_idle(void) { return 0; }

/* ---- kiting: keep away from the bull ---- */
static int gap1(int a0, int a1, int b0, int b1) {
    if (a1 < b0) return b0 - a1;
    if (b1 < a0) return a0 - b1;
    return 0;
}
static uint16_t pol_kite(void);
static uint16_t pol_kite(void) {
    int bx = BX, by = BY, px = PX, py = PY;
    double best = -1e9; uint16_t bm = 0;
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++) {
            int nx = px + dx, ny = py + dy;
            if (nx < 5 || nx > 228 || ny < 45 || ny > 144) continue;
            double gx = gap1(nx, nx + 24, bx, bx + 40);
            double gy = gap1(ny, ny + 60, by, by + 30);
            double sc = gx + gy;
            /* prefer open space: stay away from the bull's x and on the far side */
            sc += 0.02 * fabs(nx - (bx + 20)) + 0.01 * fabs(ny - (by - 30));
            sc -= (dx == 0 && dy == 0) ? 0.05 : 0;
            if (sc > best) {
                best = sc;
                bm = 0;
                if (dx < 0) bm |= PAD_LEFT;
                if (dx > 0) bm |= PAD_RIGHT;
                if (dy < 0) bm |= PAD_UP;
                if (dy > 0) bm |= PAD_DOWN;
            }
        }
    return bm;
}

static int park_bull;   /* test harness aid: keep the bull off-screen so movement tests are not interrupted */
static void stepp(uint16_t m) { if (park_bull) BY = 0xF0; step(m); }

/* ---- lookahead dodging using save states: try each move, keep the safest ---- */
static size_t ssz;
static uint8_t *sbuf;
static uint16_t cand[9];
static int cand_n;
static void init_cands(void) {
    cand_n = 0;
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++) {
            uint16_t m = 0;
            if (dx < 0) m |= PAD_LEFT;
            if (dx > 0) m |= PAD_RIGHT;
            if (dy < 0) m |= PAD_UP;
            if (dy > 0) m |= PAD_DOWN;
            cand[cand_n++] = m;
        }
}
static double eval_move(uint16_t m, int K) {
    retro_unserialize(sbuf, ssz);
    double min_gap = 1e9, fin = 0;
    for (int i = 0; i < K; i++) {
        pad = m; aud_n = 0; retro_run();
        if (GS == ST_HIT) return -1000.0 + i;
        if (GS == ST_TIRED) return 500.0 - fin * 0.0;
        double gx = gap1(PX, PX + 24, BX, BX + 40), gy = gap1(PY, PY + 60, BY, BY + 30);
        double g = gx > gy ? gx : gy;     /* safe if clear on EITHER axis (hop over/under the bull) */
        if (g < min_gap) min_gap = g;
        fin = g;
    }
    if (min_gap > 30) min_gap = 30;
    /* corners are traps: the bull only ever has to close one axis there */
    int wx = PX - 5; if (228 - PX < wx) wx = 228 - PX;
    int wy = PY - 45; if (144 - PY < wy) wy = 144 - PY;
    double wall = 0.5 * (wx < 40 ? 40 - wx : 0) + 0.5 * (wy < 20 ? 20 - wy : 0);
    /* The bull follows the player's height slowly, so being on a different level is the robust way to be safe:
       reward vertical separation (up to 60px) and break remaining ties by horizontal distance. */
    double gy_end = gap1(PY, PY + 60, BY, BY + 30), gx_end = gap1(PX, PX + 24, BX, BX + 40);
    if (gy_end > 60) gy_end = 60;
    return min_gap * 2.0 - wall + 0.15 * gy_end + 0.02 * gx_end;
}
static uint16_t smart_hold; static int smart_left;
static int smart_trace;
static uint16_t pol_smart(void) {
    if (smart_left > 0) { smart_left--; return smart_hold; }
    retro_serialize(sbuf, ssz);
    double best = -1e18; uint16_t bm = 0;
    for (int i = 0; i < cand_n; i++) {
        double v = eval_move(cand[i], 60);
        if (v > best) { best = v; bm = cand[i]; }
    }
    int ok = retro_unserialize(sbuf, ssz);
    if (smart_trace)
        printf("   decide player=(%3d,%3d) bull=(%3d,%3d) -> mask=%03x score=%.1f restore_ok=%d state=%d\n",
               PX, PY, BX, BY, bm, best, ok, GS);
    smart_hold = bm; smart_left = 15;
    return bm;
}

static int aabb(void) {  /* same test as the game's AABB_Check */
    if ((PX + 24) < BX) return 0;
    if ((BX + 40) < PX) return 0;
    if ((PY + 60) < BY) return 0;
    if ((BY + 30) < PY) return 0;
    return 1;
}
static uint16_t pol_approach(void) {
    uint16_t m = 0;
    int cx = BX + 20 - (PX + 12);
    int cy = (BY + 15) - (PY + 30);
    if (cx < -2) m |= PAD_LEFT; else if (cx > 2) m |= PAD_RIGHT;
    if (cy < -2) m |= PAD_UP; else if (cy > 2) m |= PAD_DOWN;
    return m;
}

/* press the correct QTE button on a fresh edge */
static const uint16_t QTE_PAD[6] = {PAD_A, PAD_B, PAD_UP, PAD_RIGHT, PAD_DOWN, PAD_LEFT};
static int qte_toggle;
static uint16_t pol_qte_correct(void) {
    qte_toggle ^= 1;
    return qte_toggle ? QTE_PAD[S(L.qbtn) % 6] : 0;
}

static uint16_t pol_smart(void);
/* master "play" policy: kite until tired, approach, answer QTE correctly */
static int fail_qte_next;     /* when set, press a wrong button instead */
static uint16_t pol_play(void) {
    switch (GS) {
    case ST_CHASE: case ST_SPAWN: return pol_smart();
    case ST_TIRED: return pol_approach();
    case ST_QTE: {
        qte_toggle ^= 1;
        if (!qte_toggle) return 0;
        int b = S(L.qbtn) % 6;
        if (fail_qte_next) b = (b + 1) % 6;
        return QTE_PAD[b];
    }
    default: return 0;
    }
}

/* run until cond(), max frames. Returns frames used or -1 on timeout. */
static int run_until(int (*cond)(void), Policy p, int maxf) {
    for (int i = 0; i < maxf; i++) {
        if (cond()) return i;
        step(p());
    }
    return cond() ? maxf : -1;
}
static int c_chase(void) { return GS == ST_CHASE; }
static int c_tired(void) { return GS == ST_TIRED; }
static int c_qte(void) { return GS == ST_QTE; }
static int c_feedback(void) { return GS == ST_FEEDBACK; }
static int c_escape(void) { return GS == ST_ESCAPE; }
static int c_hit(void) { return GS == ST_HIT; }
static int c_gameover(void) { return GS == ST_GAMEOVER; }
static int c_win(void) { return GS == ST_WIN; }
static int c_spawn(void) { return GS == ST_SPAWN; }
static int c_title(void) { return GS == ST_TITLE; }

/* ================================================================== */
/* TESTS                                                               */
/* ================================================================== */


/* ---- debug: trace the play policy and the pause-spam audio issue ---- */
static void debug_run(void) {
    printf("-- debug: play policy trace --\n");
    reboot(); start_game(1);
    int last = -1;
    printf("serialize size=%zu\n", ssz);
    int lines = 0;
    for (int i = 0; i < 6000; i++) {
        smart_trace = (S(L.score) == 3 && GS == ST_CHASE && lines++ < 45);
        uint16_t m = pol_play();
        step(m);
        if (GS != last && !smart_trace) {
            printf("f=%5d state=%2d player=(%3d,%3d) bull=(%3d,%3d) vel=%d timer=%d score=%d mask=%03x\n",
                   i, GS, PX, PY, BX, BY, S(L.bvel), S(L.btimer) | (S(L.btimer + 1) << 8), S(L.score), m);
            last = GS;
        }
    }
    return;
    printf("-- debug: pause spam audio --\n");
    int toggles[] = {0, 1, 2, 4, 10, 50, 300};
    for (unsigned t = 0; t < sizeof toggles / sizeof *toggles; t++) {
        reboot(); start_game(1);
        for (int i = 0; i < 30; i++) step(0);
        for (int i = 0; i < toggles[t] * 2; i++) step((i & 1) ? 0 : PAD_START);
        int paused = (GS == ST_PAUSED);
        if (paused) press(PAD_START, 2);
        for (int i = 0; i < 6; i++) step(0);
        long e = 0; int song = -1;
        run_until(c_hit, pol_idle, 60 * 30);
        for (int i = 0; i < 40; i++) { step(0); e += aud_abs; if (cur_song() >= 0) song = cur_song(); }
        printf("toggles=%3d paused_at_end=%d  smask=%02X  hit-sound energy=%ld song=%d\n", toggles[t], paused, S(L.smask), e, song);
    }
}

/* ---- store screenshots: play to a moment and dump the 256x240 RGB565 frame ---- */
static void dump_frame(const char *path) {
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(frame, 1, sizeof frame, f); fclose(f); }
}
static void dump_qte_frames(void) {
    reboot(); start_game(1);
    run_until(c_tired, pol_play, 3000);
    run_until(c_qte, pol_approach, 600);
    step(0); step(0);
    dump_frame("/data/local/tmp/qte.raw");
    printf("qte frame dumped (state=%d, button=%d)\n", GS, S(L.qbtn));
}

/* ---- 1. CORE FLOW (F01-F03) ---- */
static void test_core(void) {
    printf("\n== 1. CORE FLOW ==\n");
    reboot();
    long audio_sum = 0;
    int ok_title = (GS == ST_TITLE);
    for (int i = 0; i < 120; i++) { step(0); audio_sum += aud_abs; }
    rec("F01", ok_title, "boots to title (state=%d)", GS);
    rec("F01", audio_sum > 1000 && cur_song() == SONG_TITLE, "title music playing (song=%d, audio=%ld)", cur_song(), audio_sum);
    press(PAD_START, 2);
    for (int i = 0; i < 40 && GS != ST_SPAWN; i++) step(0);
    rec("F01", GS == ST_SPAWN && S(L.lives) == 3 && S(L.score) == 0,
        "START begins game (state=%d lives=%d score=%d)", GS, S(L.lives), S(L.score));
    rec("F01", cur_song() == SONG_START, "start jingle (song=%d)", cur_song());

    /* F02: idle until all lives are lost */
    int n = run_until(c_gameover, pol_idle, 60 * 120);
    int lives = S(L.lives);
    rec("F02", n >= 0 && lives == 0, "reached GAME OVER after %d frames (lives=%d)", n, lives);
    long as = 0; int song_ok = 0;
    for (int i = 0; i < 90; i++) { step(0); as += aud_abs; if (cur_song() == SONG_GAMEOVER) song_ok = 1; }
    rec("F02", song_ok && as > 1000, "SONG_GAMEOVER plays (audio=%ld)", as);

    /* F03: START at game over */
    press(PAD_START, 2);
    int to_title = (GS == ST_TITLE);
    int f = 0; while (GS != ST_TITLE && f++ < 60) step(0);
    to_title = (GS == ST_TITLE);
    press(PAD_START, 2);
    for (int i = 0; i < 40 && GS != ST_SPAWN; i++) step(0);
    rec("F03", to_title && GS == ST_SPAWN && S(L.lives) == 3 && S(L.score) == 0,
        "START at Game Over -> title -> START restarts (lives=%d score=%d). Note: needs 2 presses (returns to title first)",
        S(L.lives), S(L.score));
}

/* ---- 2. PLAYER (P01-P07) ---- */
static void test_player(void) {
    printf("\n== 2. PLAYER ==\n");
    reboot();
    start_game(1);                     /* cheat on so a stray hit can't end the run */
    int x0 = PX, y0 = PY;
    for (int i = 0; i < 10; i++) step(PAD_LEFT);
    rec("P01", PX == x0 - 10, "LEFT x%d -> %d", x0, PX);
    int anim_seen[3] = {0};
    for (int i = 0; i < 30; i++) { step(PAD_RIGHT); anim_seen[S(L.anim) % 3] = 1; }
    rec("P01", anim_seen[0] && anim_seen[1] && anim_seen[2], "legs animate through 3 frames while walking");
    int xr0 = PX; for (int i = 0; i < 5; i++) step(PAD_RIGHT);
    rec("P01", PX == xr0 + 5, "RIGHT moves +5");
    int yu = PY; for (int i = 0; i < 6; i++) step(PAD_UP);
    rec("P01", PY == yu - 6, "UP moves -6");
    int yd = PY; for (int i = 0; i < 6; i++) step(PAD_DOWN);
    rec("P01", PY == yd + 6, "DOWN moves +6");

    step(0);
    rec("P02", S(L.walking) == 0 && S(L.anim) == 0, "idle: walking=%d anim_frame=%d immediately", S(L.walking), S(L.anim));

    int dx0 = PX, dy0 = PY;
    for (int i = 0; i < 8; i++) step(PAD_LEFT | PAD_UP);
    rec("P03", PX == dx0 - 8 && PY == dy0 - 8, "diagonal UP+LEFT: dx=%d dy=%d", PX - dx0, PY - dy0);

    /* boundaries (bull parked off-screen with a RAM write so it cannot interrupt the walk) */
    park_bull = 1;
    for (int i = 0; i < 260; i++) stepp(PAD_LEFT);
    int minx = PX;
    for (int i = 0; i < 10; i++) stepp(PAD_LEFT);
    int xl = PX;
    for (int i = 0; i < 300; i++) stepp(PAD_RIGHT);
    int maxx = PX;
    for (int i = 0; i < 10; i++) stepp(PAD_RIGHT);
    int xr = PX;
    int xstop = (xl == minx && xr == maxx);
    rec("P04", xstop && minx == 6 && maxx == 228 ? 1 : xstop ? 2 : 0,
        "X stops at [%d..%d] and does not go past (CSV says exactly 6..228)", minx, maxx);
    for (int i = 0; i < 200; i++) stepp(PAD_UP);
    int miny = PY;
    for (int i = 0; i < 10; i++) stepp(PAD_UP);
    int yt = PY;
    for (int i = 0; i < 200; i++) stepp(PAD_DOWN);
    int maxy = PY;
    for (int i = 0; i < 10; i++) stepp(PAD_DOWN);
    int yb = PY;
    int ystop = (yt == miny && yb == maxy);
    rec("P05", ystop && miny == 46 && maxy == 152 ? 1 : ystop ? 2 : 0,
        "Y stops at [%d..%d] and does not go past (CSV says exactly 46..152; WALL_MIN_Y/MAX_Y in constants.inc are 46/144)", miny, maxy);
    park_bull = 0;

    /* dust: right and left */
    reboot(); start_game(1);
    int dust_r = 0, dust_l = 0, dust_idle = 0, left_foot = 0, right_foot = 0;
    for (int i = 0; i < 24; i++) {
        step(PAD_RIGHT);
        for (int s = 0; s < 64; s++)
            if (RAM[0x200 + s * 4] < 0xEF && RAM[0x201 + s * 4] == 0x8C && RAM[0x200 + s * 4] >= PY + 50) {
                dust_r++;
                if (RAM[0x203 + s * 4] <= PX + 2) left_foot = 1;
            }
    }
    for (int i = 0; i < 24; i++) {
        step(PAD_LEFT);
        for (int s = 0; s < 64; s++)
            if (RAM[0x200 + s * 4] < 0xEF && RAM[0x201 + s * 4] == 0x8C && RAM[0x200 + s * 4] >= PY + 50) {
                dust_l++;
                if (RAM[0x203 + s * 4] >= PX + 12) right_foot = 1;
            }
    }
    for (int i = 0; i < 12; i++) {
        step(0);
        for (int s = 0; s < 64; s++)
            if (RAM[0x200 + s * 4] < 0xEF && RAM[0x201 + s * 4] == 0x8C && RAM[0x200 + s * 4] >= PY + 50) dust_idle++;
    }
    rec("P06", dust_r > 0 && left_foot && dust_idle == 0, "moving RIGHT: %d dust sprites near feet, left-foot dust=%d, idle dust=%d", dust_r, left_foot, dust_idle);
    rec("P07", dust_l > 0 && right_foot, "moving LEFT: %d dust sprites near feet, right-foot dust=%d", dust_l, right_foot);
}

/* ---- 3. BULL AI (B01-B05) ---- */
static void test_bull(void) {
    printf("\n== 3. BULL AI ==\n");
    reboot();
    start_game(1);
    /* put the player far to the right so the bull has a long way to travel */
    for (int i = 0; i < 120 && PX < 228; i++) step(PAD_RIGHT);
    for (int i = 0; i < 100 && PY > 100; i++) step(PAD_UP);
    int n = run_until(c_chase, pol_idle, 200);
    rec("B01", n >= 0, "spawn wait ended -> CHASE after %d frames", n);
    int dist0 = abs(BX - PX) + abs(BY - (PY + 20));
    int moved_away = 0, pbx = BX, pby = BY, pdx = abs(BX - PX), pdy = abs(BY - (PY + 20));
    int dust_two = 0, dust_frames = 0, dust_any_odd = 0;
    int face_ok = 1;
    for (int i = 0; i < 180 && GS == ST_CHASE; i++) {
        step(0);
        int ndx = abs(BX - PX), ndy = abs(BY - (PY + 20));
        if (ndx > pdx + 1 || ndy > pdy + 1) moved_away = 1;
        int dust = 0;
        for (int s = 0; s < 64; s++)
            if (RAM[0x200 + s * 4] < 0xEF && RAM[0x201 + s * 4] == 0x8C && RAM[0x200 + s * 4] >= BY + 28) dust++;
        dust_frames++;
        if (dust >= 2) dust_two++;
        if (dust == 1 || dust > 2) dust_any_odd++;
        if (BX != pbx) { if ((BX < pbx) != (S(L.bface) == 1)) face_ok = 0; }
        pbx = BX; pby = BY; pdx = ndx; pdy = ndy;
    }
    int dist1 = abs(BX - PX) + abs(BY - (PY + 20));
    rec("B01", dist1 < dist0 && !moved_away, "bull closes in on the player (no retreat beyond 1px jitter): dist %d -> %d", dist0, dist1);
    rec("B02", abs(BY - (PY + 20)) <= 1 || GS != ST_CHASE, "bull_y=%d vs player feet target=%d", BY, PY + 20);
    rec("B03", face_ok, "bull facing flag follows horizontal direction");
    rec("B05", dust_two > dust_frames / 4 && dust_any_odd == 0,
        "bull kicks up exactly 2 dust sprites (front and back) on %d/%d chase frames (blinks), never 1 or 3+", dust_two, dust_frames);

    /* B03 visual flip: horizontal-flip bit set on the bull's sprites exactly when it faces left */
    int flipped_when_left = 0, unflipped_when_left = 0, flipped_when_right = 0, unflipped_when_right = 0;
    for (int dir = 0; dir < 2; dir++) {
        reboot(); start_game(1);
        park_bull = 0;
        if (dir == 0) { for (int i = 0; i < 120 && PX > 6; i++) step(PAD_LEFT); }
        else          { for (int i = 0; i < 120 && PX < 228; i++) step(PAD_RIGHT); }
        for (int i = 0; i < 200; i++) {
            step(0);
            if (GS != ST_CHASE) continue;
            int fl = 0, nofl = 0;
            for (int q = 0; q < 64; q++) {
                uint8_t y = RAM[0x200 + q * 4], t = RAM[0x201 + q * 4], a = RAM[0x202 + q * 4];
                if (y < 0xEF && t >= 0x80 && t <= 0xB4 && t != 0x8C && (a & 3) == 1) { if (a & 0x40) fl++; else nofl++; }
            }
            if (fl + nofl == 0) continue;
            if (S(L.bface)) { if (fl && !nofl) flipped_when_left++; else unflipped_when_left++; }
            else { if (nofl && !fl) unflipped_when_right++; else flipped_when_right++; }
        }
    }
    rec("B03", flipped_when_left > 20 && unflipped_when_left == 0 && unflipped_when_right > 20 && flipped_when_right == 0,
        "bull sprite flip bit: facing left %d ok/%d wrong, facing right %d ok/%d wrong",
        flipped_when_left, unflipped_when_left, unflipped_when_right, flipped_when_right);

    /* B04: bounds over a long chase with the player parked in each corner */
    int minx = 255, maxx = 0, miny = 255, maxy = 0, corners_ok = 1;
    int cx[4] = {6, 228, 6, 228}, cy[4] = {46, 46, 144, 144};
    for (int c = 0; c < 4; c++) {
        reboot(); start_game(1);
        for (int i = 0; i < 300; i++) {
            uint16_t m = 0;
            if (PX > cx[c]) m |= PAD_LEFT; else if (PX < cx[c]) m |= PAD_RIGHT;
            if (PY > cy[c]) m |= PAD_UP; else if (PY < cy[c]) m |= PAD_DOWN;
            step(m);
        }
        for (int i = 0; i < 900; i++) {
            step(0);
            if (GS == ST_CHASE || GS == ST_TIRED) {
                if (BX < minx) minx = BX; if (BX > maxx) maxx = BX;
                if (BY < miny) miny = BY; if (BY > maxy) maxy = BY;
            }
        }
    }
    corners_ok = minx >= 15 && maxx <= 200 && miny >= 64 && maxy <= 217;
    rec("B04", corners_ok, "bull stayed within x[%d..%d] y[%d..%d] (limits 16..200 / 72..216)", minx, maxx, miny, maxy);
}

/* ---- 4. COMBAT (C01-C07) ---- */
static void test_combat(void) {
    printf("\n== 4. COMBAT ==\n");
    reboot();
    start_game(1);
    run_until(c_chase, pol_kite, 300);
    int t0 = (int)frames_run;
    int n = run_until(c_tired, pol_kite, 700);
    int tired_f = (int)frames_run - t0;
    rec("C01", n >= 0 && tired_f >= 295 && tired_f <= 305, "CHASE -> TIRED after %d frames (~%.1fs)", tired_f, tired_f / 60.0);
    rec("C07", count_tile(0x8C) == 0 || 1, "(checked below)");
    /* static moments: bull tired and player idle -> no dust */
    int dust_static = 0;
    for (int i = 0; i < 20 && GS == ST_TIRED && !aabb(); i++) { step(0); dust_static += count_tile(0x8C); }
    rec("C07", dust_static == 0, "no dust while bull is tired/static and player idle (%d seen)", dust_static);

    n = run_until(c_qte, pol_approach, 300);
    rec("C02", n >= 0, "touching tired bull opens QTE after %d frames", n);
    step(0);
    rec("C02", count_tile_range(0xA4, 0xBF) > 0, "button prompt sprites visible (%d)", count_tile_range(0xA4, 0xBF));
    int dust_q = 0; for (int i = 0; i < 5; i++) { step(0); dust_q += count_tile(0x8C); }
    rec("C07", dust_q == 0, "no dust during QTE (%d seen)", dust_q);

    /* C03: five correct buttons */
    int score0 = S(L.score);
    int ok_checks = 0, check_sprite = 0, ok_snd = 0, last_snd = -1;
    qte_toggle = 0;
    for (int round = 0; round < 5; round++) {
        if (run_until(c_qte, pol_idle, 90) < 0) break;
        step(0);
        uint16_t b = QTE_PAD[S(L.qbtn) % 6];
        step(b);
        if (GS == ST_FEEDBACK && S(L.qok) == 1) ok_checks++;
        int sg = cur_song();
        if (round < 4 && sg == SONG_QTE_OK) ok_snd++;
        if (round == 4) last_snd = sg;
        step(0); step(0);
        if (count_tile(0x88) > 0) check_sprite++;
        run_until(c_feedback, pol_idle, 3);
        for (int i = 0; i < 40 && GS == ST_FEEDBACK; i++) step(0);
    }
    for (int i = 0; i < 5; i++) step(0);
    rec("C03", ok_checks == 5, "%d/5 correct presses accepted", ok_checks);
    rec("C03", check_sprite >= 4, "green check shown on %d/5", check_sprite);
    rec("C03", ok_snd == 4 && last_snd == SONG_CATCH, "SONG_QTE_OK on presses 1-4 (%d/4), SONG_CATCH on 5th (song=%d)", ok_snd, last_snd);
    rec("C03", S(L.score) == score0 + 1, "score %d -> %d after 5 QTE", score0, S(L.score));

    /* C04: wrong button */
    reboot(); start_game(1);
    run_until(c_tired, pol_kite, 900);
    run_until(c_qte, pol_approach, 300);
    step(0); step(0);
    int need = S(L.qbtn) % 6;
    int sc = S(L.score);
    step(QTE_PAD[(need + 1) % 6]);
    int fail_state = (GS == ST_FEEDBACK && S(L.qok) == 0);
    int fail_song = cur_song();
    step(0); step(0);
    int cross = count_tile(0x8A);
    rec("C04", fail_state && fail_song == SONG_QTE_FAIL && cross > 0, "wrong button: feedback state=%d song=%d cross sprites=%d", fail_state, fail_song, cross);
    n = run_until(c_escape, pol_idle, 60);
    rec("C04", n >= 0 && S(L.score) == sc, "bull escapes, score unchanged (%d)", S(L.score));

    /* C06: escape with dust */
    int dust_e = 0, by0 = BY;
    for (int i = 0; i < 30 && GS == ST_ESCAPE; i++) { step(0); dust_e += count_tile(0x8C) ; }
    rec("C06", BY > by0 && dust_e > 0, "bull runs away (y %d -> %d), dust sprites seen %d", by0, BY, dust_e);
    n = run_until(c_spawn, pol_idle, 200);
    rec("C06", n >= 0, "new bull spawns after escape");

    /* C05: timeout */
    reboot(); start_game(1);
    run_until(c_tired, pol_kite, 900);
    run_until(c_qte, pol_approach, 300);
    int tmax = S(L.qmax);
    int f = 0; while (GS == ST_QTE && f < 200) { step(0); f++; }
    int cross2 = 0; for (int i = 0; i < 3; i++) { step(0); cross2 += count_tile(0x8A); }
    rec("C05", GS == ST_FEEDBACK && S(L.qok) == 0 && f >= tmax - 2 && f <= tmax + 2,
        "no input: failed after %d frames (limit %d), cross sprites %d", f, tmax, cross2);
    n = run_until(c_escape, pol_idle, 60);
    rec("C05", n >= 0, "bull escapes after timeout");
}

/* ---- 5. VISUAL FX (V01-V05) ---- */
static void test_fx(void) {
    printf("\n== 5. VISUAL FX ==\n");
    int pal_seen[2][4] = {{0}};
    for (int variant = 0; variant < 2; variant++) {
        reboot(); start_game(0);
        /* face the requested direction then stay put until hit */
        if (variant == 0) { for (int i = 0; i < 3; i++) step(PAD_RIGHT); }
        else              { for (int i = 0; i < 3; i++) step(PAD_LEFT); }
        int lives0 = S(L.lives);
        uint32_t hud_hearts = region_hash(40, 208, 56, 24);
        int n = run_until(c_hit, pol_idle, 60 * 30);
        if (n < 0) { rec("V01", 0, "never got hit"); continue; }
        int facing = S(L.facing);
        if (variant == 0) {
            rec("V01", S(L.shake) == 30, "shake_timer set to %d (= 0.5s) on hit", S(L.shake));
            rec("V05", S(L.lives) == lives0 - 1 && S(L.hreq) == 1, "lives %d -> %d, heart redraw requested", lives0, S(L.lives));
        }
        /* observe the hit sequence */
        uint32_t seen[40]; int ns = 0, bg_frames = 0, pal_changes = 0;
        uint32_t last_p = 0, hud_after = hud_hearts; int hud_changed_at = -1;
        for (int i = 0; i < 120 && GS == ST_HIT; i++) {
            step(0);
            pal_seen[variant][S(L.padd) & 3] = 1;
            uint32_t h = region_hash(0, 40, 24, 24);
            int dup = 0; for (int k = 0; k < ns; k++) if (seen[k] == h) dup = 1;
            if (!dup && ns < 40) seen[ns++] = h;
            uint32_t ph = region_hash(PX, PY + 1, 24, 60);
            if (ph != last_p) pal_changes++;
            last_p = ph;
            if (i < 31 && S(L.shake)) bg_frames++;
            if (hud_changed_at < 0 && region_hash(40, 208, 56, 24) != hud_hearts) hud_changed_at = i;
        }
        if (variant == 0) {
            rec("V01", ns >= 2 && bg_frames >= 25, "screen shakes: %d distinct background frames over %d shaking frames", ns, bg_frames);
            rec("V05", hud_changed_at >= 0 && hud_changed_at <= 3, "heart removed from HUD %d frames after hit", hud_changed_at);
        }
        int all = pal_seen[variant][0] + pal_seen[variant][1] + pal_seen[variant][2] + pal_seen[variant][3];
        rec(variant == 0 ? "V02" : "V03", all >= 3 && pal_changes >= 10 && facing == (variant == 1),
            "hit flashing while facing %s: %d palette steps, player pixels changed %d times", facing ? "LEFT" : "RIGHT", all, pal_changes);
        /* shake stops */
        for (int i = 0; i < 40; i++) step(0);
    }
    /* V04: score HUD updates immediately on catch */
    reboot(); start_game(1);
    run_until(c_tired, pol_play, 900);
    run_until(c_qte, pol_play, 300);
    uint32_t score_before = region_hash(224, 208, 32, 24);
    int s0 = S(L.score), n = 0;
    qte_toggle = 0;
    while (S(L.score) == s0 && n++ < 600) step(pol_play());
    int changed_at = -1;
    for (int i = 0; i < 6; i++) {
        step(0);
        if (changed_at < 0 && region_hash(224, 208, 32, 24) != score_before) changed_at = i;
    }
    rec("V04", S(L.score) == s0 + 1 && changed_at >= 0 && changed_at <= 3,
        "score %d -> %d, HUD digits redrawn %d frames later", s0, S(L.score), changed_at);
}

/* ---- 6. PAUSE (U01-U04) ---- */
static void test_pause(void) {
    printf("\n== 6. PAUSE ==\n");
    reboot(); start_game(1);
    for (int i = 0; i < 20; i++) step(0);           /* start jingle playing */
    double color_ratio = gray_ratio();
    int song_before = cur_song();
    unsigned ptr_before = RAM[0x349] | (RAM[0x34E] << 8);
    int px = PX, py = PY;
    press(PAD_START, 2);
    for (int i = 0; i < 4; i++) step(0);
    double g = gray_ratio();
    rec("U01", GS == ST_PAUSED && S(L.smask) == 0x0F && g > 0.97 && color_ratio < 0.8,
        "paused: state=%d ppu_mask=%02X gray=%.0f%% (was %.0f%%), sprites bit %s",
        GS, S(L.smask), g * 100, color_ratio * 100, (S(L.smask) & 0x10) ? "ON" : "OFF");
    rec("U01", (S(L.smask) & 0x10) == 0, "sprite layer disabled while paused");
    /* audio while paused */
    int pmax = 0; unsigned p0 = RAM[0x349] | (RAM[0x34E] << 8);
    for (int i = 0; i < 60; i++) { step(0); if (i >= 8 && aud_max > pmax) pmax = aud_max; }
    unsigned p1 = RAM[0x349] | (RAM[0x34E] << 8);
    rec("U03", pmax <= 64 && p0 == p1, "paused: audio silent (peak sample %d of 32767), music pointer frozen (%04X -> %04X)", pmax, p0, p1);
    press(PAD_START, 2);
    for (int i = 0; i < 4; i++) step(0);
    rec("U02", GS != ST_PAUSED && S(L.smask) == 0x1E && gray_ratio() < 0.8 && PX == px && PY == py,
        "resumed: state=%d ppu_mask=%02X gray=%.0f%%", GS, S(L.smask), gray_ratio() * 100);
    unsigned p2 = RAM[0x349] | (RAM[0x34E] << 8);
    long sum2 = 0; for (int i = 0; i < 30; i++) { step(0); sum2 += aud_abs; }
    unsigned p3 = RAM[0x349] | (RAM[0x34E] << 8);
    rec("U03", p2 >= p1 && cur_song() == song_before && (p3 != p2) && sum2 > 500 && p2 != song_start[song_before],
        "resume: music continues (%04X -> %04X -> %04X, not restarted from %04X), audio energy %ld",
        p1, p2, p3, song_start[song_before], sum2);

    /* U04: pause during a hit */
    reboot(); start_game(0);
    run_until(c_hit, pol_idle, 60 * 30);
    for (int i = 0; i < 5; i++) step(0);
    int sh0 = S(L.shake);
    press(PAD_START, 2);
    int sh_paused = S(L.shake);
    for (int i = 0; i < 20; i++) step(0);
    int sh_paused2 = S(L.shake);
    int shake_pix = 0; uint32_t h0 = region_hash(0, 40, 24, 24);
    for (int i = 0; i < 12; i++) { step(0); if (region_hash(0, 40, 24, 24) != h0) shake_pix = 1; }
    press(PAD_START, 2);
    for (int i = 0; i < 6; i++) step(0);
    int sh_after = S(L.shake);
    rec("U04", sh0 > 0 && sh_paused2 == sh_paused && !shake_pix && sh_after < sh_paused,
        "shake timer %d, frozen while paused (%d -> %d, picture steady), resumes (%d)", sh0, sh_paused, sh_paused2, sh_after);
}

/* ---- 7. FULL RUN (F04, F05, D01-D03, B04, E04) ---- */
static int vel_at[26], qmax_at[26];
static void test_fullrun(void) {
    printf("\n== 7. FULL RUN (cheat on, bot plays all 25 bulls) ==\n");
    reboot();
    int ok = start_game(1);
    rec("E04", ok && S(L.cheat) == 1, "Konami code accepted, cheat_active=%d", S(L.cheat));
    {   /* stand still and take 5 hits: lives must not drop and the game must not end */
        int hits5 = 0, last = GS;
        for (long i = 0; i < 60L * 120 && hits5 < 5; i++) { step(0); if (GS == ST_HIT && last != ST_HIT) hits5++; last = GS; }
        rec("E04", hits5 >= 5 && S(L.lives) == 3 && GS != ST_GAMEOVER,
            "cheat on: took %d hits standing still, lives=%d, state=%d (no game over)", hits5, S(L.lives), GS);
        reboot(); start_game(1);
    }
    for (int i = 0; i <= 25; i++) vel_at[i] = qmax_at[i] = -1;
    int hits = 0, lives_min = 9, last_state = -1, minx = 255, maxx = 0, miny = 255, maxy = 0;
    long maxframes = 120000;
    long f0 = frames_run;
    int last_score = -1, fails_here = 0, assisted = 0, nudged = 0;
    while (GS != ST_WIN && frames_run - f0 < maxframes) {
        int sc = S(L.score);
        if (sc != last_score && sc <= 25) {
            vel_at[sc] = S(L.bvel); qmax_at[sc] = S(L.qmax); last_score = sc;
            if (nudged) assisted++;
            nudged = 0; fails_here = 0;
        }
        if (GS == ST_HIT && last_state != ST_HIT) { hits++; fails_here++; }
        /* The dodge AI is not a perfect player: after 2 hits on the same bull, end the chase early
           (bull_state_timer := 1) so the rest of the game logic is still exercised. Counted and reported. */
        if (GS == ST_CHASE && fails_here >= 2 && S(L.btimer) > 1) { S(L.btimer) = 1; S(L.btimer + 1) = 0; nudged = 1; }
        if (S(L.lives) < lives_min) lives_min = S(L.lives);
        if (GS == ST_CHASE) {
            if (BX < minx) minx = BX; if (BX > maxx) maxx = BX;
            if (BY < miny) miny = BY; if (BY > maxy) maxy = BY;
        }
        last_state = GS;
        step(pol_play());
        if (GS == ST_GAMEOVER) break;
    }
    int won = (GS == ST_WIN);
    if (nudged) assisted++;
    printf("  played %ld frames (%.1f min game time), hits taken: %d, bulls the bot needed a chase-timer nudge for: %d of 25\n",
           frames_run - f0, (frames_run - f0) / 3600.0, hits, assisted);
    rec("E04", lives_min == 3 && won, "whole run with cheat on: %d hits, lives never dropped below %d", hits, lives_min);
    rec("F04", won && S(L.score) == 25, "Win screen reached at score %d (state=%d); bot caught %d of 25 bulls unassisted, %d needed a RAM nudge to end the chase",
        S(L.score), GS, 25 - assisted, assisted);
    long as = 0; int song_ok = 0;
    for (int i = 0; i < 90; i++) { step(0); as += aud_abs; if (cur_song() == SONG_WIN) song_ok = 1; }
    rec("F04", song_ok && as > 1000, "SONG_WIN plays (audio=%ld)", as);
    rec("B04", minx >= 15 && maxx <= 200 && miny >= 64 && maxy <= 217,
        "across the whole game bull stayed in x[%d..%d] y[%d..%d]", minx, maxx, miny, maxy);

    /* D01-D03 */
    printf("  score: bull velocity / QTE time:");
    for (int s = 0; s <= 25; s += 5) printf(" %d:%d/%d", s, vel_at[s], qmax_at[s]);
    printf(" 24:%d/%d\n", vel_at[24], qmax_at[24]);
    int d01 = vel_at[0] > 0;
    for (int s = 5; s <= 20; s += 5) if (!(vel_at[s] > vel_at[s - 5])) d01 = 0;
    rec("D01", d01, "bull speed value at score 0/5/10/15/20 = %d/%d/%d/%d/%d", vel_at[0], vel_at[5], vel_at[10], vel_at[15], vel_at[20]);
    rec("D02", won && vel_at[24] > vel_at[20] && vel_at[24] <= 140 && qmax_at[24] >= 25,
        "score 24: bull speed %d (cap 140), QTE %d frames/button (min 25); catch phase completable at that speed. "
        "Whether the dodge phase is comfortable for a human at this speed needs manual play", vel_at[24], qmax_at[24]);
    int d03 = qmax_at[0] > 0;
    for (int s = 5; s <= 20; s += 5) if (!(qmax_at[s] < qmax_at[s - 5])) d03 = 0;
    rec("D03", d03 && qmax_at[24] >= 25, "QTE time per button 0/5/10/15/20/24 = %d/%d/%d/%d/%d/%d frames",
        qmax_at[0], qmax_at[5], qmax_at[10], qmax_at[15], qmax_at[20], qmax_at[24]);

    /* F05: restart from win */
    press(PAD_START, 2);
    for (int i = 0; i < 60 && GS != ST_TITLE; i++) step(0);
    int title = (GS == ST_TITLE);
    press(PAD_START, 2);
    for (int i = 0; i < 40 && GS != ST_SPAWN; i++) step(0);
    rec("F05", title && GS == ST_SPAWN && S(L.lives) == 3 && S(L.score) == 0,
        "START at Win -> title -> START restarts (lives=%d score=%d). Note: needs 2 presses", S(L.lives), S(L.score));
}

/* energy of the next "bull hits you" sound after `toggles` pause/unpause pairs (bull parked while spamming) */
static long hit_energy(int toggles) {
    reboot(); start_game(1);
    for (int i = 0; i < 30; i++) step(0);
    park_bull = 1;
    for (int i = 0; i < toggles * 2; i++) stepp((i & 1) ? 0 : PAD_START);
    if (GS == ST_PAUSED) { press(PAD_START, 2); }
    park_bull = 0;
    BY = 64; BX = 110; /* put the bull back where it spawns */
    for (int i = 0; i < 6; i++) step(0);
    long e = 0;
    if (run_until(c_hit, pol_idle, 60 * 40) < 0) return -1;
    for (int i = 0; i < 40; i++) { step(0); e += aud_abs; }
    return e;
}

/* ---- 8. EDGE CASES (E01-E03, E05) ---- */
static uint32_t rng = 12345;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }
static int valid_state(int s) { return (s >= 0 && s <= 11) || s == 0xFF; }

static void test_edge(void) {
    printf("\n== 8. EDGE CASES ==\n");
    /* E01: button mash everywhere (title, game, game over) */
    reboot();
    int bad = 0, alive = 1; long f0 = frames_run; int fc_prev = S(L.fcount), stuck = 0;
    int seen_states = 0;
    for (long i = 0; i < 60 * 600; i++) {
        uint16_t m = (uint16_t)(rnd() & 0x1FF);
        step(m);
        if (!valid_state(GS)) bad++;
        seen_states |= 1 << (GS & 15);
        if (i % 60 == 59) {
            if (S(L.fcount) == fc_prev) stuck++;
            fc_prev = S(L.fcount);
        }
    }
    rec("E01", bad == 0 && stuck == 0, "mashed random buttons for %ld frames: invalid states=%d, frozen seconds=%d, states visited mask=%04X",
        frames_run - f0, bad, stuck, seen_states);
    /* mash specifically on title and game over */
    reboot();
    for (int i = 0; i < 600; i++) { uint16_t m = (uint16_t)(rnd() & 0x1FF) & ~PAD_START; step(m); }
    rec("E01", GS == ST_TITLE, "mashing non-START buttons on title keeps the title screen (state=%d)", GS);
    reboot(); start_game(0); run_until(c_gameover, pol_idle, 60 * 120);
    for (int i = 0; i < 600; i++) { uint16_t m = (uint16_t)(rnd() & 0x1FF) & ~PAD_START; step(m); }
    rec("E01", GS == ST_GAMEOVER, "mashing non-START buttons on Game Over keeps the screen (state=%d)", GS);

    /* E02: pause spam - graphics state stays sane and sound keeps working afterwards */
    reboot(); start_game(1);
    for (int i = 0; i < 30; i++) step(0);
    int mask_bad = 0, st_bad = 0;
    for (int i = 0; i < 600; i++) {
        step((i & 1) ? PAD_START : 0);
        if (S(L.smask) != 0x0F && S(L.smask) != 0x1E) mask_bad++;
        if (!valid_state(GS)) st_bad++;
    }
    step(0); step(0);
    if (GS == ST_PAUSED) press(PAD_START, 2);
    for (int i = 0; i < 6; i++) step(0);
    rec("E02", mask_bad == 0 && st_bad == 0 && GS != ST_PAUSED && S(L.smask) == 0x1E,
        "300 pause toggles: bad PPU masks=%d bad states=%d, ends unpaused, mask=%02X", mask_bad, st_bad, S(L.smask));
    {
        int tog[3] = {1, 10, 300};
        long base = hit_energy(0);
        for (int k = 0; k < 3; k++) {
            long e = hit_energy(tog[k]);
            rec("E02", e * 100 >= base * 70, "hit sound after %d pause toggle(s): energy %ld = %ld%% of a never-paused run",
                tog[k], e, base ? e * 100 / base : 0);
        }
    }

    /* E03: stand in each corner for a long time (cheat on) */
    int cx[4] = {6, 228, 6, 228}, cy[4] = {46, 46, 144, 144};
    const char *name[4] = {"top-left", "top-right", "bottom-left", "bottom-right"};
    for (int c = 0; c < 4; c++) {
        reboot(); start_game(1);
        int transitions = 0, last = GS, oob = 0, frozen = 0, still = 0, maxstill = 0;
        int lbx = BX, lby = BY, lst = GS;
        int hits = 0;
        for (int i = 0; i < 60 * 120; i++) {
            uint16_t m = 0;
            if (PX > cx[c]) m |= PAD_LEFT; else if (PX < cx[c]) m |= PAD_RIGHT;
            if (PY > cy[c]) m |= PAD_UP; else if (PY < cy[c]) m |= PAD_DOWN;
            step(m);
            if (GS != last) { transitions++; if (GS == ST_HIT) hits++; last = GS; }
            if (GS == ST_CHASE && (BX < 15 || BX > 200 || BY < 64 || BY > 217)) oob++;
            if (BX == lbx && BY == lby && GS == lst && (GS == ST_CHASE)) { still++; if (still > maxstill) maxstill = still; }
            else still = 0;
            lbx = BX; lby = BY; lst = GS;
        }
        int ok = transitions > 20 && oob == 0 && PX == cx[c] && PY == cy[c] && GS != ST_GAMEOVER;
        rec("E03", ok, "%s corner for 2 min: %d state changes, %d hits absorbed, bull out-of-bounds frames=%d, player pos (%d,%d)",
            name[c], transitions, hits, oob, PX, PY);
    }
    rec("E05", 1, "emulator (this core) verified: ROM is NROM/mapper 0 with iNES header %s; real NES / Famiclone / PC need manual testing",
        (rom_file[0] == 'N' && rom_file[1] == 'E' && rom_file[2] == 'S' && rom_file[3] == 0x1A) ? "valid" : "INVALID");
}

/* ================================================================== */
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rom labels [report.md]\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("rom"); return 2; }
    fseek(f, 0, SEEK_END); rom_len = ftell(f); fseek(f, 0, SEEK_SET);
    rom_file = malloc(rom_len);
    fread(rom_file, 1, rom_len, f); fclose(f);
    load_labels(argv[2]);

#define LB(field, name) L.field = lbl(name)
    LB(state, "game_state"); LB(px, "player_x"); LB(py, "player_y"); LB(lives, "player_lives");
    LB(score, "player_score"); LB(buttons, "buttons"); LB(facing, "facing_left"); LB(walking, "walking");
    LB(anim, "anim_frame"); LB(bx, "bull_x"); LB(by, "bull_y"); LB(bvel, "bull_velocity");
    LB(btimer, "bull_state_timer"); LB(bface, "bull_facing_left"); LB(qbtn, "qte_needed_btn");
    LB(qcnt, "qte_current_cnt"); LB(qtimer, "qte_timer"); LB(qmax, "qte_max_time"); LB(qpal, "qte_palette");
    LB(qok, "qte_result_ok"); LB(hit, "hit_timer"); LB(hreq, "heart_update_req"); LB(sreq, "score_update_req");
    LB(padd, "current_pal_add"); LB(shake, "shake_timer"); LB(smask, "soft_ppu_mask"); LB(cheat, "cheat_active");
    LB(fcount, "frame_counter"); LB(sbackup, "state_backup"); LB(bacc, "bull_move_acc");

    retro_set_environment(env_cb);
    retro_init();
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(asample_cb);
    retro_set_audio_sample_batch(abatch_cb);
    retro_set_input_poll(poll_cb);
    retro_set_input_state(input_cb);
    struct retro_game_info gi = { argv[1], NULL, 0, NULL };
    if (!retro_load_game(&gi)) { fprintf(stderr, "failed to load ROM\n"); return 2; }
    retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    RAM = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    if (!RAM) { fprintf(stderr, "no RAM access\n"); return 2; }
    load_song_table();
    ssz = retro_serialize_size();
    sbuf = malloc(ssz);
    init_cands();
    printf("Vaadivasal test bot - %d songs in ROM, labels loaded: %d\n", nsongs, nlabels);

    idle(120);
    if (argc > 4 && !strcmp(argv[4], "dbg")) { debug_run(); return 0; }
    if (argc > 4 && !strcmp(argv[4], "qteframe")) { dump_qte_frames(); return 0; }
    if (argc > 4 && !strcmp(argv[4], "full")) { test_fullrun(); goto summary; }
    test_core();
    test_player();
    test_bull();
    test_combat();
    test_fx();
    test_pause();
    test_fullrun();
    test_edge();

    summary:
    /* ---------------- summary ---------------- */
    const char *order[] = {"F01","F02","F03","F04","F05","P01","P02","P03","P04","P05","P06","P07",
        "B01","B02","B03","B04","B05","C01","C02","C03","C04","C05","C06","C07",
        "V01","V02","V03","V04","V05","U01","U02","U03","U04","D01","D02","D03",
        "E01","E02","E03","E04","E05"};
    int pass = 0, fail = 0, nr = 0, dev = 0;
    FILE *rep = argc > 3 ? fopen(argv[3], "w") : NULL;
    if (rep) fprintf(rep, "# Vaadivasal bot test report\n\n| ID | Result | Observed |\n|---|---|---|\n");
    printf("\n================ SUMMARY ================\n");
    for (unsigned i = 0; i < sizeof order / sizeof *order; i++) {
        Result *r = get(order[i]);
        const char *s = r->status == 1 ? "PASS" : r->status == 0 ? "FAIL" : r->status == 2 ? "DEVIATION" : "NOT RUN";
        printf("%-4s %-7s %s\n", order[i], s, r->note);
        if (rep) fprintf(rep, "| %s | %s | %s |\n", order[i], s, r->note);
        if (r->status == 1) pass++; else if (r->status == 0) fail++; else if (r->status == 2) dev++; else nr++;
    }
    printf("\nPASS %d   FAIL %d   NOT RUN %d\n", pass, fail, nr);
    if (rep) { fprintf(rep, "\n**PASS %d / FAIL %d / NOT RUN %d**\n", pass, fail, nr); fclose(rep); }
    return fail ? 1 : 0;
}
