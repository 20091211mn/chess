// Compiles the platform-independent app sources plus the Java2D test code. Usage: java Build.java <android dir> <out classes dir>
import javax.tools.*;
import java.io.File;
import java.nio.file.*;
import java.util.*;
import java.util.stream.Stream;

public class Build {
    public static void main(String[] a) throws Exception {
        Path root = Paths.get(a[0]); Path out = Paths.get(a[1]); Files.createDirectories(out);
        List<File> srcs = new ArrayList<File>();
        for (String d : new String[] {"app/src/main/java/com/ps4chess/app/core", "app/src/main/java/com/ps4chess/app/ui"}) try (Stream<Path> s = Files.walk(root.resolve(d))) { s.filter(p -> p.toString().endsWith(".java")).forEach(p -> srcs.add(p.toFile())); }
        srcs.add(root.resolve("app/src/main/java/com/ps4chess/app/Discovery.java").toFile());
        srcs.add(root.resolve("tests/Java2DGfx.java").toFile());
        srcs.add(root.resolve("tests/AppTests.java").toFile());
        JavaCompiler jc = ToolProvider.getSystemJavaCompiler(); DiagnosticCollector<JavaFileObject> diag = new DiagnosticCollector<>(); StandardJavaFileManager fm = jc.getStandardFileManager(diag, null, null);
        boolean ok = jc.getTask(null, fm, diag, Arrays.asList("-Xlint:-options", "-d", out.toString()), null, fm.getJavaFileObjectsFromFiles(srcs)).call();
        for (Diagnostic<?> d : diag.getDiagnostics()) if (d.getKind() == Diagnostic.Kind.ERROR) System.out.println(d);
        System.out.println(ok ? "UI sources compile" : "UI COMPILE ERRORS");
        System.exit(ok ? 0 : 1);
    }
}
