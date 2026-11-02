// Renders every screen of the Android app with Java2D and drives it with simulated touches.
// Usage: java -cp <classes> AppTests <chessweb binary> <out dir> [font dir]
import com.ps4chess.app.core.*;
import com.ps4chess.app.ui.*;
import javax.imageio.ImageIO;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.ConcurrentLinkedQueue;

public class AppTests {
    static int fails = 0;
    static void check(boolean c, String m) { if (!c) { System.out.println("FAIL: " + m); fails++; } }

    static final class FakePf implements Platform {
        long now = 1_000_000; final Map<String, String> prefs = new HashMap<String, String>(); final List<Integer> sounds = new ArrayList<Integer>();
        final ConcurrentLinkedQueue<Object[]> queue = new ConcurrentLinkedQueue<Object[]>();
        public void async(Runnable w, Runnable d) { w.run(); d.run(); }
        public void ui(Runnable r, long delay) { queue.add(new Object[] {r, now + delay}); }
        public long now() { return now; }
        public String pref(String k, String d) { String v = prefs.get(k); return v == null ? d : v; }
        public void setPref(String k, String v) { prefs.put(k, v); }
        public void sound(int k) { sounds.add(k); } public void vibrate(int ms) { }
        public void prompt(String t, String h, String i, TextCallback cb) { cb.onText("192.168.1.20"); }
        public void invalidate() { } public void toast(String s) { } public void exit() { }
        void pump() { for (Object[] o : new ArrayList<Object[]>(queue)) if ((Long) o[1] <= now) { queue.remove(o); ((Runnable) o[0]).run(); } }
        void advance(long ms) { now += ms; pump(); }
    }

    static FakePf pf; static ChessApp app; static String fonts; static Path outDir;

    static Java2DGfx frame() throws Exception { Java2DGfx g = new Java2DGfx(780, 1688, fonts); app.draw(g, pf.now); return g; }
    static void shot(String name) throws Exception { ImageIO.write(frame().img, "png", outDir.resolve(name + ".png").toFile()); }
    static float[] c(String s) { return app.squareCenter((s.charAt(0) - 'a') + 8 * (s.charAt(1) - '1')); }
    static void tap(String sq) { float[] p = c(sq); app.touch(0, p[0], p[1]); app.touch(2, p[0], p[1]); }
    static void drag(String a, String b) throws Exception {
        float[] p = c(a), q = c(b); app.touch(0, p[0], p[1]); frame(); app.touch(1, (p[0] + q[0]) / 2, (p[1] + q[1]) / 2); frame(); app.touch(1, q[0], q[1]); frame(); app.touch(2, q[0], q[1]);
    }
    static boolean tapId(String id) throws Exception { frame(); return app.tapId(id); }
    static void settle() throws Exception { for (int i = 0; i < 8; i++) { pf.advance(150); frame(); } }
    static boolean waitFor(java.util.function.BooleanSupplier cnd, int ms) throws Exception { long end = System.currentTimeMillis() + ms; while (System.currentTimeMillis() < end) { pf.now += 40; pf.pump(); frame(); if (cnd.getAsBoolean()) return true; Thread.sleep(40); } return cnd.getAsBoolean(); }

    public static void main(String[] a) throws Exception {
        System.setProperty("java.awt.headless", "true");
        outDir = Paths.get(a[1]); Files.createDirectories(outDir);
        fonts = a.length > 2 ? a[2] : "/usr/share/fonts/truetype/google-fonts";
        pf = new FakePf(); app = new ChessApp(pf); app.resize(780, 1688);

        shot("01_home"); check(app.screenName().equals("HOME"), "starts on home");
        check(tapId("home_robot") && app.screenName().equals("SETUP"), "Play Computer opens setup");
        tapId("level3"); tapId("time3"); shot("02_setup_robot");
        check(tapId("start_game") && app.screenName().equals("GAME") && app.model().mode == GameModel.ROBOT, "starts a robot game");
        shot("03_game_start");

        // tap-tap move, robot answers
        tap("e2"); frame(); shot("04_selected"); tap("e4"); settle();
        check(app.model().plies() >= 1 && app.model().san().get(0).equals("e4"), "e2-e4 played by tapping (plies=" + app.model().plies() + ")");
        pf.advance(900); settle();
        check(app.model().plies() == 2, "robot answered (plies=" + app.model().plies() + ")");
        // drag and drop
        drag("d2", "d4"); settle(); pf.advance(900); settle();
        check(app.model().plies() == 4, "drag & drop d2-d4 + robot answer (plies=" + app.model().plies() + ")");
        int n0 = app.model().plies(); drag("a2", "a5"); settle(); check(app.model().plies() == n0, "illegal drag ignored");
        check(!pf.sounds.isEmpty(), "move sounds played");
        // review
        check(tapId("nav_prev"), "review button"); shot("05_review"); tapId("nav_last");
        // hint
        check(tapId("act_hint"), "hint button"); settle(); shot("06_hint");
        // promotion
        app.model().game.loadFen("8/P6k/8/8/8/8/8/K7 w - - 0 1"); frame();
        tap("a7"); frame(); tap("a8"); frame(); check(app.dialogName().equals("PROMO"), "promotion dialog opens"); shot("07_promo");
        check(tapId("promo1"), "choose rook"); settle();
        check(app.model().game.pos.b[56] == Engine.piece(Engine.ROOK, Engine.WHITE) || app.model().game.plies() >= 1, "promoted piece placed");
        // resign
        check(tapId("act_resign") && app.dialogName().equals("CONFIRM"), "resign asks to confirm"); shot("08_confirm");
        tapId("dlg0"); settle(); pf.advance(500); settle();
        check(app.model().over() && app.dialogName().equals("OVER"), "game over dialog (" + app.dialogName() + ")"); shot("09_over");
        check(tapId("over_rematch"), "rematch"); pf.advance(900); settle(); check(app.model().plies() >= 0, "rematch started");
        // leave & settings
        tapId("game_back"); frame(); if (app.dialogName().equals("CONFIRM")) tapId("dlg0"); frame();
        check(app.screenName().equals("HOME"), "back to home (" + app.screenName() + ")");
        check(tapId("home_settings") && app.screenName().equals("SETTINGS"), "settings"); tapId("theme1"); tapId("opt_sound"); shot("10_settings");
        check(pf.pref("theme", "0").equals("1") && pf.pref("sound", "1").equals("0"), "settings are stored");
        ChessApp again = new ChessApp(pf); check(again != null, "app restarts with stored settings");
        // friend game
        app.back(); frame(); tapId("home_friend"); frame(); shot("11_setup_friend"); tapId("start_game"); frame();
        for (String m : new String[] {"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6", "h5f7"}) check(app.model().submit(m), "friend move " + m);
        settle(); pf.advance(500); settle(); check(app.dialogName().equals("OVER"), "friend game over"); shot("12_friend_over");
        tapId("over_close"); frame(); shot("13_friend_review");
        app.back(); frame(); if (app.dialogName().equals("CONFIRM")) tapId("dlg0"); app.back(); frame();
        // PS4 screens
        app.back(); frame(); tapId("home_lan"); frame(); app.debugAddHost("192.168.1.20:8080"); shot("14_lan_found");

        if (a.length > 0 && !a[0].equals("-")) lan(a[0]);
        System.out.println(fails == 0 ? "ALL APP UI TESTS PASSED" : fails + " APP UI FAILURES");
        System.exit(fails == 0 ? 0 : 1);
    }

    static void lan(String exe) throws Exception {
        Process srv = new ProcessBuilder(exe, "18084", "1").redirectErrorStream(true).start();
        try {
            Thread.sleep(800);
            app.debugGoLan(); frame(); app.debugConnect("127.0.0.1:18084");
            check(waitFor(() -> app.model() != null && app.model().ls != null, 5000), "LAN: first state arrived");
            GameModel m = app.model(); shot("15_lan_game");
            check(m.mode == GameModel.LAN && m.bottomSide() == Engine.WHITE && m.myTurn(), "LAN: we play white and it is our move");
            tap("e2"); frame(); tap("e4");
            check(waitFor(() -> m.plies() >= 2, 8000), "LAN: move sent and the PS4's robot answered (plies=" + m.plies() + ")");
            shot("16_lan_after");
            check(tapId("nav_prev"), "LAN: review"); waitFor(() -> false, 600); shot("17_lan_review"); tapId("nav_last");
            check(tapId("act_draw"), "LAN: draw button");
            // the simulated PS4 robot judges the offer at once: equal position -> it accepts
            check(waitFor(() -> m.over(), 4000) && m.result() == 3 && m.reason().equals("agreement"), "LAN: the PS4 answered the draw offer (over=" + m.over() + ")");
            frame(); check(app.dialogName().equals("OVER"), "LAN: game over card appears"); shot("18_lan_over");
            app.back(); frame(); app.back(); frame(); check(app.screenName().equals("HOME"), "LAN: leaving returns home (" + app.screenName() + ")");
        } finally { srv.destroy(); }
    }
}
