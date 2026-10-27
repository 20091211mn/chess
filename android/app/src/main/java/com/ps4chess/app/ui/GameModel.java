package com.ps4chess.app.ui;

import com.ps4chess.app.core.Ai;
import com.ps4chess.app.core.Engine;
import com.ps4chess.app.core.Game;
import com.ps4chess.app.core.LanSession;
import com.ps4chess.app.core.LanState;
import com.ps4chess.app.core.Pos;

import java.util.ArrayList;
import java.util.List;

/** One game as the UI sees it: a local game (robot / friend) or a mirror of the game running on the PS4. */
public final class GameModel {
    public static final int ROBOT = 0, FRIEND = 1, LAN = 2;
    public static final String[][] TIMES = {{"Unlimited", "", "0", "0"}, {"1 min", "Bullet", "60", "0"}, {"3 min", "Blitz", "180", "0"}, {"3 | 2", "Blitz", "180", "2"},
        {"5 min", "Blitz", "300", "0"}, {"5 | 3", "Blitz", "300", "3"}, {"10 min", "Rapid", "600", "0"}, {"15 | 10", "Rapid", "900", "10"}, {"30 min", "Rapid", "1800", "0"}};

    public final int mode;
    private final Platform pf;
    public final Game game = new Game();
    public int level = 2;
    public int mySide = Engine.WHITE;           // side at the bottom for robot games
    public int moveSeq;                          // bumps whenever the board changed because of a move
    public String lastUci = "";
    public boolean hasEval;
    public int evalCp;
    public String notice = "";
    public long noticeUntil;
    public boolean thinking;

    private final Ai aiRobot = new Ai(), aiHelper = new Ai();
    private int gen;

    // LAN mirror
    public LanSession lan;
    public LanState ls;
    private Pos lanPos = Engine.start();
    private long lanStamp;
    public boolean lanConnected;
    public String lanStatus = "";
    private List<Pos> lanHist;
    private int lanHistPlies = -1;
    private long lastHistRequest;

    public GameModel(Platform pf, int mode) { this.pf = pf; this.mode = mode; }

    // -------------------------------------------------------------------- setup
    public static GameModel robot(Platform pf, int level, int sideChoice, int timeIdx) {
        GameModel m = new GameModel(pf, ROBOT);
        m.level = level;
        m.mySide = sideChoice == 0 ? Engine.WHITE : sideChoice == 1 ? Engine.BLACK : (int) ((pf.now() / 7) & 1);
        m.game.reset(Long.parseLong(TIMES[timeIdx][2]) * 1000, Long.parseLong(TIMES[timeIdx][3]) * 1000);
        m.afterChange(false);
        return m;
    }

    public static GameModel friend(Platform pf, int timeIdx) {
        GameModel m = new GameModel(pf, FRIEND);
        m.game.reset(Long.parseLong(TIMES[timeIdx][2]) * 1000, Long.parseLong(TIMES[timeIdx][3]) * 1000);
        m.afterChange(false);
        return m;
    }

    public static GameModel lan(Platform pf, LanSession session) {
        GameModel m = new GameModel(pf, LAN);
        m.lan = session;
        return m;
    }

    // -------------------------------------------------------------------- queries
    public Pos pos() { return mode == LAN ? lanPos : game.pos; }
    public int plies() { return mode == LAN ? (ls == null ? 0 : ls.plies) : game.plies(); }
    public List<String> san() { return mode == LAN ? (ls == null ? new ArrayList<String>() : ls.san) : game.san; }
    public int result() { return mode == LAN ? (ls == null ? 0 : ls.res) : game.result; }
    public String reason() { return mode == LAN ? (ls == null ? "" : ls.reason) : game.reason; }
    public boolean over() { return result() != 0; }
    public boolean check() { return mode == LAN ? ls != null && ls.check : game.check(); }
    public int drawOffer() { return mode == LAN ? (ls == null ? -1 : ls.draw) : game.drawOffer; }
    public boolean timed() { return mode == LAN ? ls != null && ls.wt >= 0 : game.initMs > 0; }

    /** The side shown at the bottom (the phone's colour on LAN; White for local friend games unless flipped by the UI). */
    public int bottomSide() {
        if (mode == LAN) return ls == null || "s".equals(ls.you) ? Engine.WHITE : ls.you.equals("w") ? Engine.WHITE : Engine.BLACK;
        return mode == ROBOT ? mySide : Engine.WHITE;
    }

    public boolean spectator() { return mode == LAN && (ls == null || "s".equals(ls.you)); }

    public long clock(int color) {
        if (mode != LAN) return game.clk[color];
        if (ls == null) return -1;
        long v = color == Engine.WHITE ? ls.wt : ls.bt;
        if (v >= 0 && ls.res == 0 && ls.plies > 0 && lanPos.side == color) v = Math.max(0, v - (pf.now() - lanStamp));
        return v;
    }

    public boolean myTurn() {
        if (over() || thinking) return false;
        if (mode == FRIEND) return true;
        if (mode == ROBOT) return game.pos.side == mySide;
        return ls != null && !spectator() && ls.err.isEmpty() && ls.you.equals(lanPos.side == Engine.WHITE ? "w" : "b");
    }

    public String name(int color) {
        if (mode == FRIEND) return color == Engine.WHITE ? "White" : "Black";
        if (mode == ROBOT) return color == mySide ? "You" : "Robot - " + Ai.LEVELS[level].name;
        if (spectator()) return color == Engine.WHITE ? "White" : "Black";
        return color == bottomSide() ? "You" : "PS4";
    }

    public String title() { return mode == ROBOT ? "vs Robot - " + Ai.LEVELS[level].name : mode == FRIEND ? "Play a Friend" : "Play on PS4"; }

    public boolean canUndo() { return mode != LAN && game.plies() > 0 && !thinking; }
    public boolean canHint() { return !over() && myTurn() && mode != FRIEND || (mode == FRIEND && !over()); }

    public List<Integer> movesFrom(int sq) {
        List<Integer> r = new ArrayList<Integer>();
        if (over()) return r;
        for (int m : Engine.legalList(pos())) if (Engine.from(m) == sq) r.add(m);
        return r;
    }

    public void notice(String s) { notice = s; noticeUntil = pf.now() + 2600; }

    // -------------------------------------------------------------------- actions
    /** Plays a move for the user. Returns false if it was not accepted. */
    public boolean submit(String uci) {
        if (!myTurn()) return false;
        if (mode == LAN) {
            if (Engine.parseUci(lanPos, uci) < 0) return false;
            lan.send("move " + uci);
            return true;
        }
        int before = game.plies();
        if (!game.playUci(uci)) return false;
        if (game.plies() == before) return false;
        lastUci = uci; moveSeq++;
        afterChange(true);
        return true;
    }

    private void afterChange(boolean moved) {
        gen++;
        refreshEval();
        if (mode == ROBOT && !game.over() && game.pos.side != mySide) startRobot();
    }

    private void startRobot() {
        final int myGen = gen;
        final long t0 = pf.now();
        thinking = true;
        final Pos snapshot = game.pos.copy();
        final int[] result = {-1};
        pf.async(new Runnable() { public void run() { synchronized (aiRobot) { result[0] = aiRobot.think(snapshot, Ai.LEVELS[level]); } } }, new Runnable() {
            public void run() {
                long wait = Math.max(0, 500 - (pf.now() - t0));
                pf.ui(new Runnable() {
                    public void run() {
                        if (myGen != gen) return;       // game was undone / restarted meanwhile
                        thinking = false;
                        if (result[0] >= 0 && game.play(result[0])) { lastUci = Engine.uci(result[0]); moveSeq++; afterChange(true); }
                        pf.invalidate();
                    }
                }, wait);
            }
        });
    }

    private void refreshEval() {
        if (mode == LAN) return;
        final int myGen = gen;
        final Pos snapshot = game.pos.copy();
        final int[] v = {0};
        pf.async(new Runnable() { public void run() { synchronized (aiHelper) { v[0] = aiHelper.evalWhiteCp(snapshot, 30000); } } }, new Runnable() {
            public void run() { if (myGen == gen) { evalCp = v[0]; hasEval = true; pf.invalidate(); } }
        });
    }

    public interface MoveCallback { void onMove(String uci); }

    public void hint(final MoveCallback cb) {
        final Pos snapshot = pos().copy();
        final int[] m = {-1};
        pf.async(new Runnable() { public void run() { m[0] = new Ai().think(snapshot, Ai.LEVELS[4]); } }, new Runnable() {
            public void run() { if (m[0] >= 0) cb.onMove(Engine.uci(m[0])); }
        });
    }

    public void undo() {
        if (!canUndo()) return;
        game.undo();
        if (mode == ROBOT && game.plies() > 0 && game.pos.side != mySide) game.undo();
        thinking = false;
        lastUci = game.plies() > 0 ? Engine.uci(game.moves.get(game.plies() - 1)) : "";
        moveSeq++;
        afterChange(false);
    }

    public void resign() {
        if (over()) return;
        if (mode == LAN) { lan.send("resign"); return; }
        game.resign(mode == ROBOT ? mySide : game.pos.side);
        gen++; thinking = false;
    }

    /** Offers a draw. Robot answers immediately; a friend answers on the same phone; the PS4 answers over the network. */
    public void offerDraw() {
        if (over() || drawOffer() >= 0) return;
        if (mode == LAN) { lan.send("draw"); notice("Draw offer sent to the PS4"); return; }
        if (mode == FRIEND) { game.offerDraw(game.pos.side); return; }
        int cp = new Ai().evalWhiteCp(game.pos, 30000), robotCp = mySide == Engine.WHITE ? -cp : cp;
        game.offerDraw(mySide);
        boolean accept = robotCp <= 30;
        game.answerDraw(accept);
        if (!accept) notice("Robot declined the draw");
        else gen++;
    }

    public void answerDraw(boolean accept) {
        if (mode == LAN) { lan.send(accept ? "draw_yes" : "draw_no"); return; }
        game.answerDraw(accept);
        if (!accept) notice("Draw declined");
    }

    public void rematch(int timeIdx) {
        if (mode == LAN) { lan.send("new"); return; }
        if (mode == ROBOT) mySide ^= 1;
        game.reset(Long.parseLong(TIMES[timeIdx][2]) * 1000, Long.parseLong(TIMES[timeIdx][3]) * 1000);
        lastUci = ""; moveSeq++; thinking = false; hasEval = false;
        afterChange(false);
    }

    /** Advances local clocks. */
    public void tick(long dtMs) {
        if (mode == LAN || game.over()) return;
        boolean was = game.over();
        game.tick(dtMs);
        if (!was && game.over()) { gen++; thinking = false; }
    }

    // -------------------------------------------------------------------- LAN
    public void onLanState(LanState s) {
        boolean fenChanged = ls == null || !s.fen.equals(ls.fen);
        if (ls != null && s.plies < ls.plies) lanHist = null;
        ls = s;
        lanStamp = pf.now();
        Pos p = Engine.fromFen(s.fen);
        if (p != null) lanPos = p;
        if (fenChanged) { lastUci = s.last; moveSeq++; }
        if (s.hasEval) { hasEval = true; evalCp = s.evalCp; }
        pf.invalidate();
    }

    public void onLanHistory(List<String> fens) {
        List<Pos> l = new ArrayList<Pos>();
        for (String f : fens) { Pos p = Engine.fromFen(f); if (p != null) l.add(p); }
        lanHist = l; lanHistPlies = l.size() - 1;
        pf.invalidate();
    }

    /** Positions of the whole game (index = ply, last = current) or null if not available yet (LAN: requested lazily). */
    public List<Pos> positions() {
        List<Pos> l = new ArrayList<Pos>();
        if (mode != LAN) { l.addAll(game.hist); l.add(game.pos); return l; }
        if (lanHist != null && lanHistPlies == plies()) return lanHist;
        if (lan != null && pf.now() - lastHistRequest > 600) { lastHistRequest = pf.now(); lan.send("hist"); }
        return null;
    }
}
