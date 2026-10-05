package com.dineshrichard.vaadivasal;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import java.io.InputStream;

public class MainActivity extends Activity {
    private GameView game;
    private int touchMask, keyMask;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        java.io.File rom = new java.io.File(getFilesDir(), "vaadivasal.nes");
        try (InputStream in = getAssets().open("vaadivasal.nes");
             java.io.FileOutputStream out = new java.io.FileOutputStream(rom)) {
            byte[] buf = new byte[8192];
            for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        game = new GameView(this, rom.getAbsolutePath());
        FrameLayout root = new FrameLayout(this);
        root.addView(game);
        ControlsView controls = new ControlsView(this, m -> { touchMask = m; push(); });
        root.addView(controls);
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            int top = insets.getSystemWindowInsetTop();
            if (android.os.Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null) {
                top = Math.max(top, insets.getDisplayCutout().getSafeInsetTop());
            }
            GameView.safeTop = top;
            controls.invalidate();
            return insets;
        });
        addInfoButtons(root);
        setContentView(root);
        hideBars();
    }

    private static final String HELP_TEXT =
            "Goal: tame all 25 bulls in the Alanganallur arena without losing your 3 lives.\n\n"
            + "1. Dodge - move with the D-pad and stay clear of the charging bull.\n"
            + "2. Wait - when the bull stops and blinks, it is tired.\n"
            + "3. Grab - run up and touch the bull.\n"
            + "4. The Lock - five buttons flash on screen. Press each one quickly (A, B or a D-pad direction) before time runs out.\n\n"
            + "START pauses and resumes. SELECT is not used.";

    private static final String CREDITS_TEXT =
            "Vaadivasal (\u0BB5\u0BBE\u0B9F\u0BBF\u0BB5\u0BBE\u0B9A\u0BB2\u0BCD)\n"
            + "The first Tamil 8-bit game\n\n"
            + "Game, pixel art, music and sound: Dinesh Richard\n\n"
            + "Made with cc65, FamiStudio, YY-CHR and NES Screen Tool.\n\n"
            + "This Android version runs on the FCEUmm emulator core (GNU GPL v2). "
            + "The app's source code is free software: github.com/DineshRichard/Vaadivasal-NES\n\n"
            + "\u00A9 2026 Dinesh Richard. Independent homebrew project, not affiliated with Nintendo.";

    /** Two very small buttons, one in each bottom corner. */
    private void addInfoButtons(FrameLayout root) {
        float d = getResources().getDisplayMetrics().density;
        addCorner(root, smallButton("உதவி · Help", "Help", HELP_TEXT),
                android.view.Gravity.BOTTOM | android.view.Gravity.START, d);
        addCorner(root, smallButton("Credits", "Credits", CREDITS_TEXT),
                android.view.Gravity.BOTTOM | android.view.Gravity.END, d);
    }

    private void addCorner(FrameLayout root, View v, int gravity, float d) {
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, gravity);
        lp.bottomMargin = (int) (28 * d);
        lp.leftMargin = lp.rightMargin = (int) (16 * d);
        root.addView(v, lp);
    }

    private android.widget.Button smallButton(String label, String title, String body) {
        float d = getResources().getDisplayMetrics().density;
        android.widget.Button b = new android.widget.Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextSize(11);
        b.setTextColor(ControlsView.ORANGE);
        b.setMinHeight(0); b.setMinimumHeight(0);
        b.setMinWidth(0); b.setMinimumWidth(0);
        b.setPadding((int) (10 * d), (int) (4 * d), (int) (10 * d), (int) (4 * d));
        android.graphics.drawable.GradientDrawable bg = new android.graphics.drawable.GradientDrawable();
        bg.setColor(ControlsView.lift(0xFF101010));
        bg.setStroke((int) Math.max(1, d), ControlsView.ORANGE);
        bg.setCornerRadius(2 * d);
        b.setBackground(bg);
        b.setOnClickListener(v -> showInfo(title, body));
        return b;
    }

    private void showInfo(String title, String body) {
        game.setPaused(true);
        touchMask = 0; keyMask = 0; push();
        new android.app.AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
                .setTitle(title)
                .setMessage(body)
                .setPositiveButton("OK", null)
                .setOnDismissListener(dlg -> { game.setPaused(false); hideBars(); })
                .show();
    }

    private void push() { game.setInput(touchMask | keyMask); }

    @SuppressWarnings("deprecation")
    private void hideBars() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    @Override public void onWindowFocusChanged(boolean f) {
        super.onWindowFocusChanged(f);
        if (f) hideBars();
    }

    @Override protected void onPause() {
        super.onPause();
        touchMask = 0; keyMask = 0; push();       // no stuck buttons when the player comes back
        game.setPaused(true);
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        if (isFinishing()) Emulator.nativeShutdown();
    }
    @Override protected void onResume() { super.onResume(); game.setPaused(false); }

    private static int map(int code) {
        switch (code) {
            case KeyEvent.KEYCODE_DPAD_UP: return Emulator.UP;
            case KeyEvent.KEYCODE_DPAD_DOWN: return Emulator.DOWN;
            case KeyEvent.KEYCODE_DPAD_LEFT: return Emulator.LEFT;
            case KeyEvent.KEYCODE_DPAD_RIGHT: return Emulator.RIGHT;
            case KeyEvent.KEYCODE_BUTTON_A: return Emulator.A;
            case KeyEvent.KEYCODE_BUTTON_B: return Emulator.B;
            case KeyEvent.KEYCODE_BUTTON_START: return Emulator.START;
            case KeyEvent.KEYCODE_BUTTON_SELECT: return Emulator.SELECT;
            default: return 0;
        }
    }

    @Override public boolean onKeyDown(int code, KeyEvent e) {
        int m = map(code);
        if (m == 0) return super.onKeyDown(code, e);
        keyMask |= m; push(); return true;
    }

    @Override public boolean onKeyUp(int code, KeyEvent e) {
        int m = map(code);
        if (m == 0) return super.onKeyUp(code, e);
        keyMask &= ~m; push(); return true;
    }
}
