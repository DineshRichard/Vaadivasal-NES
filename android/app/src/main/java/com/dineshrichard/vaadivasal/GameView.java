package com.dineshrichard.vaadivasal;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** Runs the emulator on its own thread, paced by the blocking audio write. */
final class GameView extends SurfaceView implements SurfaceHolder.Callback, Runnable {
    private static final int RATE = 48000;
    private static final int AUDIO_MAX = 4096;

    private final Bitmap bmp = Bitmap.createBitmap(256, 240, Bitmap.Config.RGB_565);
    private final Paint paint = new Paint();
    private final String rom;
    private volatile int input;
    private volatile boolean running, paused;
    private Thread thread;
    private SurfaceHolder holder;

    GameView(Context ctx, String rom) {
        super(ctx);
        this.rom = rom;
        paint.setFilterBitmap(false);
        getHolder().addCallback(this);
    }

    void setInput(int mask) { input = mask; }
    void setPaused(boolean p) { paused = p; }

    @Override public void surfaceCreated(SurfaceHolder h) {
        holder = h;
        running = true;
        thread = new Thread(this, "emu");
        thread.start();
    }

    @Override public void surfaceChanged(SurfaceHolder h, int f, int w, int hh) {}

    @Override public void surfaceDestroyed(SurfaceHolder h) {
        running = false;
        try { thread.join(); } catch (InterruptedException ignored) {}
    }

    @Override public void run() {
        if (!Emulator.nativeInit(rom)) {
            android.util.Log.e("Vaadivasal", "nativeInit failed");
            return;
        }
        int min = AudioTrack.getMinBufferSize(RATE, AudioFormat.CHANNEL_OUT_STEREO,
                AudioFormat.ENCODING_PCM_16BIT);
        AudioTrack track = new AudioTrack.Builder()
                .setAudioFormat(new AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setSampleRate(RATE)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
                .setBufferSizeInBytes(Math.max(min, 8192))
                .setTransferMode(AudioTrack.MODE_STREAM).build();
        track.play();
        short[] buf = new short[AUDIO_MAX * 2];
        Rect dst = new Rect();
        while (running) {
            if (paused) {
                try { Thread.sleep(50); } catch (InterruptedException ignored) {}
                continue;
            }
            Emulator.nativeRunFrame(input);
            Emulator.nativeCopyFrame(bmp);
            draw(dst);
            int n = Emulator.nativeGetAudio(buf);
            if (n > 0) track.write(buf, 0, n * 2);
        }
        track.release();
        // The core is deliberately NOT shut down here: the surface is destroyed whenever the app goes to the
        // background, and the player must come back to the same game. MainActivity.onDestroy shuts it down.
    }

    /** Top safe area (camera cutout / status bar) in pixels, set by the activity. */
    static volatile int safeTop;

    /** Portrait: pin the picture to the top, just below the safe area. */
    static int topInset(int w, int h) {
        return h > w ? safeTop : 0;
    }

    /** Height in pixels of the scaled NES picture. */
    static int pictureHeight(int w, int h) {
        return (int) (240 * Math.min(w / 256f, h / 240f));
    }

    private void draw(Rect dst) {
        Canvas c = holder.lockCanvas();
        if (c == null) return;
        c.drawColor(0xFF000000);
        int w = c.getWidth(), h = c.getHeight();
        float scale = Math.min(w / 256f, h / 240f);
        int top = topInset(w, h);
        int dw = (int) (256 * scale), dh = pictureHeight(w, h);
        dst.set((w - dw) / 2, top, (w + dw) / 2, top + dh);
        c.drawBitmap(bmp, null, dst, paint);
        holder.unlockCanvasAndPost(c);
    }
}
