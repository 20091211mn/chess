package com.ps4chess.app;

import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;

import com.ps4chess.app.ui.Gfx;

import java.util.HashMap;
import java.util.Map;
import java.util.WeakHashMap;

/** Gfx on top of android.graphics.Canvas. */
final class AndroidGfx implements Gfx {
    private Canvas c;
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rf = new RectF();
    private final Typeface regular, bold;
    private final Map<float[], Path> pathCache = new WeakHashMap<float[], Path>();   // weak: per-frame polygons vanish, piece shapes stay
    private final Map<String, LinearGradient> gradients = new HashMap<String, LinearGradient>();

    AndroidGfx(Typeface regular, Typeface bold) { this.regular = regular; this.bold = bold; }

    void begin(Canvas canvas) { c = canvas; }

    private Path path(float[] pts) {
        Path path = pathCache.get(pts);
        if (path == null) {
            path = new Path();
            path.moveTo(pts[0], pts[1]);
            for (int i = 2; i + 1 < pts.length; i += 2) path.lineTo(pts[i], pts[i + 1]);
            path.close();
            pathCache.put(pts, path);
        }
        return path;
    }

    private void fill(int argb) { p.setShader(null); p.setStyle(Paint.Style.FILL); p.setColor(argb); }
    private void stroke(int argb, float w) { p.setShader(null); p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(w); p.setColor(argb); }

    public void fillRect(float x, float y, float w, float h, int argb) { fill(argb); c.drawRect(x, y, x + w, y + h, p); }
    public void fillRound(float x, float y, float w, float h, float r, int argb) { fill(argb); rf.set(x, y, x + w, y + h); c.drawRoundRect(rf, r, r, p); }
    public void strokeRound(float x, float y, float w, float h, float r, float sw, int argb) { stroke(argb, sw); rf.set(x + sw / 2, y + sw / 2, x + w - sw / 2, y + h - sw / 2); c.drawRoundRect(rf, r, r, p); }
    public void fillCircle(float cx, float cy, float r, int argb) { fill(argb); c.drawCircle(cx, cy, r, p); }
    public void strokeCircle(float cx, float cy, float r, float sw, int argb) { stroke(argb, sw); c.drawCircle(cx, cy, r, p); }
    public void fillOval(float cx, float cy, float rx, float ry, int argb) { fill(argb); rf.set(cx - rx, cy - ry, cx + rx, cy + ry); c.drawOval(rf, p); }
    public void fillPoly(float[] pts, int argb) { fill(argb); c.drawPath(path(pts), p); }

    public void strokePoly(float[] pts, float sw, int argb) {
        stroke(argb, sw); p.setStrokeJoin(Paint.Join.ROUND); p.setStrokeCap(Paint.Cap.ROUND);
        c.drawPath(path(pts), p);
        p.setStrokeJoin(Paint.Join.MITER); p.setStrokeCap(Paint.Cap.BUTT);
    }

    public void fillPolyGrad(float[] pts, float y0, float y1, int top, int bottom) {
        String key = y0 + ":" + y1 + ":" + top + ":" + bottom;
        LinearGradient lg = gradients.get(key);
        if (lg == null) { lg = new LinearGradient(0, y0, 0, y1, top, bottom, Shader.TileMode.CLAMP); gradients.put(key, lg); }
        p.setStyle(Paint.Style.FILL); p.setColor(0xFFFFFFFF); p.setShader(lg);
        c.drawPath(path(pts), p);
        p.setShader(null);
    }

    public void text(String s, float x, float y, float size, int argb, boolean b, int align) {
        p.setShader(null); p.setStyle(Paint.Style.FILL); p.setColor(argb); p.setTypeface(b ? bold : regular); p.setTextSize(size);
        float w = p.measureText(s), bx = align == CENTER ? x - w / 2 : align == RIGHT ? x - w : x;
        Paint.FontMetrics fm = p.getFontMetrics();
        c.drawText(s, bx, y - (fm.ascent + fm.descent) / 2f, p);
    }

    public float textWidth(String s, float size, boolean b) { p.setTypeface(b ? bold : regular); p.setTextSize(size); return p.measureText(s); }
    public void save() { c.save(); }
    public void translate(float x, float y) { c.translate(x, y); }
    public void scale(float s) { c.scale(s, s); }
    public void restore() { c.restore(); }
    public void clip(float x, float y, float w, float h) { c.clipRect(x, y, x + w, y + h); }
}
