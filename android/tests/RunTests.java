// Compiles the Android sources against tiny stubs (type-check only) and tests Discovery against the real C++ server.
// Usage: java android/tests/RunTests.java <path-to-chessweb-binary>
import javax.tools.*;
import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.stream.*;

public class RunTests {
    static int fails = 0;
    static void check(boolean c, String m) { if (!c) { System.out.println("FAIL: " + m); fails++; } }

    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args.length > 1 ? args[1] : "android");
        Path out = Files.createTempDirectory("ps4chess_android");
        // 1. everything compiles as Java 8 (Android desugars from there), against the stubs
        List<File> srcs = new ArrayList<File>();
        for (String d : new String[] {"app/src/main/java", "tests/stubs"})
            try (Stream<Path> s = Files.walk(root.resolve(d))) { s.filter(p -> p.toString().endsWith(".java")).forEach(p -> srcs.add(p.toFile())); }
        JavaCompiler jc = ToolProvider.getSystemJavaCompiler();
        DiagnosticCollector<JavaFileObject> diag = new DiagnosticCollector<>();
        StandardJavaFileManager fm = jc.getStandardFileManager(diag, null, null);
        boolean ok = jc.getTask(null, fm, diag, Arrays.asList("-source", "8", "-target", "8", "-d", out.toString(), "-Xlint:-options"), null, fm.getJavaFileObjectsFromFiles(srcs)).call();
        for (Diagnostic<?> d : diag.getDiagnostics()) if (d.getKind() == Diagnostic.Kind.ERROR) System.out.println("  " + d);
        check(ok, "Android sources compile (" + srcs.size() + " files)");
        // Android desugars Java 8 only: refuse newer library calls that would crash on old phones
        for (File f : srcs) { if (f.getPath().contains("stubs")) continue;
            String code = new String(Files.readAllBytes(f.toPath()), "UTF-8");
            for (String bad : new String[] {"List.of(", "Map.of(", "Set.of(", " var ", "String.isBlank", ".strip()", "Optional.isEmpty"})
                check(!code.contains(bad), f.getName() + " must not use " + bad); }
        if (!ok) { System.exit(1); }
        URLClassLoader cl = new URLClassLoader(new URL[] {out.toUri().toURL()});
        Class<?> disc = cl.loadClass("com.ps4chess.app.Discovery");

        // 2. pure logic
        java.lang.reflect.Method parseAddr = disc.getMethod("parseAddress", String.class), parseRep = disc.getMethod("parseReplyPort", String.class);
        check(Arrays.equals((String[]) parseAddr.invoke(null, "192.168.1.20"), new String[] {"192.168.1.20", "8080"}), "address without port");
        check(Arrays.equals((String[]) parseAddr.invoke(null, "http://10.0.0.5:9000/"), new String[] {"10.0.0.5", "9000"}), "address with scheme and port");
        check(parseAddr.invoke(null, "bad address!") == null && parseAddr.invoke(null, "1.2.3.4:99999") == null && parseAddr.invoke(null, "") == null, "junk addresses rejected");
        check((Integer) parseRep.invoke(null, "PS4CHESS|8080|PS4 Chess") == 8080 && (Integer) parseRep.invoke(null, "hello") == -1 && (Integer) parseRep.invoke(null, "PS4CHESS|x") == -1, "reply parsing");

        // 3. discovery against the real server (HTTP 18080, UDP 18081)
        if (args.length > 0) {
            Process srv = new ProcessBuilder(args[0], "18080", "1").redirectErrorStream(true).start();
            try {
                Thread.sleep(800);
                java.lang.reflect.Method ping = disc.getMethod("ping", String.class, int.class, int.class);
                check((Boolean) ping.invoke(null, "127.0.0.1", 18080, 800), "ping finds the server");
                check(!(Boolean) ping.invoke(null, "127.0.0.1", 18999, 400), "ping fails on a closed port");
                Class<?> lst = cl.loadClass("com.ps4chess.app.Discovery$Listener");
                final Set<String> found = ConcurrentHashMap.newKeySet(); final CountDownLatch done = new CountDownLatch(1); final int[] total = {-1};
                Object listener = java.lang.reflect.Proxy.newProxyInstance(cl, new Class<?>[] {lst}, (p, m, a) -> {
                    if (m.getName().equals("onFound")) found.add(a[0] + ":" + a[1]); else if (m.getName().equals("onDone")) { total[0] = (Integer) a[0]; done.countDown(); }
                    return null; });
                long t0 = System.currentTimeMillis();
                disc.getMethod("search", int.class, List.class, List.class, lst).invoke(null, 18080, Arrays.asList("127.0.0."), Arrays.asList("127.0.0.1"), listener);
                check(done.await(40, TimeUnit.SECONDS), "search finishes");
                System.out.println("  search took " + (System.currentTimeMillis() - t0) + " ms, found " + found.size() + " hosts incl. loopback aliases");
                check(found.contains("127.0.0.1:18080"), "search finds the server at 127.0.0.1:18080 (found=" + found + ")");
                check(total[0] == found.size(), "each host reported once (total=" + total[0] + " found=" + found.size() + ")");
                // nothing listening -> clean 'not found'
                final CountDownLatch done2 = new CountDownLatch(1); final int[] total2 = {-1};
                Object l2 = java.lang.reflect.Proxy.newProxyInstance(cl, new Class<?>[] {lst}, (p, m, a) -> { if (m.getName().equals("onDone")) { total2[0] = (Integer) a[0]; done2.countDown(); } return null; });
                disc.getMethod("search", int.class, List.class, List.class, lst).invoke(null, 18999, Arrays.asList("127.0.0."), Arrays.asList("127.0.0.1"), l2);
                check(done2.await(40, TimeUnit.SECONDS) && total2[0] == 0, "nothing found when no server runs (total=" + total2[0] + ")");
            } finally { srv.destroy(); }
        }
        System.out.println(fails == 0 ? "ALL ANDROID TESTS PASSED" : fails + " ANDROID FAILURES");
        System.exit(fails == 0 ? 0 : 1);
    }
}
