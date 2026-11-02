package com.ps4chess.app.ui;

import com.ps4chess.app.Discovery;
import com.ps4chess.app.core.Ai;
import com.ps4chess.app.core.Engine;
import com.ps4chess.app.core.LanSession;
import com.ps4chess.app.core.LanState;
import com.ps4chess.app.core.Pos;

import java.util.ArrayList;
import java.util.List;

/** The whole app UI (home, setup, PS4 connection, game, settings) drawn through Gfx. Platform-independent and unit-testable. */
public final class ChessApp {
    private enum Screen { HOME, SETUP, LAN, GAME, SETTINGS }
    private enum Dlg { NONE, INFO, PROMO, OVER, CONFIRM }

    private static final class Hit { final String id; final float x, y, w, h; final boolean modal; Hit(String id, float x, float y, float w, float h, boolean modal) { this.id = id; this.x = x; this.y = y; this.w = w; this.h = h; this.modal = modal; } }

    private final Platform pf;
    private float W = 390, H = 844, u = 1;
    private Screen screen = Screen.HOME;
    private final List<Hit> hits = new ArrayList<Hit>();
    private boolean drawingDialog;

    // settings
    private int theme; private boolean optHints = true, optSound = true, optVibrate = true, optCoords = true, optAnim = true, optAutoQueen;
    // setup
    private int setupMode = GameModel.ROBOT, setupLevel = 2, setupSide = 0, setupTime = 0;
    // lan screen
    private final List<String> foundHosts = new ArrayList<String>();
    private String lanMsg = "";
    private int lanSearchId;
    private boolean searching;
    // game
    private GameModel gm;
    private int seenSeq = -1, viewPly = -1, sel = -1;
    private boolean manualFlip, dragging, dragMoved;
    private float dragX, dragY, downX, downY;
    private String hintFrom, hintTo; private long hintUntil;
    private byte[] prevBoard = new byte[64];
    private int animFrom = -1, animTo = -1, animPiece, animCap, animRook = -1, animRookTo = -1, animRookPiece; private long animStart;
    private float evalDisp = 0.5f;
    private long lastNow;
    private float movesScroll;
    private boolean pendingNav; private int pendingNavTo;
    private String overKey = "", offerKey = "";
    // dialog
    private Dlg dlg = Dlg.NONE;
    private String dlgTitle = "", dlgMsg = "";
    private String[] dlgLabels = new String[0]; private Runnable[] dlgActs = new Runnable[0];
    private List<Integer> promoMoves = new ArrayList<Integer>();
    private String downHit;
    private float boardX, boardY, cell;

    public ChessApp(Platform pf) {
        this.pf = pf;
        theme = parse(pf.pref("theme", "0"), 0) % Theme.NAMES.length;
        optHints = pf.pref("hints", "1").equals("1"); optSound = pf.pref("sound", "1").equals("1"); optVibrate = pf.pref("vib", "1").equals("1");
        optCoords = pf.pref("coords", "1").equals("1"); optAnim = pf.pref("anim", "1").equals("1"); optAutoQueen = pf.pref("autoq", "0").equals("1");
    }

    private static int parse(String s, int d) { try { return Integer.parseInt(s); } catch (NumberFormatException e) { return d; } }
    private void save(String k, boolean v) { pf.setPref(k, v ? "1" : "0"); }

    public void resize(int w, int h) { W = w; H = h; u = w / 390f; }

    // ================================================================== drawing
    public void draw(Gfx g, long now) {
        long dt = lastNow == 0 ? 0 : Math.min(200, now - lastNow);
        lastNow = now;
        hits.clear();
        if (gm != null && screen == Screen.GAME) { gm.tick(dt); watchGame(now); }
        g.fillRect(0, 0, W, H, Theme.BG);
        switch (screen) {
            case HOME: drawHome(g); break;
            case SETUP: drawSetup(g); break;
            case LAN: drawLan(g); break;
            case SETTINGS: drawSettings(g); break;
            default: drawGame(g, now); break;
        }
        if (dlg != Dlg.NONE) drawDialog(g);
        if (gm != null && screen == Screen.GAME && gm.noticeUntil > pf.now() && gm.notice.length() > 0) toast(g, gm.notice);
        if (screen == Screen.GAME && gm != null && !gm.over() && gm.timed()) pf.invalidate();
        if (animActive(now) || (gm != null && (gm.thinking || screen == Screen.LAN || Math.abs(evalDisp - target()) > 0.003f))) pf.invalidate();
    }

    private void toast(Gfx g, String s) {
        float w = g.textWidth(s, 14 * u, true) + 36 * u;
        g.fillRound((W - w) / 2, 52 * u, w, 36 * u, 18 * u, 0xEE14130F);
        g.text(s, W / 2, 70 * u, 14 * u, Theme.TXT, true, Gfx.CENTER);
    }

    private void hit(String id, float x, float y, float w, float h) { hits.add(new Hit(id, x, y, w, h, drawingDialog)); }

    private void header(Gfx g, String title, String back) {
        g.fillRect(0, 0, W, 56 * u, Theme.PANEL);
        chevron(g, 28 * u, 28 * u, 9 * u, Theme.TXT);
        hit(back, 0, 0, 72 * u, 56 * u);
        g.text(title, 52 * u, 28 * u, 20 * u, Theme.TXT, true, Gfx.LEFT);
    }

    private void chevron(Gfx g, float cx, float cy, float s, int c) {
        g.fillPoly(new float[] {cx + s * 0.35f, cy - s, cx + s * 0.35f - s * 0.28f, cy - s, cx - s * 0.6f, cy, cx + s * 0.35f - s * 0.28f, cy + s, cx + s * 0.35f, cy + s, cx - s * 0.2f, cy}, c);
    }

    private void card(Gfx g, String id, float y, float h, String title, String sub, int accent) {
        g.fillRound(16 * u, y, W - 32 * u, h, 16 * u, Theme.CARD);
        g.fillRound(16 * u, y, 5 * u, h, 2.5f * u, accent);
        g.text(title, 98 * u, y + h * 0.37f, 18 * u, Theme.TXT, true, Gfx.LEFT);
        g.text(sub, 98 * u, y + h * 0.65f, 12.5f * u, Theme.MUT, false, Gfx.LEFT);
        float cx = W - 36 * u, cy = y + h / 2;
        g.fillPoly(new float[] {cx - 4 * u, cy - 8 * u, cx - 7 * u, cy - 8 * u, cx + 3 * u, cy, cx - 7 * u, cy + 8 * u, cx - 4 * u, cy + 8 * u, cx + 6 * u, cy}, Theme.MUT);
        hit(id, 16 * u, y, W - 32 * u, h);
    }

    private void iconBox(Gfx g, float x, float y, float s, int bg) { g.fillRound(x, y, s, s, 14 * u, bg); }

    private void drawHome(Gfx g) {
        float y = 70 * u;
        iconBox(g, 22 * u, y - 30 * u, 60 * u, Theme.GREEN_D);
        PieceArt.draw(g, Engine.piece(Engine.KING, Engine.WHITE), 26 * u, y - 26 * u, 52 * u, 1f);
        g.text("PS4 Chess", 98 * u, y - 8 * u, 30 * u, Theme.TXT, true, Gfx.LEFT);
        g.text("Play on your phone or with your PS4", 98 * u, y + 17 * u, 13 * u, Theme.MUT, false, Gfx.LEFT);
        float cy = 140 * u, ch = 92 * u, gap = 12 * u;
        card(g, "home_robot", cy, ch, "Play Computer", "5 levels - hints - analysis", Theme.GREEN);
        iconBox(g, 28 * u, cy + 18 * u, 56 * u, Theme.GREEN_D);
        PieceArt.draw(g, Engine.piece(Engine.KNIGHT, Engine.BLACK), 28 * u, cy + 18 * u, 56 * u, 1f);
        cy += ch + gap;
        card(g, "home_friend", cy, ch, "Play a Friend", "Pass and play on this phone", 0xFFE8A33D);
        iconBox(g, 28 * u, cy + 18 * u, 56 * u, 0xFF8A6A2E);
        PieceArt.draw(g, Engine.piece(Engine.PAWN, Engine.WHITE), 26 * u, cy + 24 * u, 36 * u, 1f);
        PieceArt.draw(g, Engine.piece(Engine.PAWN, Engine.BLACK), 46 * u, cy + 28 * u, 36 * u, 1f);
        cy += ch + gap;
        card(g, "home_lan", cy, ch, "Play on PS4", "Find your console over Wi-Fi", Theme.BLUE);
        iconBox(g, 28 * u, cy + 18 * u, 56 * u, 0xFF2E5A7A);
        gamepad(g, 56 * u, cy + 46 * u, 1.0f);
        cy += ch + gap;
        card(g, "home_settings", cy, ch, "Settings", "Board, sounds and options", 0xFF8B8987);
        iconBox(g, 28 * u, cy + 18 * u, 56 * u, 0xFF4A4744);
        for (int i = 0; i < 3; i++) { float ly = cy + 30 * u + i * 12 * u; g.fillRound(38 * u, ly, 36 * u, 3 * u, 1.5f * u, Theme.TXT); g.fillCircle(46 * u + i * 10 * u, ly + 1.5f * u, 5 * u, Theme.GREEN); }
        g.text("Chess engine and pieces are built in - no internet needed", W / 2, H - 28 * u, 11.5f * u, Theme.MUT, false, Gfx.CENTER);
    }

    private void gamepad(Gfx g, float cx, float cy, float s) {
        g.fillRound(cx - 22 * u * s, cy - 11 * u * s, 44 * u * s, 24 * u * s, 11 * u * s, Theme.TXT);
        g.fillRect(cx - 16 * u * s, cy - 2 * u * s, 10 * u * s, 3 * u * s, 0xFF2E5A7A); g.fillRect(cx - 12.5f * u * s, cy - 5.5f * u * s, 3 * u * s, 10 * u * s, 0xFF2E5A7A);
        g.fillCircle(cx + 11 * u * s, cy - 4 * u * s, 2.6f * u * s, 0xFF2E5A7A); g.fillCircle(cx + 11 * u * s, cy + 3 * u * s, 2.6f * u * s, 0xFF2E5A7A);
        g.fillCircle(cx + 7.5f * u * s, cy - 0.5f * u * s, 2.6f * u * s, 0xFF2E5A7A); g.fillCircle(cx + 14.5f * u * s, cy - 0.5f * u * s, 2.6f * u * s, 0xFF2E5A7A);
    }

    private void chip(Gfx g, String id, float x, float y, float w, float h, String label, String small, boolean on) {
        g.fillRound(x, y, w, h, 12 * u, on ? Theme.GREEN_D : Theme.CARD);
        if (on) g.strokeRound(x, y, w, h, 12 * u, 2 * u, Theme.GREEN);
        if (small != null && small.length() > 0) {
            float fs = 15 * u, tw = g.textWidth(label, fs, true);
            if (tw > w - 8 * u) fs = fs * (w - 8 * u) / tw;
            g.text(label, x + w / 2, y + h * 0.42f, fs, Theme.TXT, true, Gfx.CENTER);
            g.text(small, x + w / 2, y + h * 0.75f, 10.5f * u, on ? 0xFFD6EBC2 : Theme.MUT, false, Gfx.CENTER);
        } else g.text(label, x + w / 2, y + h / 2, 14 * u, Theme.TXT, true, Gfx.CENTER);
        hit(id, x, y, w, h);
    }

    private void drawSetup(Gfx g) {
        boolean robot = setupMode == GameModel.ROBOT;
        header(g, robot ? "Play Computer" : "Play a Friend", "back_home");
        float y = 78 * u, x = 16 * u, fw = W - 32 * u;
        if (robot) {
            g.text("Opponent level", x, y, 15 * u, Theme.SUB, true, Gfx.LEFT); y += 20 * u;
            float cw = (fw - 4 * 8 * u) / 5;
            for (int i = 0; i < 5; i++) chip(g, "level" + i, x + i * (cw + 8 * u), y, cw, 54 * u, Ai.LEVELS[i].name, String.valueOf(i + 1), setupLevel == i);
            y += 54 * u + 8 * u;
            g.text(Ai.LEVELS[setupLevel].name + " - " + new String[] {"just learning the rules", "makes mistakes", "a fair fight", "plays tactically", "thinks deeply"}[setupLevel], x, y + 6 * u, 12.5f * u, Theme.MUT, false, Gfx.LEFT);
            y += 34 * u;
            g.text("Play as", x, y, 15 * u, Theme.SUB, true, Gfx.LEFT); y += 20 * u;
            float sw = (fw - 2 * 10 * u) / 3;
            String[] names = {"White", "Random", "Black"};
            for (int i = 0; i < 3; i++) {
                float sx = x + i * (sw + 10 * u);
                g.fillRound(sx, y, sw, 78 * u, 14 * u, setupSide == i ? Theme.GREEN_D : Theme.CARD);
                if (setupSide == i) g.strokeRound(sx, y, sw, 78 * u, 14 * u, 2 * u, Theme.GREEN);
                if (i == 1) { PieceArt.draw(g, Engine.piece(Engine.KING, Engine.WHITE), sx + sw / 2 - 30 * u, y + 6 * u, 36 * u, 1f); PieceArt.draw(g, Engine.piece(Engine.KING, Engine.BLACK), sx + sw / 2 - 6 * u, y + 6 * u, 36 * u, 1f); }
                else PieceArt.draw(g, Engine.piece(Engine.KING, i == 0 ? Engine.WHITE : Engine.BLACK), sx + sw / 2 - 20 * u, y + 4 * u, 40 * u, 1f);
                g.text(names[i], sx + sw / 2, y + 62 * u, 13 * u, Theme.TXT, true, Gfx.CENTER);
                hit("side" + i, sx, y, sw, 78 * u);
            }
            y += 78 * u + 22 * u;
        } else { g.text("Two players share this phone. White starts at the bottom;", x, y + 4 * u, 12.5f * u, Theme.MUT, false, Gfx.LEFT); g.text("use Flip to turn the board around.", x, y + 22 * u, 12.5f * u, Theme.MUT, false, Gfx.LEFT); y += 52 * u; }
        g.text("Time control", x, y, 15 * u, Theme.SUB, true, Gfx.LEFT); y += 20 * u;
        float tw = (fw - 2 * 8 * u) / 3, th = 50 * u;
        for (int i = 0; i < GameModel.TIMES.length; i++) {
            float tx = x + (i % 3) * (tw + 8 * u), ty = y + (i / 3) * (th + 8 * u);
            chip(g, "time" + i, tx, ty, tw, th, GameModel.TIMES[i][0], GameModel.TIMES[i][1], setupTime == i);
        }
        float by = H - 78 * u;
        g.fillRound(x, by, fw, 56 * u, 14 * u, Theme.GREEN);
        g.text("Play", W / 2, by + 28 * u, 21 * u, Theme.TXT, true, Gfx.CENTER);
        hit("start_game", x, by, fw, 56 * u);
    }

    private void drawLan(Gfx g) {
        header(g, "Play on PS4", "back_home");
        float x = 16 * u, fw = W - 32 * u, y = 76 * u;
        g.fillRound(x, y, fw, 112 * u, 16 * u, Theme.CARD);
        gamepad(g, x + 44 * u, y + 40 * u, 1.3f);
        g.text("1. Open PS4 Chess on the console", x + 84 * u, y + 24 * u, 13.5f * u, Theme.TXT, false, Gfx.LEFT);
        g.text("2. Choose  Play on Phone (LAN)", x + 84 * u, y + 46 * u, 13.5f * u, Theme.TXT, false, Gfx.LEFT);
        g.text("3. Tap your PS4 below", x + 84 * u, y + 68 * u, 13.5f * u, Theme.TXT, false, Gfx.LEFT);
        g.text("Same Wi-Fi as this phone", x + 84 * u, y + 92 * u, 12 * u, Theme.MUT, false, Gfx.LEFT);
        y += 128 * u;
        long t = pf.now();
        if (searching) { for (int i = 0; i < 3; i++) g.fillCircle(x + 8 * u + i * 16 * u, y + 10 * u, 4.5f * u * (0.6f + 0.4f * (float) Math.sin(t / 220.0 - i)), Theme.GREEN); g.text("Looking for your PS4 ...", x + 62 * u, y + 10 * u, 14 * u, Theme.SUB, false, Gfx.LEFT); y += 28 * u; }
        else if (foundHosts.isEmpty()) { g.text(lanMsg.length() > 0 ? lanMsg : "No PS4 found yet", x, y + 10 * u, 13.5f * u, Theme.RED, false, Gfx.LEFT); y += 28 * u; }
        for (int i = 0; i < foundHosts.size(); i++) {
            String h = foundHosts.get(i);
            g.fillRound(x, y, fw, 64 * u, 14 * u, Theme.CARD); g.fillRound(x, y, 5 * u, 64 * u, 2.5f * u, Theme.GREEN);
            g.fillCircle(x + 32 * u, y + 32 * u, 7 * u, Theme.GREEN);
            g.text("PS4 Chess", x + 52 * u, y + 24 * u, 16 * u, Theme.TXT, true, Gfx.LEFT);
            g.text(h, x + 52 * u, y + 45 * u, 12.5f * u, Theme.MUT, false, Gfx.LEFT);
            g.fillRound(x + fw - 96 * u, y + 14 * u, 84 * u, 36 * u, 10 * u, Theme.GREEN);
            g.text("Connect", x + fw - 54 * u, y + 32 * u, 14 * u, Theme.TXT, true, Gfx.CENTER);
            hit("connect:" + h, x, y, fw, 64 * u);
            y += 74 * u;
        }
        float by = H - 142 * u;
        String last = pf.pref("lanhost", "");
        if (last.length() > 0) { g.fillRound(x, by, fw, 48 * u, 12 * u, Theme.CARD); g.text("Reconnect to " + last, W / 2, by + 24 * u, 14 * u, Theme.TXT, true, Gfx.CENTER); hit("connect:" + last, x, by, fw, 48 * u); by += 58 * u; }
        g.fillRound(x, by, fw / 2 - 5 * u, 48 * u, 12 * u, 0xFF454340); g.text("Search again", x + fw / 4 - 2.5f * u, by + 24 * u, 14 * u, Theme.TXT, true, Gfx.CENTER); hit("lan_search", x, by, fw / 2 - 5 * u, 48 * u);
        g.fillRound(x + fw / 2 + 5 * u, by, fw / 2 - 5 * u, 48 * u, 12 * u, 0xFF454340); g.text("Type address", x + fw * 0.75f + 2.5f * u, by + 24 * u, 14 * u, Theme.TXT, true, Gfx.CENTER); hit("lan_manual", x + fw / 2 + 5 * u, by, fw / 2 - 5 * u, 48 * u);
    }

    private void toggleRow(Gfx g, String id, float y, String label, boolean on) {
        float x = 16 * u, fw = W - 32 * u;
        g.fillRound(x, y, fw, 54 * u, 14 * u, Theme.CARD);
        g.text(label, x + 18 * u, y + 27 * u, 15 * u, Theme.TXT, false, Gfx.LEFT);
        float tx = x + fw - 66 * u;
        g.fillRound(tx, y + 14 * u, 48 * u, 26 * u, 13 * u, on ? Theme.GREEN : 0xFF5A5754);
        g.fillCircle(on ? tx + 35 * u : tx + 13 * u, y + 27 * u, 10 * u, Theme.TXT);
        hit(id, x, y, fw, 54 * u);
    }

    private void drawSettings(Gfx g) {
        header(g, "Settings", "back_home");
        float x = 16 * u, fw = W - 32 * u, y = 76 * u;
        g.fillRound(x, y, fw, 128 * u, 14 * u, Theme.CARD);
        g.text("Board", x + 18 * u, y + 24 * u, 15 * u, Theme.TXT, false, Gfx.LEFT);
        float sw = (fw - 36 * u - 3 * 10 * u) / 4;
        for (int i = 0; i < 4; i++) {
            float sx = x + 18 * u + i * (sw + 10 * u), sy = y + 44 * u;
            g.fillRect(sx, sy, sw / 2, 36 * u, Theme.LIGHT[i]); g.fillRect(sx + sw / 2, sy, sw / 2, 36 * u, Theme.DARK[i]);
            g.fillRect(sx, sy + 36 * u, sw / 2, 0, 0); // keep strict ordering of calls simple
            if (theme == i) g.strokeRound(sx - 3 * u, sy - 3 * u, sw + 6 * u, 42 * u, 6 * u, 2.5f * u, Theme.GREEN);
            g.text(Theme.NAMES[i], sx + sw / 2, y + 100 * u, 12 * u, theme == i ? Theme.TXT : Theme.MUT, theme == i, Gfx.CENTER);
            hit("theme" + i, sx - 3 * u, sy - 3 * u, sw + 6 * u, 56 * u);
        }
        y += 140 * u;
        String[] ids = {"opt_hints", "opt_sound", "opt_vib", "opt_coords", "opt_anim", "opt_autoq"};
        String[] labels = {"Show legal moves", "Sounds", "Vibration", "Board coordinates", "Move animations", "Auto-queen promotion"};
        boolean[] vals = {optHints, optSound, optVibrate, optCoords, optAnim, optAutoQueen};
        for (int i = 0; i < ids.length; i++) toggleRow(g, ids[i], y + i * 62 * u, labels[i], vals[i]);
    }

    // ------------------------------------------------------------------ game screen
    private boolean flipped() { return (gm.bottomSide() == Engine.BLACK) != manualFlip; }
    private boolean live() { return viewPly < 0; }
    private float target() { if (gm == null) return 0.5f; int cp = gm.result() == 1 ? 30000 : gm.result() == 2 ? -30000 : gm.evalCp; return (float) (1.0 / (1.0 + Math.exp(-Math.max(-3000, Math.min(3000, cp)) / 280.0))); }

    private int sqAt(float x, float y) {
        float fx = (x - boardX) / cell, fy = (y - boardY) / cell;
        if (fx < 0 || fy < 0 || fx >= 8 || fy >= 8) return -1;
        int c = (int) fx, r = (int) fy;
        return flipped() ? Engine.sq(7 - c, r) : Engine.sq(c, 7 - r);
    }
    private float sqX(int sq) { int f = Engine.file(sq); return boardX + (flipped() ? 7 - f : f) * cell; }
    private float sqY(int sq) { int r = Engine.rank(sq); return boardY + (flipped() ? r : 7 - r) * cell; }

    private boolean animActive(long now) { return optAnim && animFrom >= 0 && now - animStart < 220; }

    private void watchGame(long now) {
        if (gm.moveSeq != seenSeq) {
            Pos p = gm.pos();
            if (seenSeq >= 0 && gm.lastUci.length() >= 4) {
                int from = (gm.lastUci.charAt(0) - 'a') + 8 * (gm.lastUci.charAt(1) - '1'), to = (gm.lastUci.charAt(2) - 'a') + 8 * (gm.lastUci.charAt(3) - '1');
                animFrom = from; animTo = to; animPiece = p.b[to]; animCap = prevBoard[to]; animStart = now; animRook = -1;
                if (Engine.type(p.b[to]) == Engine.KING && Math.abs(Engine.file(to) - Engine.file(from)) == 2) {
                    int r = Engine.rank(to); boolean ks = Engine.file(to) == 6;
                    animRook = Engine.sq(ks ? 7 : 0, r); animRookTo = Engine.sq(ks ? 5 : 3, r); animRookPiece = p.b[animRookTo];
                }
                List<String> san = gm.san();
                String s = san.isEmpty() ? "" : san.get(san.size() - 1);
                int kind = gm.over() ? Platform.SND_END : s.indexOf('+') >= 0 || s.indexOf('#') >= 0 ? Platform.SND_CHECK : s.startsWith("O-O") ? Platform.SND_CASTLE : s.indexOf('x') >= 0 ? Platform.SND_CAPTURE : Platform.SND_MOVE;
                if (optSound) pf.sound(kind);
                if (optVibrate) pf.vibrate(kind == Platform.SND_MOVE ? 12 : 25);
            } else animFrom = -1;
            System.arraycopy(p.b, 0, prevBoard, 0, 64);
            seenSeq = gm.moveSeq; viewPly = -1; sel = -1; hintFrom = null;
        }
        if (hintFrom != null && now > hintUntil) hintFrom = null;
        float tg = target();
        evalDisp += (tg - evalDisp) * 0.12f;
        if (pendingNav) { List<Pos> ps = gm.positions(); if (ps != null) { pendingNav = false; navTo(pendingNavTo, ps.size() - 1); } }
        // dialogs triggered by the game
        if (dlg == Dlg.NONE && gm.over() && !gm.spectator() && !animActive(now)) {
            String key = gm.plies() + ":" + gm.result();
            if (!key.equals(overKey)) { overKey = key; dlg = Dlg.OVER; }
        }
        if (!gm.over()) overKey = overKey.length() > 0 && gm.plies() == 0 ? "" : overKey;
        int offer = gm.drawOffer();
        if (offer >= 0 && dlg == Dlg.NONE && !gm.over()) {
            boolean forMe = gm.mode == GameModel.FRIEND || offer != gm.bottomSide();
            String key = offer + ":" + gm.plies();
            if (forMe && !key.equals(offerKey)) {
                offerKey = key; if (optSound) pf.sound(Platform.SND_NOTIFY);
                String who = gm.mode == GameModel.LAN ? "The PS4" : offer == Engine.WHITE ? "White" : "Black";
                confirm(who + " offers a draw", gm.mode == GameModel.FRIEND ? (offer == Engine.WHITE ? "Black" : "White") + ": do you accept?" : "Do you accept?", "Accept", "Decline", new Runnable() { public void run() { gm.answerDraw(true); } }, new Runnable() { public void run() { gm.answerDraw(false); } });
            }
        }
        if (offer < 0 && dlg == Dlg.CONFIRM && dlgTitle.endsWith("offers a draw")) dlg = Dlg.NONE;
    }

    private void drawBar(Gfx g, int color, float y, float x0, float w, float h) {
        g.fillRound(x0, y, w, h, 12 * u, Theme.PANEL);
        float as = h - 14 * u;
        g.fillRound(x0 + 7 * u, y + 7 * u, as, as, 9 * u, Theme.DARK[theme]);
        PieceArt.draw(g, Engine.piece(Engine.KING, color), x0 + 7 * u, y + 7 * u, as, 1f);
        float tx = x0 + as + 18 * u;
        g.text(gm.name(color), tx, y + h * 0.32f, 15 * u, Theme.TXT, true, Gfx.LEFT);
        // captured pieces
        int[] init = {0, 8, 2, 2, 2, 1, 0}, val = {0, 1, 3, 3, 5, 9, 0};
        int[] cw = new int[7], cb = new int[7];
        Pos p = live() ? gm.pos() : viewPos();
        for (int s = 0; s < 64; s++) { int pc = p.b[s]; if (pc != 0) (Engine.color(pc) == Engine.WHITE ? cw : cb)[Engine.type(pc)]++; }
        int diff = 0; float cx = tx;
        for (int t = 5; t >= 1; t--) {
            int miss = Math.max(0, init[t] - (color == Engine.WHITE ? cb[t] : cw[t]));
            diff += (color == Engine.WHITE ? 1 : -1) * 0;
            for (int k = 0; k < miss; k++) { PieceArt.draw(g, Engine.piece(t, color ^ 1), cx, y + h * 0.5f, 20 * u, 1f); cx += (t == 1 ? 8 : 13) * u; }
        }
        int adv = 0;
        for (int t = 1; t <= 5; t++) { adv += (Math.max(0, init[t] - cb[t]) - Math.max(0, init[t] - cw[t])) * val[t]; }
        if (color == Engine.BLACK) adv = -adv;
        if (adv > 0) g.text("+" + adv, cx + 8 * u, y + h * 0.72f, 12 * u, Theme.SUB, true, Gfx.LEFT);
        // clock
        boolean timed = gm.timed();
        long ms = gm.clock(color);
        boolean active = !gm.over() && gm.plies() > 0 && gm.pos().side == color && live();
        boolean low = timed && ms >= 0 && ms < 20000;
        float cw2 = 86 * u;
        g.fillRound(x0 + w - cw2 - 8 * u, y + 8 * u, cw2, h - 16 * u, 9 * u, active ? (low ? Theme.RED : 0xFFFFFFFF) : Theme.DEEP);
        String t = !timed || ms < 0 ? "--:--" : ms < 10000 && ms > 0 ? String.format("0:%02d.%d", ms / 1000, (ms % 1000) / 100) : String.format("%d:%02d", (ms + 999) / 1000 / 60, (ms + 999) / 1000 % 60);
        g.text(t, x0 + w - cw2 / 2 - 8 * u, y + h / 2, 19 * u, active ? (low ? Theme.TXT : 0xFF262522) : (low ? Theme.RED : Theme.TXT), true, Gfx.CENTER);
    }

    private Pos viewPos() {
        if (live()) return gm.pos();
        List<Pos> ps = gm.positions();
        return ps == null || viewPly >= ps.size() ? gm.pos() : ps.get(viewPly);
    }

    private void navTo(int ply, int last) {
        int n = Math.max(0, Math.min(last, ply));
        viewPly = n == last ? -1 : n; sel = -1; animFrom = -1;
    }

    private void drawGame(Gfx g, long now) {
        float pad = 8 * u, evalW = 12 * u, evalGap = 6 * u, barH = 56 * u, topBar = 48 * u, gap = 6 * u;
        float movesH = 38 * u, navH = 40 * u, actH = 58 * u, safe = 10 * u;
        float availH = H - topBar - 2 * barH - movesH - navH - actH - safe - 7 * gap;
        float availW = W - 2 * pad - evalW - evalGap;
        float bs = Math.max(160 * u, Math.min(availW, availH));
        cell = (float) Math.floor(bs / 8f); bs = cell * 8;
        float groupW = evalW + evalGap + bs, gx = (W - groupW) / 2;
        boardX = gx + evalW + evalGap;
        float y = topBar + gap;
        // top bar
        g.fillRect(0, 0, W, topBar, Theme.PANEL);
        chevron(g, 26 * u, topBar / 2, 9 * u, Theme.TXT); hit("game_back", 0, 0, 68 * u, topBar);
        g.text(gm.title(), 48 * u, topBar / 2, 17 * u, Theme.TXT, true, Gfx.LEFT);
        String status = statusText();
        g.text(status, W - 14 * u, topBar / 2, 13 * u, status.startsWith("Your") ? Theme.GREEN : Theme.SUB, true, Gfx.RIGHT);
        int bottom = flipped() ? Engine.BLACK : Engine.WHITE;
        drawBar(g, bottom ^ 1, y, gx, groupW, barH); y += barH + gap;
        boardY = y;
        drawBoardArea(g, now, bs, evalW, gx);
        y += bs + gap;
        drawBar(g, bottom, y, gx, groupW, barH); y += barH + gap;
        drawMoves(g, y, gx, groupW, movesH); y += movesH + gap;
        drawNav(g, y, gx, groupW, navH); y += navH + gap;
        drawActions(g, y, gx, groupW, actH);
        if (gm.mode == GameModel.LAN && (gm.ls == null || !gm.lanConnected || gm.ls.err.length() > 0 && !gm.ls.err.equals("spectator"))) lanOverlay(g, bs);
    }

    private String statusText() {
        if (!live()) return "Reviewing";
        if (gm.over()) return gm.result() == 3 ? "Draw" : "Game over";
        if (gm.mode == GameModel.LAN && !gm.lanConnected) return "Connecting...";
        if (gm.thinking) return "Robot thinking";
        if (gm.spectator()) return "Spectating";
        if (gm.mode == GameModel.FRIEND) return (gm.pos().side == Engine.WHITE ? "White" : "Black") + " to move";
        return gm.myTurn() ? (gm.check() ? "Check! Your move" : "Your move") : (gm.check() ? "Check!" : "Waiting...");
    }

    private void lanOverlay(Gfx g, float bs) {
        g.fillRect(boardX, boardY, bs, bs, 0xCC1A1917);
        String l1 = !gm.lanConnected ? gm.lanStatus.length() > 0 ? gm.lanStatus : "Connecting to your PS4 ..." : gm.ls != null && gm.ls.err.length() > 0 ? gm.ls.err : "Waiting for the PS4 ...";
        g.text(l1, boardX + bs / 2, boardY + bs / 2 - 8 * u, 15 * u, Theme.TXT, true, Gfx.CENTER);
        g.text("Make sure Play on Phone (LAN) is open on the PS4", boardX + bs / 2, boardY + bs / 2 + 16 * u, 11.5f * u, Theme.MUT, false, Gfx.CENTER);
    }

    private void drawBoardArea(Gfx g, long now, float bs, float evalW, float gx) {
        Pos p = viewPos();
        // eval bar
        float ex = gx;
        g.fillRound(ex, boardY, evalW, bs, 5 * u, 0xFF46433F);
        float wh = bs * evalDisp; boolean whiteBottom = !flipped();
        if (gm.hasEval || gm.mode != GameModel.LAN) g.fillRect(ex + 2 * u, whiteBottom ? boardY + bs - wh : boardY, evalW - 4 * u, wh, 0xFFF5F5F5);
        if (gm.hasEval) {
            int cp = gm.evalCp; String s = Math.abs(cp) >= 29000 ? "M" : String.format("%.1f", Math.abs(cp) / 100.0);
            boolean whiteAhead = cp >= 0; boolean atBottom = whiteAhead == whiteBottom;
            g.text(s, ex + evalW / 2, atBottom ? boardY + bs - 9 * u : boardY + 9 * u, 8.5f * u, whiteAhead ? 0xFF262522 : 0xFFEEEEEE, true, Gfx.CENTER);
        }
        // squares
        for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
            int sq = flipped() ? Engine.sq(7 - c, r) : Engine.sq(c, 7 - r);
            boolean light = ((Engine.file(sq) + Engine.rank(sq)) & 1) == 1;
            float x = boardX + c * cell, y = boardY + r * cell;
            g.fillRect(x, y, cell, cell, light ? Theme.LIGHT[theme] : Theme.DARK[theme]);
            if (optCoords) {
                int cc = light ? Theme.DARK[theme] : Theme.LIGHT[theme];
                if (r == 7) g.text(String.valueOf((char) ('a' + Engine.file(sq))), x + cell - 4 * u, y + cell - 7 * u, 9.5f * u, cc, true, Gfx.RIGHT);
                if (c == 0) g.text(String.valueOf(Engine.rank(sq) + 1), x + 4 * u, y + 8 * u, 9.5f * u, cc, true, Gfx.LEFT);
            }
        }
        if (live() && gm.lastUci.length() >= 4) { for (int i = 0; i < 2; i++) { int sq = (gm.lastUci.charAt(2 * i) - 'a') + 8 * (gm.lastUci.charAt(2 * i + 1) - '1'); g.fillRect(sqX(sq), sqY(sq), cell, cell, 0x80FFFF33); } }
        if (sel >= 0 && live()) g.fillRect(sqX(sel), sqY(sel), cell, cell, 0x80146E28);
        if (live() && gm.check()) { int ks = Engine.kingSq(p, p.side); if (ks >= 0) { g.fillCircle(sqX(ks) + cell / 2, sqY(ks) + cell / 2, cell * 0.62f, 0xAAE83A3A); g.fillCircle(sqX(ks) + cell / 2, sqY(ks) + cell / 2, cell * 0.38f, 0xBBE83A3A); } }
        // pieces
        boolean anim = live() && animActive(now);
        for (int sq = 0; sq < 64; sq++) {
            int pc = p.b[sq];
            if (pc == 0) continue;
            if (anim && (sq == animTo || (animRook >= 0 && sq == animRookTo))) continue;
            if (dragging && dragMoved && sq == sel) continue;
            PieceArt.shadow(g, sqX(sq), sqY(sq), cell, 1f);
            PieceArt.draw(g, pc, sqX(sq), sqY(sq), cell, 1f);
        }
        // legal move hints
        if (sel >= 0 && live() && optHints) for (int m : gm.movesFrom(sel)) {
            int to = Engine.to(m); float cx = sqX(to) + cell / 2, cy = sqY(to) + cell / 2;
            if (p.b[to] != 0 || (Engine.flags(m) & Engine.F_EP) != 0) g.strokeCircle(cx, cy, cell * 0.46f, cell * 0.09f, 0x6A145500); else g.fillCircle(cx, cy, cell * 0.16f, 0x66145500);
        }
        // hint arrow
        if (hintFrom != null && live()) arrow(g, hintFrom, hintTo);
        // sliding pieces
        if (anim) {
            float t = Math.min(1f, (now - animStart) / 220f), e = 1f - (1f - t) * (1f - t) * (1f - t);
            if (animCap != 0 && t < 0.8f) PieceArt.draw(g, animCap, sqX(animTo), sqY(animTo), cell, 1f - t / 0.8f);
            slide(g, animPiece, animFrom, animTo, e);
            if (animRook >= 0) slide(g, animRookPiece, animRook, animRookTo, e);
        }
        // dragged piece follows the finger
        if (dragging && dragMoved && sel >= 0) { int pc = p.b[sel]; PieceArt.draw(g, pc, dragX - cell * 0.6f, dragY - cell * 0.95f, cell * 1.2f, 1f); }
    }

    private void slide(Gfx g, int pc, int from, int to, float e) {
        float x = sqX(from) + (sqX(to) - sqX(from)) * e, y = sqY(from) + (sqY(to) - sqY(from)) * e;
        PieceArt.shadow(g, x, y, cell, 1f); PieceArt.draw(g, pc, x, y, cell, 1f);
    }

    private void arrow(Gfx g, String from, String to) {
        int a = (from.charAt(0) - 'a') + 8 * (from.charAt(1) - '1'), b = (to.charAt(0) - 'a') + 8 * (to.charAt(1) - '1');
        float sx = sqX(a) + cell / 2, sy = sqY(a) + cell / 2, ex = sqX(b) + cell / 2, ey = sqY(b) + cell / 2;
        float dx = ex - sx, dy = ey - sy, len = (float) Math.sqrt(dx * dx + dy * dy);
        if (len < 1) return;
        dx /= len; dy /= len;
        float nx = -dy, ny = dx, sh = cell * 0.11f, hd = cell * 0.27f, hl = cell * 0.44f, bx = ex - dx * hl, by = ey - dy * hl;
        g.fillPoly(new float[] {sx + dx * cell * 0.2f + nx * sh, sy + dy * cell * 0.2f + ny * sh, bx + nx * sh, by + ny * sh, bx + nx * hd, by + ny * hd, ex - dx * cell * 0.05f, ey - dy * cell * 0.05f,
            bx - nx * hd, by - ny * hd, bx - nx * sh, by - ny * sh, sx + dx * cell * 0.2f - nx * sh, sy + dy * cell * 0.2f - ny * sh}, 0xCCFFAA00);
    }

    private void drawMoves(Gfx g, float y, float x, float w, float h) {
        g.fillRound(x, y, w, h, 10 * u, Theme.PANEL);
        List<String> san = gm.san();
        g.save(); g.clip(x + 6 * u, y, w - 12 * u, h);
        float total = 8 * u;
        for (int i = 0; i < san.size(); i++) { if (i % 2 == 0) total += g.textWidth((i / 2 + 1) + ".", 12.5f * u, false) + 6 * u; total += g.textWidth(san.get(i), 14 * u, true) + 14 * u; }
        float maxScroll = Math.max(0, total - (w - 12 * u));
        if (live()) movesScroll = maxScroll; else movesScroll = Math.min(movesScroll, maxScroll);
        float cx = x + 10 * u - movesScroll;
        int cur = live() ? san.size() - 1 : viewPly - 1;
        if (san.isEmpty()) g.text("No moves yet", x + 14 * u, y + h / 2, 13 * u, Theme.MUT, false, Gfx.LEFT);
        for (int i = 0; i < san.size(); i++) {
            if (i % 2 == 0) { String n = (i / 2 + 1) + "."; g.text(n, cx, y + h / 2, 12.5f * u, Theme.MUT, false, Gfx.LEFT); cx += g.textWidth(n, 12.5f * u, false) + 6 * u; }
            float tw = g.textWidth(san.get(i), 14 * u, true);
            if (i == cur) g.fillRound(cx - 5 * u, y + 6 * u, tw + 10 * u, h - 12 * u, 6 * u, Theme.GREEN_D);
            g.text(san.get(i), cx, y + h / 2, 14 * u, Theme.TXT, true, Gfx.LEFT);
            cx += tw + 14 * u;
        }
        g.restore();
    }

    private void navBtn(Gfx g, String id, float x, float y, float w, float h, int kind, boolean enabled) {
        g.fillRound(x, y, w, h, 10 * u, Theme.PANEL);
        int c = enabled ? Theme.TXT : 0xFF5A5754; float cx = x + w / 2, cy = y + h / 2, s = 8 * u;
        boolean left = kind == 0 || kind == 1;
        float d = left ? -1 : 1;
        g.fillPoly(new float[] {cx - d * s * 0.5f + (kind == 0 || kind == 3 ? d * s * 0.5f : 0), cy - s, cx + d * s * 0.9f + (kind == 0 || kind == 3 ? d * s * 0.5f : 0), cy, cx - d * s * 0.5f + (kind == 0 || kind == 3 ? d * s * 0.5f : 0), cy + s}, c);
        if (kind == 0 || kind == 3) g.fillRect(cx - d * s * 1.2f - (kind == 0 ? 2 * u : 0) , cy - s, 2.6f * u, 2 * s, c);
        if (enabled) hit(id, x, y, w, h);
    }

    private void drawNav(Gfx g, float y, float x, float w, float h) {
        float bw = (w - 3 * 6 * u) / 4;
        int total = gm.plies(), cur = live() ? total : viewPly;
        navBtn(g, "nav_first", x, y, bw, h, 0, total > 0 && cur > 0);
        navBtn(g, "nav_prev", x + (bw + 6 * u), y, bw, h, 1, total > 0 && cur > 0);
        navBtn(g, "nav_next", x + 2 * (bw + 6 * u), y, bw, h, 2, !live());
        navBtn(g, "nav_last", x + 3 * (bw + 6 * u), y, bw, h, 3, !live());
    }

    private void action(Gfx g, String id, float x, float y, float w, float h, String label, boolean enabled, int color) {
        g.fillRound(x, y, w, h, 12 * u, Theme.PANEL);
        g.text(label, x + w / 2, y + h / 2, 12.5f * u, enabled ? color : 0xFF5A5754, true, Gfx.CENTER);
        if (enabled) hit(id, x, y, w, h);
    }

    private void drawActions(Gfx g, float y, float x, float w, float h) {
        float bw = (w - 4 * 6 * u) / 5;
        boolean play = !gm.over() && !gm.spectator();
        action(g, "act_hint", x, y, bw, h, "Hint", play && live() && !gm.thinking, Theme.TXT);
        action(g, "act_undo", x + (bw + 6 * u), y, bw, h, "Undo", gm.canUndo() && live(), Theme.TXT);
        action(g, "act_draw", x + 2 * (bw + 6 * u), y, bw, h, "1/2 Draw", play && gm.drawOffer() < 0 && gm.plies() > 0, Theme.TXT);
        action(g, "act_resign", x + 3 * (bw + 6 * u), y, bw, h, "Resign", play, 0xFFFF8A80);
        action(g, "act_flip", x + 4 * (bw + 6 * u), y, bw, h, "Flip", true, Theme.TXT);
    }

    // ------------------------------------------------------------------ dialogs
    private void info(String title, String msg, String ok) { dlg = Dlg.INFO; dlgTitle = title; dlgMsg = msg; dlgLabels = new String[] {ok}; dlgActs = new Runnable[] {null}; }
    private void confirm(String title, String msg, String yes, String no, Runnable onYes, Runnable onNo) { dlg = Dlg.CONFIRM; dlgTitle = title; dlgMsg = msg; dlgLabels = new String[] {yes, no}; dlgActs = new Runnable[] {onYes, onNo}; }

    private void drawDialog(Gfx g) {
        g.fillRect(0, 0, W, H, 0x99000000);
        drawingDialog = true;
        float dw = W - 48 * u, dx = 24 * u, dh, dy;
        if (dlg == Dlg.PROMO) {
            dh = 150 * u; dy = (H - dh) / 2;
            g.fillRound(dx, dy, dw, dh, 18 * u, Theme.PANEL);
            g.text("Promote to", W / 2, dy + 28 * u, 17 * u, Theme.TXT, true, Gfx.CENTER);
            float bw = (dw - 5 * 10 * u) / 4;
            for (int i = 0; i < promoMoves.size(); i++) {
                float bx = dx + 10 * u + i * (bw + 10 * u), by = dy + 50 * u;
                g.fillRound(bx, by, bw, bw, 12 * u, Theme.DARK[theme]);
                PieceArt.draw(g, Engine.piece(Engine.promo(promoMoves.get(i)), gm.pos().side), bx + 2 * u, by + 2 * u, bw - 4 * u, 1f);
                hit("promo" + i, bx, by, bw, bw);
            }
        } else if (dlg == Dlg.OVER) {
            dh = 250 * u; dy = (H - dh) / 2;
            g.fillRound(dx, dy, dw, dh, 20 * u, Theme.PANEL);
            int res = gm.result(); boolean draw = res == 3;
            boolean iWon = gm.mode == GameModel.FRIEND ? true : (res == 1) == (gm.bottomSide() == Engine.WHITE);
            String t = draw ? "Draw" : gm.mode == GameModel.FRIEND ? (res == 1 ? "White won" : "Black won") : iWon ? "You won!" : "You lost";
            g.text(t, W / 2, dy + 46 * u, 30 * u, draw ? Theme.TXT : iWon ? Theme.GOLD : Theme.RED, true, Gfx.CENTER);
            g.text((draw ? "Draw by " : (res == 1 ? "White" : "Black") + " won by ") + gm.reason(), W / 2, dy + 84 * u, 13.5f * u, Theme.SUB, false, Gfx.CENTER);
            float bw = (dw - 3 * 12 * u) / 2;
            g.fillRound(dx + 12 * u, dy + 118 * u, bw, 52 * u, 12 * u, Theme.GREEN); g.text("Rematch", dx + 12 * u + bw / 2, dy + 144 * u, 16 * u, Theme.TXT, true, Gfx.CENTER); hit("over_rematch", dx + 12 * u, dy + 118 * u, bw, 52 * u);
            g.fillRound(dx + 24 * u + bw, dy + 118 * u, bw, 52 * u, 12 * u, 0xFF454340); g.text("New game", dx + 24 * u + bw * 1.5f, dy + 144 * u, 16 * u, Theme.TXT, true, Gfx.CENTER); hit("over_new", dx + 24 * u + bw, dy + 118 * u, bw, 52 * u);
            g.text("Review the game", W / 2, dy + 204 * u, 14 * u, Theme.BLUE, true, Gfx.CENTER); hit("over_close", dx, dy + 182 * u, dw, 50 * u);
        } else {
            dh = 190 * u; dy = (H - dh) / 2;
            g.fillRound(dx, dy, dw, dh, 20 * u, Theme.PANEL);
            g.text(dlgTitle, W / 2, dy + 38 * u, 20 * u, Theme.TXT, true, Gfx.CENTER);
            g.text(dlgMsg, W / 2, dy + 76 * u, 13.5f * u, Theme.SUB, false, Gfx.CENTER);
            int n = dlgLabels.length; float bw = (dw - (n + 1) * 12 * u) / n;
            for (int i = 0; i < n; i++) {
                float bx = dx + 12 * u + i * (bw + 12 * u), by = dy + dh - 66 * u;
                g.fillRound(bx, by, bw, 52 * u, 12 * u, i == 0 ? Theme.GREEN : 0xFF454340);
                g.text(dlgLabels[i], bx + bw / 2, by + 26 * u, 16 * u, Theme.TXT, true, Gfx.CENTER);
                hit("dlg" + i, bx, by, bw, 52 * u);
            }
        }
        drawingDialog = false;
    }

    // ================================================================== input
    private Hit hitAt(float x, float y) {
        boolean anyModal = false;
        for (Hit h : hits) if (h.modal) { anyModal = true; break; }
        for (int i = hits.size() - 1; i >= 0; i--) {
            Hit h = hits.get(i);
            if (anyModal && !h.modal) continue;
            if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) return h;
        }
        return null;
    }

    public void touch(int action, float x, float y) {
        if (action == 0) {
            downX = x; downY = y; Hit h = hitAt(x, y); downHit = h == null ? null : h.id;
            if (dlg == Dlg.NONE && screen == Screen.GAME && gm != null && h == null) boardDown(x, y);
        } else if (action == 1) {
            if (dragging) { dragX = x; dragY = y; if (Math.abs(x - downX) + Math.abs(y - downY) > 8 * u) dragMoved = true; pf.invalidate(); }
        } else {
            if (dragging) { boardUp(x, y); dragging = false; dragMoved = false; pf.invalidate(); return; }
            Hit h = hitAt(x, y);
            if (h != null && downHit != null && h.id.equals(downHit)) click(h.id);
            downHit = null;
        }
    }

    private void boardDown(float x, float y) {
        int sq = sqAt(x, y);
        if (sq < 0 || !live() || !gm.myTurn()) { return; }
        Pos p = gm.pos();
        if (sel >= 0 && sel != sq) for (int m : gm.movesFrom(sel)) if (Engine.to(m) == sq) { tryMove(sel, sq); sel = -1; pf.invalidate(); return; }
        int pc = p.b[sq];
        if (pc != 0 && Engine.color(pc) == p.side && !gm.movesFrom(sq).isEmpty()) {
            boolean again = sel == sq; sel = sq; dragging = true; dragMoved = false; dragX = x; dragY = y; downHit = "board" + (again ? "!" : "");
        } else sel = -1;
        pf.invalidate();
    }

    private void boardUp(float x, float y) {
        int to = sqAt(x, y);
        if (dragMoved && sel >= 0 && to >= 0 && to != sel) { int from = sel; if (!tryMove(from, to)) { if (optSound) pf.sound(Platform.SND_ILLEGAL); } sel = -1; }
        else if (!dragMoved && downHit != null && downHit.endsWith("!")) sel = -1;
        downHit = null;
    }

    private boolean tryMove(int from, int to) {
        List<Integer> c = new ArrayList<Integer>();
        for (int m : gm.movesFrom(from)) if (Engine.to(m) == to) c.add(m);
        if (c.isEmpty()) return false;
        if (c.size() > 1 && Engine.promo(c.get(0)) != 0) {
            if (optAutoQueen) { for (int m : c) if (Engine.promo(m) == Engine.QUEEN) return gm.submit(Engine.uci(m)); }
            promoMoves = c; dlg = Dlg.PROMO; return true;
        }
        return gm.submit(Engine.uci(c.get(0)));
    }

    private void click(String id) {
        if (dlg != Dlg.NONE) { dlgClick(id); return; }
        if (id.startsWith("connect:")) { connectTo(id.substring(8)); return; }
        if (id.startsWith("level")) { setupLevel = parse(id.substring(5), 2); return; }
        if (id.startsWith("side")) { setupSide = parse(id.substring(4), 0); return; }
        if (id.startsWith("time")) { setupTime = parse(id.substring(4), 0); return; }
        if (id.startsWith("theme")) { theme = parse(id.substring(5), 0); pf.setPref("theme", String.valueOf(theme)); return; }
        switch (id) {
            case "home_robot": setupMode = GameModel.ROBOT; screen = Screen.SETUP; break;
            case "home_friend": setupMode = GameModel.FRIEND; screen = Screen.SETUP; break;
            case "home_lan": screen = Screen.LAN; foundHosts.clear(); lanMsg = ""; startSearch(); break;
            case "home_settings": screen = Screen.SETTINGS; break;
            case "back_home": stopSearch(); screen = Screen.HOME; break;
            case "start_game": startLocalGame(); break;
            case "lan_search": startSearch(); break;
            case "lan_manual": pf.prompt("PS4 address", "e.g. 192.168.1.20", pf.pref("lanhost", ""), new Platform.TextCallback() { public void onText(String t) { String[] a = Discovery.parseAddress(t); if (a == null) { lanMsg = "That address does not look right"; pf.invalidate(); } else connectTo(a[0] + ":" + a[1]); } }); break;
            case "opt_hints": optHints = !optHints; save("hints", optHints); break;
            case "opt_sound": optSound = !optSound; save("sound", optSound); break;
            case "opt_vib": optVibrate = !optVibrate; save("vib", optVibrate); break;
            case "opt_coords": optCoords = !optCoords; save("coords", optCoords); break;
            case "opt_anim": optAnim = !optAnim; save("anim", optAnim); break;
            case "opt_autoq": optAutoQueen = !optAutoQueen; save("autoq", optAutoQueen); break;
            case "game_back": leaveGame(); break;
            case "act_flip": manualFlip = !manualFlip; break;
            case "act_hint": requestHint(); break;
            case "act_undo": gm.undo(); sel = -1; hintFrom = null; break;
            case "act_draw": gm.offerDraw(); break;
            case "act_resign": confirm("Resign this game?", "Your opponent will win.", "Resign", "Cancel", new Runnable() { public void run() { gm.resign(); } }, null); break;
            case "nav_first": nav(0); break;
            case "nav_prev": nav((live() ? gm.plies() : viewPly) - 1); break;
            case "nav_next": nav((live() ? gm.plies() : viewPly) + 1); break;
            case "nav_last": nav(gm.plies()); break;
            default: break;
        }
        pf.invalidate();
    }

    private void nav(int ply) {
        List<Pos> ps = gm.positions();
        if (ps == null) { pendingNav = true; pendingNavTo = ply; return; }
        navTo(ply, ps.size() - 1);
    }

    private void dlgClick(String id) {
        if (id.startsWith("promo")) { int i = parse(id.substring(5), 0); if (i < promoMoves.size()) { dlg = Dlg.NONE; gm.submit(Engine.uci(promoMoves.get(i))); sel = -1; } return; }
        if (id.startsWith("dlg")) { int i = parse(id.substring(3), 0); Runnable r = i < dlgActs.length ? dlgActs[i] : null; dlg = Dlg.NONE; if (r != null) r.run(); return; }
        switch (id) {
            case "over_rematch": dlg = Dlg.NONE; gm.rematch(setupTime); manualFlip = false; viewPly = -1; break;
            case "over_new": dlg = Dlg.NONE; leaveGameNow(); break;
            case "over_close": dlg = Dlg.NONE; break;
            default: break;
        }
    }

    private void startLocalGame() {
        gm = setupMode == GameModel.ROBOT ? GameModel.robot(pf, setupLevel, setupSide, setupTime) : GameModel.friend(pf, setupTime);
        resetGameView();
        screen = Screen.GAME;
    }

    private void resetGameView() { seenSeq = gm.moveSeq - 1000; manualFlip = false; viewPly = -1; sel = -1; hintFrom = null; overKey = ""; offerKey = ""; animFrom = -1; evalDisp = 0.5f; dlg = Dlg.NONE; java.util.Arrays.fill(prevBoard, (byte) 0); seenSeq = -1; }

    private void leaveGame() {
        if (gm != null && !gm.over() && gm.plies() > 0 && gm.mode != GameModel.LAN && !gm.spectator()) confirm("Leave this game?", "Your game will be lost.", "Leave", "Stay", new Runnable() { public void run() { leaveGameNow(); } }, null);
        else leaveGameNow();
    }

    private void leaveGameNow() {
        if (gm != null && gm.lan != null) gm.lan.stop();
        gm = null; screen = Screen.HOME; dlg = Dlg.NONE;
    }

    private void requestHint() {
        if (gm.thinking || gm.over()) return;
        gm.notice("Thinking ...");
        gm.hint(new GameModel.MoveCallback() { public void onMove(String uci) { gm.noticeUntil = 0; hintFrom = uci.substring(0, 2); hintTo = uci.substring(2, 4); hintUntil = pf.now() + 7000; pf.invalidate(); } });
    }

    // ------------------------------------------------------------------ PS4 connection
    private void stopSearch() { lanSearchId++; searching = false; }

    private void startSearch() {
        final int id = ++lanSearchId;
        searching = true; foundHosts.clear(); lanMsg = "";
        Discovery.search(Discovery.PORT, Discovery.localPrefixes(false), Discovery.broadcastTargets(), new Discovery.Listener() {
            public void onFound(final String ip, final int port) {
                pf.ui(new Runnable() { public void run() { if (id == lanSearchId && !foundHosts.contains(ip + ":" + port)) { foundHosts.add(ip + ":" + port); pf.invalidate(); } } }, 0);
            }
            public void onDone(final int total) {
                pf.ui(new Runnable() { public void run() { if (id == lanSearchId) { searching = false; if (total == 0) lanMsg = "No PS4 found. Open the game and pick Play on Phone (LAN)."; pf.invalidate(); } } }, 0);
            }
        });
        pf.invalidate();
    }

    private void connectTo(String hostPort) {
        String[] a = Discovery.parseAddress(hostPort);
        if (a == null) return;
        stopSearch();
        pf.setPref("lanhost", a[0] + ":" + a[1]);
        final GameModel[] holder = new GameModel[1];
        LanSession s = new LanSession(new LanSession.Listener() {
            public void onState(final LanState st) { pf.ui(new Runnable() { public void run() { if (holder[0] != null) holder[0].onLanState(st); } }, 0); }
            public void onHistory(final List<String> fens) { pf.ui(new Runnable() { public void run() { if (holder[0] != null) holder[0].onLanHistory(fens); } }, 0); }
            public void onStatus(final String text, final boolean ok) { pf.ui(new Runnable() { public void run() { if (holder[0] != null) { holder[0].lanStatus = text; holder[0].lanConnected = ok; pf.invalidate(); } } }, 0); }
        });
        gm = GameModel.lan(pf, s);
        holder[0] = gm;
        resetGameView();
        screen = Screen.GAME;
        s.start(a[0], Integer.parseInt(a[1]));
    }

    /** Back button / gesture. Returns true if the app handled it. */
    public boolean back() {
        if (dlg != Dlg.NONE) { if (dlg == Dlg.OVER) dlg = Dlg.NONE; else dlgClick("dlg" + (dlgLabels.length - 1)); return true; }
        switch (screen) {
            case HOME: return false;
            case GAME: leaveGame(); return true;
            default: stopSearch(); screen = Screen.HOME; return true;
        }
    }

    public void onPause() { if (gm != null && gm.lan != null) { /* keep the connection: the PS4 clocks keep running */ } }

    /** Test hooks: tap the first hit area with this id (after at least one draw). */
    public boolean tapId(String id) {
        for (int i = hits.size() - 1; i >= 0; i--) {
            Hit h = hits.get(i);
            if (h.id.equals(id)) { float cx = h.x + h.w / 2, cy = h.y + h.h / 2; touch(0, cx, cy); touch(2, cx, cy); return true; }
        }
        return false;
    }
    public float[] squareCenter(int sq) { return new float[] {sqX(sq) + cell / 2, sqY(sq) + cell / 2}; }
    public void debugConnect(String hostPort) { connectTo(hostPort); }
    public void debugAddHost(String h) { foundHosts.add(h); searching = false; }
    public void debugGoLan() { screen = Screen.LAN; }
    public String dialogName() { return dlg.name(); }

    /** Test hooks. */
    public String screenName() { return screen.name(); }
    public GameModel model() { return gm; }
    public void debugOpenLocal(int mode, int level, int side, int time) { setupMode = mode; setupLevel = level; setupSide = side; setupTime = time; startLocalGame(); }
}
