package android.graphics; public class Color { public static final int WHITE = -1; public static int parseColor(String s) { return Integer.parseInt(s.substring(1), 16) | 0xFF000000; } }
