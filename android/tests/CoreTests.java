// Pure-Java tests of the Android app core: engine (perft), AI, JSON, WebSocket client and LAN session vs the real C++ server.
// Usage: java CoreTests.java <chessweb binary> <android dir>   (compiles the app sources itself)
import javax.tools.*;
import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.stream.*;

public class CoreTests {
    static int fails = 0;
    static void check(boolean c, String m) { if (!c) { System.out.println("FAIL: " + m); fails++; } }

    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args.length > 1 ? args[1] : "android");
        Path out = Files.createTempDirectory("ps4chess_core");
        List<File> srcs = new ArrayList<File>();
        for (String d : new String[] {"app/src/main/java/com/ps4chess/app/core"}) try (Stream<Path> s = Files.walk(root.resolve(d))) { s.filter(p -> p.toString().endsWith(".java")).forEach(p -> srcs.add(p.toFile())); }
        srcs.add(root.resolve("app/src/main/java/com/ps4chess/app/Discovery.java").toFile());
        JavaCompiler jc = ToolProvider.getSystemJavaCompiler();
        DiagnosticCollector<JavaFileObject> diag = new DiagnosticCollector<>();
        StandardJavaFileManager fm = jc.getStandardFileManager(diag, null, null);
        boolean ok = jc.getTask(null, fm, diag, Arrays.asList("-source", "8", "-target", "8", "-Xlint:-options", "-d", out.toString()), null, fm.getJavaFileObjectsFromFiles(srcs)).call();
        for (Diagnostic<?> d : diag.getDiagnostics()) if (d.getKind() == Diagnostic.Kind.ERROR) System.out.println("  " + d);
        check(ok, "core sources compile"); if (!ok) System.exit(1);
        URLClassLoader cl = new URLClassLoader(new URL[] {out.toUri().toURL()});
        Class<?> E = cl.loadClass("com.ps4chess.app.core.Engine");
        java.lang.reflect.Method fromFen = E.getMethod("fromFen", String.class), perft = E.getMethod("perft", cl.loadClass("com.ps4chess.app.core.Pos"), int.class);
        Object[][] suites = {
            {"start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20L}, {"start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281L},
            {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862L},
            {"pos3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238L},
            {"pos4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467L},
            {"pos5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379L},
            {"pos6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890L}};
        for (Object[] s : suites) { long got = (Long) perft.invoke(null, fromFen.invoke(null, s[1]), s[2]); check(got == (Long) s[3], "perft " + s[0] + " d" + s[2] + " = " + got + " expected " + s[3]); }
        System.out.println("  perft suites checked");

        Class<?> G = cl.loadClass("com.ps4chess.app.core.Game"); Object g = G.getConstructor().newInstance();
        java.lang.reflect.Method playUci = G.getMethod("playUci", String.class);
        for (String m : new String[] {"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6", "h5f7"}) check((Boolean) playUci.invoke(g, m), "scholar's mate move " + m);
        check(G.getField("result").getInt(g) == 1 && G.getField("reason").get(g).equals("checkmate"), "scholar's mate detected");
        check(((List<?>) G.getField("san").get(g)).get(6).equals("Qxf7#"), "SAN Qxf7#");
        Object g2 = G.getConstructor().newInstance();
        for (String m : new String[] {"g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6", "f3g1", "f6g8"}) playUci.invoke(g2, m);
        check(G.getField("reason").get(g2).equals("threefold repetition"), "threefold repetition");
        Object g3 = G.getConstructor().newInstance(); G.getMethod("loadFen", String.class).invoke(g3, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        check((Boolean) playUci.invoke(g3, "e1g1") && ((List<?>) G.getField("san").get(g3)).get(0).equals("O-O"), "castling");
        Object g4 = G.getConstructor().newInstance(); G.getMethod("loadFen", String.class).invoke(g4, "8/P6k/8/8/8/8/8/K7 w - - 0 1");
        check(!(Boolean) playUci.invoke(g4, "a7a8") && (Boolean) playUci.invoke(g4, "a7a8n") && ((List<?>) G.getField("san").get(g4)).get(0).equals("a8=N"), "promotion needs a piece");

        Class<?> A = cl.loadClass("com.ps4chess.app.core.Ai"); Object ai = A.getConstructor().newInstance();
        Object[] levels = (Object[]) A.getField("LEVELS").get(null); java.lang.reflect.Method think = A.getMethod("think", cl.loadClass("com.ps4chess.app.core.Pos"), levels[0].getClass());
        java.lang.reflect.Method uci = E.getMethod("uci", int.class);
        String[][] puzzles = {{"6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1", "a1a8"}, {"r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", "h5f7"}, {"rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2", "d8h4"}, {"4k3/8/8/3q4/8/8/8/3RK3 w - - 0 1", "d1d5"}};
        for (String[] pz : puzzles) { int m = (Integer) think.invoke(ai, fromFen.invoke(null, pz[0]), levels[3]); check(uci.invoke(null, m).equals(pz[1]), "AI finds " + pz[1] + " got " + uci.invoke(null, m)); }
        long t0 = System.currentTimeMillis(); think.invoke(ai, fromFen.invoke(null, "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"), levels[4]);
        System.out.println("  expert move took " + (System.currentTimeMillis() - t0) + " ms (JVM)");
        // self-play legality
        Random rnd = new Random(3); int plies = 0;
        for (int gi = 0; gi < 3; gi++) { Object gg = G.getConstructor().newInstance();
            while (!(Boolean) G.getMethod("over").invoke(gg) && (Integer) G.getMethod("plies").invoke(gg) < 160) {
                Object pos = G.getField("pos").get(gg); int m = (Integer) think.invoke(ai, pos, levels[gi % 3]);
                check(m >= 0 && (Boolean) G.getMethod("play", int.class).invoke(gg, m), "AI move legal"); plies++; } }
        System.out.println("  AI self-play: " + plies + " plies, all legal");

        Class<?> J = cl.loadClass("com.ps4chess.app.core.MiniJson");
        Object o = J.getMethod("parse", String.class).invoke(null, "{\"a\":[1,2.5,\"x\\\"y\"],\"b\":{\"c\":true,\"d\":null},\"e\":-3}");
        check(o instanceof Map && ((Map<?, ?>) o).get("e").equals(-3.0) && ((List<?>) ((Map<?, ?>) o).get("a")).get(2).equals("x\"y"), "json parse");
        check(J.getMethod("parse", String.class).invoke(null, "{bad") == null, "json rejects junk");

        Class<?> W = cl.loadClass("com.ps4chess.app.core.WsClient");
        check(W.getMethod("accept", String.class).invoke(null, "dGhlIHNhbXBsZSBub25jZQ==").equals("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), "websocket accept key");

        if (args.length > 0) lanTests(cl, args[0]);
        System.out.println(fails == 0 ? "ALL CORE TESTS PASSED" : fails + " CORE FAILURES");
        System.exit(fails == 0 ? 0 : 1);
    }

    static void lanTests(URLClassLoader cl, String exe) throws Exception {
        Process srv = new ProcessBuilder(exe, "18083", "1").redirectErrorStream(true).start();
        try {
            Thread.sleep(800);
            Class<?> S = cl.loadClass("com.ps4chess.app.core.LanSession"), L = cl.loadClass("com.ps4chess.app.core.LanSession$Listener"), ST = cl.loadClass("com.ps4chess.app.core.LanState");
            final List<Object> states = Collections.synchronizedList(new ArrayList<Object>()); final List<Object> hists = Collections.synchronizedList(new ArrayList<Object>());
            final List<String> statuses = Collections.synchronizedList(new ArrayList<String>());
            Object lis = java.lang.reflect.Proxy.newProxyInstance(cl, new Class<?>[] {L}, (p, m, a) -> { if (m.getName().equals("onState")) states.add(a[0]); else if (m.getName().equals("onHistory")) hists.add(a[0]); else if (m.getName().equals("onStatus")) statuses.add((String) a[0]); return null; });
            Object sess = S.getConstructor(L).newInstance(lis);
            S.getMethod("start", String.class, int.class).invoke(sess, "127.0.0.1", 18083);
            java.util.function.BooleanSupplier hasState = () -> !states.isEmpty();
            wait(hasState, 4000); check(!states.isEmpty(), "LAN: first state received");
            Object st = states.get(states.size() - 1);
            check(ST.getField("you").get(st).equals("w") && ((List<?>) ST.getField("legal").get(st)).contains("e2e4"), "LAN: state says we are white with e2e4 legal");
            check(ST.getField("fen").get(st).toString().startsWith("rnbqkbnr/pppppppp"), "LAN: start FEN");
            states.clear();
            S.getMethod("send", String.class).invoke(sess, "move e2e4");
            wait(() -> states.stream().anyMatch(x -> { try { return ((Integer) ST.getField("plies").get(x)) >= 2; } catch (Exception e) { return false; } }), 6000);
            Object after = states.get(states.size() - 1);
            check(((Integer) ST.getField("plies").get(after)) >= 2, "LAN: PS4 robot answered our move (plies=" + ST.getField("plies").get(after) + ")");
            S.getMethod("send", String.class).invoke(sess, "hist"); wait(() -> !hists.isEmpty(), 3000);
            check(!hists.isEmpty() && ((List<?>) hists.get(0)).size() >= 3, "LAN: history FENs received");
            states.clear(); S.getMethod("send", String.class).invoke(sess, "move e7e3"); wait(hasState, 3000);
            check(!states.isEmpty() && ST.getField("err").get(states.get(0)).toString().contains("illegal"), "LAN: illegal move reported");
            S.getMethod("stop").invoke(sess);
            // reconnect: server restarts, session comes back by itself
            Object sess2 = S.getConstructor(L).newInstance(lis); states.clear(); statuses.clear();
            S.getMethod("start", String.class, int.class).invoke(sess2, "127.0.0.1", 18083); wait(hasState, 4000);
            srv.destroy(); srv.waitFor(); Thread.sleep(300); states.clear();
            srv = new ProcessBuilder(exe, "18083", "1").redirectErrorStream(true).start();
            wait(hasState, 12000); check(!states.isEmpty(), "LAN: automatic reconnect after the PS4 restarted (statuses " + statuses + ")");
            S.getMethod("stop").invoke(sess2);
        } finally { srv.destroy(); }
    }

    static void wait(java.util.function.BooleanSupplier c, int ms) throws Exception { long end = System.currentTimeMillis() + ms; while (System.currentTimeMillis() < end && !c.getAsBoolean()) Thread.sleep(25); }
}
