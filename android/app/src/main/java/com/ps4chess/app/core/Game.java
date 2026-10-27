package com.ps4chess.app.core;

import java.util.ArrayList;
import java.util.List;

/** A game with history, undo, clocks, draw rules and draw offers. Mirrors the C++ Game. */
public final class Game {
    public static final int NONE = 0, WHITE_WINS = 1, BLACK_WINS = 2, DRAW = 3;

    public Pos pos;
    public final List<Pos> hist = new ArrayList<Pos>();
    public final List<Integer> moves = new ArrayList<Integer>();
    public final List<String> san = new ArrayList<String>();
    private final List<Long> keys = new ArrayList<Long>();
    private final List<long[]> clockSnap = new ArrayList<long[]>();
    public int result = NONE;
    public String reason = "";
    public final long[] clk = new long[2];
    public long initMs, incMs;
    public int drawOffer = -1;

    public Game() { reset(0, 0); }

    public void reset(long initTimeMs, long incrementMs) {
        pos = Engine.start();
        hist.clear(); moves.clear(); san.clear(); keys.clear(); clockSnap.clear();
        keys.add(Engine.posKey(pos));
        result = NONE; reason = ""; drawOffer = -1;
        initMs = initTimeMs; incMs = incrementMs; clk[0] = clk[1] = initTimeMs;
    }

    public boolean loadFen(String fen) {
        Pos p = Engine.fromFen(fen);
        if (p == null) return false;
        pos = p;
        hist.clear(); moves.clear(); san.clear(); keys.clear(); clockSnap.clear();
        keys.add(Engine.posKey(pos));
        result = NONE; reason = ""; drawOffer = -1;
        updateStatus();
        return true;
    }

    public boolean over() { return result != NONE; }
    public int plies() { return moves.size(); }
    public boolean check() { return Engine.inCheck(pos, pos.side); }

    private void updateStatus() {
        if (!Engine.hasLegal(pos)) {
            if (Engine.inCheck(pos, pos.side)) { result = pos.side == Engine.WHITE ? BLACK_WINS : WHITE_WINS; reason = "checkmate"; }
            else { result = DRAW; reason = "stalemate"; }
            return;
        }
        if (Engine.insufficientMaterial(pos)) { result = DRAW; reason = "insufficient material"; return; }
        if (pos.half >= 100) { result = DRAW; reason = "50-move rule"; return; }
        int cnt = 0; long k = keys.get(keys.size() - 1);
        for (int i = 0; i < keys.size(); i++) if (keys.get(i) == k) cnt++;
        if (cnt >= 3) { result = DRAW; reason = "threefold repetition"; }
    }

    public boolean play(int m) {
        if (over()) return false;
        int real = -1;
        for (int c : Engine.legalList(pos)) if (Engine.from(c) == Engine.from(m) && Engine.to(c) == Engine.to(m) && Engine.promo(c) == Engine.promo(m)) { real = c; break; }
        if (real < 0) return false;
        san.add(Engine.san(pos, real));
        hist.add(pos.copy());
        moves.add(real);
        clockSnap.add(new long[] {clk[0], clk[1]});
        int mover = pos.side;
        Pos q = new Pos();
        Engine.make(pos, real, q);
        pos = q;
        drawOffer = -1;
        if (initMs > 0 && moves.size() > 1) clk[mover] += incMs;
        keys.add(Engine.posKey(pos));
        updateStatus();
        return true;
    }

    public boolean playUci(String u) {
        int m = Engine.parseUci(pos, u);
        return m >= 0 && play(m);
    }

    public boolean undo() {
        if (moves.isEmpty()) return false;
        pos = hist.remove(hist.size() - 1);
        moves.remove(moves.size() - 1); san.remove(san.size() - 1); keys.remove(keys.size() - 1);
        long[] c = clockSnap.remove(clockSnap.size() - 1);
        clk[0] = c[0]; clk[1] = c[1];
        result = NONE; reason = ""; drawOffer = -1;
        return true;
    }

    public boolean offerDraw(int side) {
        if (over() || drawOffer >= 0) return false;
        drawOffer = side; return true;
    }

    public void answerDraw(boolean accept) {
        if (drawOffer < 0 || over()) return;
        if (accept) { result = DRAW; reason = "agreement"; }
        drawOffer = -1;
    }

    public void resign(int side) {
        if (over()) return;
        result = side == Engine.WHITE ? BLACK_WINS : WHITE_WINS; reason = "resignation";
    }

    /** Advances the running clock; true if a flag fell. */
    public boolean tick(long ms) {
        if (initMs <= 0 || over() || moves.isEmpty()) return false;
        int s = pos.side;
        clk[s] -= ms;
        if (clk[s] > 0) return false;
        clk[s] = 0;
        boolean oppMaterial = false;
        for (int i = 0; i < 64; i++) { int pc = pos.b[i]; if (pc != 0 && Engine.color(pc) == (s ^ 1) && Engine.type(pc) != Engine.KING) oppMaterial = true; }
        if (!oppMaterial) { result = DRAW; reason = "timeout vs insufficient material"; }
        else { result = s == Engine.WHITE ? BLACK_WINS : WHITE_WINS; reason = "timeout"; }
        return true;
    }

    public String resultText() { return result == WHITE_WINS ? "White wins" : result == BLACK_WINS ? "Black wins" : result == DRAW ? "Draw" : ""; }
}
