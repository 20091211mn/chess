package com.ps4chess.app.ui;

/** chess.com-like dark palette and board colour themes. */
public final class Theme {
    private Theme() {}
    public static final int BG = 0xFF262522, PANEL = 0xFF2B2926, CARD = 0xFF312E2B, DEEP = 0xFF21201D, LINE = 0xFF403D39;
    public static final int TXT = 0xFFFFFFFF, SUB = 0xFFBABABA, MUT = 0xFF8B8987, GREEN = 0xFF81B64C, GREEN_D = 0xFF5D8A3A, RED = 0xFFE04E46, GOLD = 0xFFFFCD00, BLUE = 0xFF78C8FF;
    public static final String[] NAMES = {"Green", "Brown", "Blue", "Purple"};
    public static final int[] LIGHT = {0xFFEBECD0, 0xFFF0D9B5, 0xFFDEE3E6, 0xFFF0F1F0};
    public static final int[] DARK = {0xFF779556, 0xFFB58863, 0xFF8CA2AD, 0xFF8476BA};

    public static int alpha(int argb, int a) { return (argb & 0x00FFFFFF) | (Math.max(0, Math.min(255, a)) << 24); }
    public static int mulAlpha(int argb, float f) { return alpha(argb, (int) (((argb >>> 24) & 255) * f)); }
}
