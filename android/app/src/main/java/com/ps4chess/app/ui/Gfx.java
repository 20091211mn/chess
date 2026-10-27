package com.ps4chess.app.ui;

/** Minimal 2D drawing surface. Android implements it with Canvas, the tests with Java2D (so screens can be rendered to PNG). Colours are ARGB. */
public interface Gfx {
    int LEFT = 0, CENTER = 1, RIGHT = 2;

    void fillRect(float x, float y, float w, float h, int argb);
    void fillRound(float x, float y, float w, float h, float r, int argb);
    void strokeRound(float x, float y, float w, float h, float r, float sw, int argb);
    void fillCircle(float cx, float cy, float r, int argb);
    void strokeCircle(float cx, float cy, float r, float sw, int argb);
    void fillOval(float cx, float cy, float rx, float ry, int argb);
    void fillPoly(float[] pts, int argb);
    void strokePoly(float[] pts, float sw, int argb);
    /** Polygon filled with a vertical gradient from y0 (top colour) to y1 (bottom colour). */
    void fillPolyGrad(float[] pts, float y0, float y1, int top, int bottom);
    /** y is the vertical centre of the text line. */
    void text(String s, float x, float y, float size, int argb, boolean bold, int align);
    float textWidth(String s, float size, boolean bold);
    void save();
    void translate(float x, float y);
    void scale(float s);
    void restore();
    void clip(float x, float y, float w, float h);
}
