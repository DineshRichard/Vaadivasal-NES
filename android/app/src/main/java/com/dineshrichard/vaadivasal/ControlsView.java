package com.dineshrichard.vaadivasal;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

/**
 * Touch overlay styled after the game's title screen: charcoal panels, orange
 * borders, brick-red pressed states, hard pixel edges and Tamil labels.
 */
final class ControlsView extends View {
    interface Listener { void onInput(int mask); }

    /** Same brightness curve the game picture gets (native-lib.c GAMMA), so the controls match it. */
    static final float GAMMA = 0.70f;

    static int lift(int argb) {
        int r = (argb >> 16) & 255, g = (argb >> 8) & 255, b = argb & 255;
        return 0xFF000000 | (curve(r) << 16) | (curve(g) << 8) | curve(b);
    }

    private static int curve(int v) {
        return (int) (255.0 * Math.pow(v / 255.0, GAMMA) + 0.5);
    }

    private static final int CHARCOAL = lift(0xFF101010);
    static final int ORANGE = lift(0xFFF2802C);
    private static final int BRICK = lift(0xFFB03F10);
    private static final int BULL_RED = lift(0xFFD82020);
    private static final int PANEL = lift(0xFF2A0F06);

    private final Paint fill = new Paint();   // anti-aliasing off: hard pixel edges
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Listener listener;
    private int mask;
    private float dx, dy, dr, ax, ay, bx, by, br, sx, sy, stx, sty, sr;
    private float px, panelTop;
    private final android.graphics.Path arrowPath = new android.graphics.Path();

    ControlsView(Context ctx, Listener l) {
        super(ctx);
        listener = l;
        fill.setAntiAlias(false);
        text.setColor(ORANGE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTypeface(Typeface.DEFAULT_BOLD);
    }

    @Override protected void onSizeChanged(int w, int h, int ow, int oh) {
        layout(w, h);
    }

    /** Positions every control; the Select/Start row hangs off the picture's bottom edge. */
    private void layout(int w, int h) {
        panelTop = GameView.topInset(w, h) + GameView.pictureHeight(w, h);
        float u = Math.min(w, h) / 6f;
        px = Math.max(2f, u / 20f);

        // The controls must fit between the picture and the bottom (Help/Credits row + gesture bar).
        // Everything below the divider scales by k so shorter screens (16:9 etc.) never overlap.
        float reserve = u * 1.3f;                       // room kept at the bottom
        float need = u * (0.55f * 3.94f + 0.3f + 2.3f); // Select/Start block + gap + D-pad diameter, at k = 1
        float k = Math.min(1f, Math.max(0.55f, (h - reserve - panelTop) / need));

        sr = u * 0.55f * k;
        sy = panelTop + sr * 2.2f;                      // Select/Start hang off the picture's bottom edge
        sx = w / 2f - sr * 2.2f; stx = w / 2f + sr * 2.2f; sty = sy;
        float captionBottom = panelTop + sr * 3.94f;    // below the small SELECT / START captions

        dr = u * 1.15f * k;
        dx = dr + u * 0.3f;
        float bottom = h - reserve;
        float wantedDy = bottom - dr - u * 1.0f;        // preferred spot: lifted a little off the bottom
        dy = Math.max(wantedDy, captionBottom + u * 0.3f * k + dr);

        br = u * 0.6f * k;
        ax = w - br - u * 0.3f;
        ay = Math.max(bottom - br * 2.4f, captionBottom + u * 0.3f * k + br);
        bx = ax - br * 2.6f; by = ay + br * 1.1f;
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        int m = 0;
        int act = e.getActionMasked();
        int skip = act == MotionEvent.ACTION_UP || act == MotionEvent.ACTION_CANCEL ? 0
                : act == MotionEvent.ACTION_POINTER_UP ? e.getActionIndex() : -1;
        for (int i = 0; i < e.getPointerCount(); i++) {
            if (i == skip) continue;
            float x = e.getX(i), y = e.getY(i);
            float ddx = x - dx, ddy = y - dy;
            if (ddx * ddx + ddy * ddy < dr * dr * 1.5f) {
                float t = dr * 0.3f;
                if (ddx < -t) m |= Emulator.LEFT;
                if (ddx > t) m |= Emulator.RIGHT;
                if (ddy < -t) m |= Emulator.UP;
                if (ddy > t) m |= Emulator.DOWN;
            }
            if (hit(x, y, ax, ay, br * 1.2f)) m |= Emulator.A;
            if (hit(x, y, bx, by, br * 1.2f)) m |= Emulator.B;
            if (inBox(x, y, sx, sy, sr * 2.0f, sr * 1.1f)) m |= Emulator.SELECT;
            if (inBox(x, y, stx, sty, sr * 2.0f, sr * 1.1f)) m |= Emulator.START;
        }
        if (m != mask) {
            if ((m & ~mask) != 0) performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
            mask = m;
            listener.onInput(m);
            invalidate();
        }
        return true;
    }

    private static boolean hit(float x, float y, float cx, float cy, float r) {
        return (x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r;
    }

    private static boolean inBox(float x, float y, float cx, float cy, float hw, float hh) {
        return Math.abs(x - cx) < hw && Math.abs(y - cy) < hh;
    }

    private void rect(Canvas c, float l, float t, float r, float b, int color) {
        fill.setColor(color);
        c.drawRect(l, t, r, b, fill);
    }

    /** Box with notched (stepped) corners and a hard orange border. */
    private void pixelBox(Canvas c, float cx, float cy, float hw, float hh, boolean on) {
        float n = px * 2;
        float l = cx - hw, r = cx + hw, t = cy - hh, b = cy + hh;
        rect(c, l + n, t, r - n, b, ORANGE);
        rect(c, l, t + n, r, b - n, ORANGE);
        float i = px * 1.5f;  // border thickness
        int body = on ? BRICK : CHARCOAL;
        rect(c, l + n + i / 2, t + i, r - n - i / 2, b - i, body);
        rect(c, l + i, t + n + i / 2, r - i, b - n - i / 2, body);
    }

    private void label(Canvas c, String s, float cx, float cy, float size, int color) {
        label(c, s, cx, cy, size, color, Float.MAX_VALUE);
    }

    /** Draws centred text, shrinking it so it fits within maxWidth. */
    private void label(Canvas c, String s, float cx, float cy, float size, int color, float maxWidth) {
        text.setTextSize(size);
        float wd = text.measureText(s);
        if (wd > maxWidth) {
            size *= maxWidth / wd;
            text.setTextSize(size);
        }
        text.setColor(color);
        c.drawText(s, cx, cy + size * 0.35f, text);
    }

    /** Solid triangular arrow centred on (cx, cy) pointing along (ux, uy). */
    private void arrow(Canvas c, float cx, float cy, int ux, int uy) {
        float h = px * 3.5f, w = px * 3.5f;      // half-length and half-width
        float tx = cx + ux * h, ty = cy + uy * h;           // tip
        float bx0 = cx - ux * h, by0 = cy - uy * h;         // base centre
        arrowPath.rewind();
        arrowPath.moveTo(tx, ty);
        arrowPath.lineTo(bx0 + uy * w, by0 + ux * w);
        arrowPath.lineTo(bx0 - uy * w, by0 - ux * w);
        arrowPath.close();
        fill.setColor(ORANGE);
        c.drawPath(arrowPath, fill);
    }

    private void dpad(Canvas c) {
        float arm = dr * 0.34f, len = dr, b = px * 1.5f;
        // orange outline of the plus shape, then charcoal body
        rect(c, dx - len - b, dy - arm - b, dx + len + b, dy + arm + b, ORANGE);
        rect(c, dx - arm - b, dy - len - b, dx + arm + b, dy + len + b, ORANGE);
        rect(c, dx - len, dy - arm, dx + len, dy + arm, CHARCOAL);
        rect(c, dx - arm, dy - len, dx + arm, dy + len, CHARCOAL);
        // pressed arms in brick red
        if ((mask & Emulator.LEFT) != 0) rect(c, dx - len, dy - arm, dx - arm, dy + arm, BRICK);
        if ((mask & Emulator.RIGHT) != 0) rect(c, dx + arm, dy - arm, dx + len, dy + arm, BRICK);
        if ((mask & Emulator.UP) != 0) rect(c, dx - arm, dy - len, dx + arm, dy - arm, BRICK);
        if ((mask & Emulator.DOWN) != 0) rect(c, dx - arm, dy + arm, dx + arm, dy + len, BRICK);
        // centre dot and direction arrows
        float d = px * 2;
        rect(c, dx - d, dy - d, dx + d, dy + d, ORANGE);
        float off = (len + arm) / 2f;
        arrow(c, dx, dy - off, 0, -1);
        arrow(c, dx, dy + off, 0, 1);
        arrow(c, dx - off, dy, -1, 0);
        arrow(c, dx + off, dy, 1, 0);
    }

    @Override protected void onDraw(Canvas c) {
        // the picture's bottom edge can move when the top inset arrives
        float top = GameView.topInset(getWidth(), getHeight())
                + GameView.pictureHeight(getWidth(), getHeight());
        if (top != panelTop) layout(getWidth(), getHeight());
        // dark brick panel under the game picture, with an orange rule on top
        rect(c, 0, panelTop, getWidth(), getHeight(), PANEL);
        rect(c, 0, panelTop, getWidth(), panelTop + px * 1.5f, ORANGE);

        dpad(c);

        boolean aOn = (mask & Emulator.A) != 0, bOn = (mask & Emulator.B) != 0;
        pixelBox(c, ax, ay, br, br, aOn);
        pixelBox(c, bx, by, br, br, bOn);
        label(c, "A", ax, ay, br * 1.2f, aOn ? CHARCOAL : BULL_RED);
        label(c, "B", bx, by, br * 1.2f, bOn ? CHARCOAL : ORANGE);

        boolean sOn = (mask & Emulator.SELECT) != 0, tOn = (mask & Emulator.START) != 0;
        pixelBox(c, sx, sy, sr * 2.0f, sr * 0.9f, sOn);
        pixelBox(c, stx, sty, sr * 2.0f, sr * 0.9f, tOn);
        label(c, "தேர்வு", sx, sy, sr * 0.8f, sOn ? CHARCOAL : ORANGE, sr * 3.2f);
        label(c, "தொடங்கு", stx, sty, sr * 0.8f, tOn ? CHARCOAL : ORANGE, sr * 3.2f);
        float small = sr * 0.4f, ey = sy + sr * 0.9f + small * 1.5f;
        label(c, "SELECT", sx, ey, small, ORANGE);
        label(c, "START", stx, ey, small, ORANGE);
    }
}
