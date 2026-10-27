package com.ps4chess.app.ui;

/** Everything the UI needs from the host (Android activity or a test). */
public interface Platform {
    int SND_MOVE = 0, SND_CAPTURE = 1, SND_CHECK = 2, SND_CASTLE = 3, SND_END = 4, SND_ILLEGAL = 5, SND_NOTIFY = 6;

    interface TextCallback { void onText(String text); }

    /** Runs work on a background thread, then done on the UI thread. */
    void async(Runnable work, Runnable done);
    /** Runs r on the UI thread after delayMs. */
    void ui(Runnable r, long delayMs);
    long now();
    String pref(String key, String def);
    void setPref(String key, String value);
    void sound(int kind);
    void vibrate(int ms);
    void prompt(String title, String hint, String initial, TextCallback cb);
    void invalidate();
    void toast(String s);
    void exit();
}
