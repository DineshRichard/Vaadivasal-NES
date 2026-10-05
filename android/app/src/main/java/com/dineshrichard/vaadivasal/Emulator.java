package com.dineshrichard.vaadivasal;

import android.graphics.Bitmap;

final class Emulator {
    static { System.loadLibrary("vaadivasal"); }

    // libretro joypad ids as bit positions
    static final int B = 1, Y = 2, SELECT = 4, START = 8, UP = 16, DOWN = 32,
            LEFT = 64, RIGHT = 128, A = 256;

    static native boolean nativeInit(String romPath);
    static native void nativeRunFrame(int mask);
    static native int nativeGetAudio(short[] out);
    static native void nativeCopyFrame(Bitmap bmp);
    static native void nativeShutdown();
}
