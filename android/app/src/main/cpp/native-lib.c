/* Minimal libretro frontend bridging the FCEUmm core to Java. */
#include <jni.h>
#include <android/bitmap.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <android/log.h>
#include "libretro.h"

#define AUDIO_CAP 16384

static uint16_t frame[256 * 240];
static int16_t audio[AUDIO_CAP * 2];
static int audio_frames = 0;
static uint16_t input_mask = 0;

static void log_cb(enum retro_log_level l, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "fceumm", fmt, ap);
    va_end(ap);
}

static bool env_cb(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)data = true; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = log_cb; return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = "/data/local/tmp"; return true;
    default:
        return false;
    }
}

static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data) return;
    if (w > 256) w = 256;
    if (h > 240) h = 240;
    for (unsigned y = 0; y < h; y++)
        memcpy(&frame[y * 256], (const uint8_t *)data + y * pitch, w * 2);
}

static size_t audio_batch_cb(const int16_t *data, size_t frames) {
    if (audio_frames + (int)frames > AUDIO_CAP) frames = AUDIO_CAP - audio_frames;
    memcpy(&audio[audio_frames * 2], data, frames * 4);
    audio_frames += frames;
    return frames;
}
static void audio_cb(int16_t l, int16_t r) {
    int16_t s[2] = {l, r};
    audio_batch_cb(s, 1);
}
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned port, unsigned dev, unsigned idx, unsigned id) {
    if (port != 0 || dev != RETRO_DEVICE_JOYPAD || id > 15) return 0;
    return (input_mask >> id) & 1;
}

static int loaded = 0;

/* Phone screens render the NES palette darker than a TV does. Lift the mid-tones with a gamma curve;
   GAMMA < 1 brightens (1.0 = unchanged). Built once as a lookup table for every RGB565 value. */
#define GAMMA 0.70f
static uint16_t bright[65536];
static void build_brightness_table(void) {
    uint8_t ch5[32], ch6[64];
    for (int i = 0; i < 32; i++) ch5[i] = (uint8_t)(31.0f * powf(i / 31.0f, GAMMA) + 0.5f);
    for (int i = 0; i < 64; i++) ch6[i] = (uint8_t)(63.0f * powf(i / 63.0f, GAMMA) + 0.5f);
    for (int v = 0; v < 65536; v++)
        bright[v] = (uint16_t)((ch5[(v >> 11) & 31] << 11) | (ch6[(v >> 5) & 63] << 5) | ch5[v & 31]);
}

JNIEXPORT jboolean JNICALL
Java_com_dineshrichard_vaadivasal_Emulator_nativeInit(JNIEnv *env, jclass c, jstring path) {
    if (loaded) return JNI_TRUE;   /* already running: keep the game state (e.g. after the app was in the background) */
    retro_set_environment(env_cb);
    retro_init();
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);

    const char *p = (*env)->GetStringUTFChars(env, path, NULL);
    static char rom_path[1024];
    strncpy(rom_path, p, sizeof(rom_path) - 1);
    (*env)->ReleaseStringUTFChars(env, path, p);
    struct retro_game_info info = { rom_path, NULL, 0, NULL };
    if (!retro_load_game(&info)) return JNI_FALSE;
    retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    build_brightness_table();
    loaded = 1;
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_dineshrichard_vaadivasal_Emulator_nativeRunFrame(JNIEnv *env, jclass c, jint mask) {
    input_mask = (uint16_t)mask;
    audio_frames = 0;
    retro_run();
}

JNIEXPORT jint JNICALL
Java_com_dineshrichard_vaadivasal_Emulator_nativeGetAudio(JNIEnv *env, jclass c, jshortArray out) {
    int n = audio_frames;
    jsize cap = (*env)->GetArrayLength(env, out) / 2;
    if (n > cap) n = cap;
    (*env)->SetShortArrayRegion(env, out, 0, n * 2, audio);
    return n;
}

JNIEXPORT void JNICALL
Java_com_dineshrichard_vaadivasal_Emulator_nativeCopyFrame(JNIEnv *env, jclass c, jobject bmp) {
    void *px;
    if (AndroidBitmap_lockPixels(env, bmp, &px) != 0) return;
    uint16_t *dst = (uint16_t *)px;
    for (int i = 0; i < 256 * 240; i++) dst[i] = bright[frame[i]];
    AndroidBitmap_unlockPixels(env, bmp);
}

JNIEXPORT void JNICALL
Java_com_dineshrichard_vaadivasal_Emulator_nativeShutdown(JNIEnv *env, jclass c) {
    if (!loaded) return;
    loaded = 0;
    retro_unload_game();
    retro_deinit();
}
