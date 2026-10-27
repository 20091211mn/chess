package com.ps4chess.app.core;

/** Board position. Squares 0..63 with a1 = 0; piece code = type | color << 3 (see Engine). */
public final class Pos {
    public final byte[] b = new byte[64];
    public int side, castle, ep = -1, half, full = 1;

    public Pos copy() { Pos p = new Pos(); p.copyFrom(this); return p; }

    public void copyFrom(Pos o) {
        System.arraycopy(o.b, 0, b, 0, 64);
        side = o.side; castle = o.castle; ep = o.ep; half = o.half; full = o.full;
    }
}
