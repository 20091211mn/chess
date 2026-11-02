package com.ps4chess.app.ui;

import com.ps4chess.app.core.Engine;

/** Draws the vector chess pieces (same artwork as the PS4 game). */
public final class PieceArt {
    private PieceArt() {}
    private static final float[][][] FILL = new float[7][][], DETAIL = new float[7][][];

    static {
        for (int t = 1; t <= 6; t++) {
            int nf = 0, nd = 0;
            for (String s : PieceData.SHAPES[t]) if (s.startsWith("d:")) nd++; else nf++;
            FILL[t] = new float[nf][]; DETAIL[t] = new float[nd][];
            int fi = 0, di = 0;
            for (String s : PieceData.SHAPES[t]) {
                boolean d = s.startsWith("d:");
                String[] pts = (d ? s.substring(2) : s).split(" ");
                float[] a = new float[pts.length * 2];
                for (int i = 0; i < pts.length; i++) { int c = pts[i].indexOf(','); a[2 * i] = Float.parseFloat(pts[i].substring(0, c)); a[2 * i + 1] = Float.parseFloat(pts[i].substring(c + 1)); }
                if (d) DETAIL[t][di++] = a; else FILL[t][fi++] = a;
            }
        }
    }

    /** Draws piece code pc (Engine.piece) with its top-left at x,y in a size x size box. alpha 0..1. */
    public static void draw(Gfx g, int pc, float x, float y, float size, float alpha) {
        int t = Engine.type(pc);
        if (t < 1 || t > 6) return;
        boolean white = Engine.color(pc) == Engine.WHITE;
        int line = Theme.mulAlpha(white ? 0xFF1D1D1F : 0xFF0B0B0C, alpha);
        int top = Theme.mulAlpha(white ? 0xFFFFFFFF : 0xFF7A7A80, alpha), bot = Theme.mulAlpha(white ? 0xFFCDCDD3 : 0xFF2A2A2E, alpha);
        g.save();
        g.translate(x, y);
        g.scale(size / 100f);
        for (float[] p : FILL[t]) { g.fillPoly(p, line); g.strokePoly(p, 3.6f, line); }
        for (float[] p : FILL[t]) g.fillPolyGrad(p, 0, 95, top, bot);
        for (float[] p : DETAIL[t]) g.fillPoly(p, line);
        g.restore();
    }

    /** Soft contact shadow under a piece. */
    public static void shadow(Gfx g, float x, float y, float size, float alpha) {
        g.fillOval(x + size * 0.5f, y + size * 0.86f, size * 0.31f, size * 0.07f, Theme.alpha(0xFF000000, (int) (60 * alpha)));
    }
}
