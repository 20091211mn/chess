package com.ps4chess.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.os.Vibrator;
import android.text.InputType;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.Toast;

import com.ps4chess.app.ui.ChessApp;
import com.ps4chess.app.ui.Platform;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** PS4 Chess for Android: a native chess app (own engine, own UI) that can also play against the console over Wi-Fi. */
public class MainActivity extends Activity implements Platform {
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final ExecutorService workers = Executors.newFixedThreadPool(2);
    private ChessView view;
    private SharedPreferences prefs;
    private SoundBank sounds;
    private Vibrator vibrator;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setStatusBarColor(0xFF262522);
        getWindow().setNavigationBarColor(0xFF262522);
        prefs = getSharedPreferences("ps4chess", MODE_PRIVATE);
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        sounds = new SoundBank();
        Typeface regular = Typeface.DEFAULT, bold = Typeface.DEFAULT_BOLD;
        try {
            regular = Typeface.createFromAsset(getAssets(), "fonts/Poppins-Medium.ttf");
            bold = Typeface.createFromAsset(getAssets(), "fonts/Poppins-Bold.ttf");
        } catch (Throwable ignored) { }
        ChessApp app = new ChessApp(this);
        app.resize(1080, 1920);
        view = new ChessView(this, app, new AndroidGfx(regular, bold));
        this.app = app;
        setContentView(view);
    }

    private ChessApp app;

    @Override
    public void onBackPressed() {
        if (app == null || !app.back()) super.onBackPressed();
        else view.invalidate();
    }

    @Override
    protected void onPause() { super.onPause(); if (app != null) app.onPause(); }

    @Override
    protected void onDestroy() { super.onDestroy(); workers.shutdownNow(); }

    // ------------------------------------------------------------------ Platform
    public void async(final Runnable work, final Runnable done) {
        workers.execute(new Runnable() {
            public void run() {
                try { work.run(); } catch (Throwable t) { /* a failed AI job must not crash the app */ }
                ui.post(done);
            }
        });
    }

    public void ui(Runnable r, long delayMs) { if (delayMs <= 0) ui.post(r); else ui.postDelayed(r, delayMs); }
    public long now() { return SystemClock.uptimeMillis(); }
    public String pref(String key, String def) { return prefs.getString(key, def); }
    public void setPref(String key, String value) { prefs.edit().putString(key, value).apply(); }
    public void sound(int kind) { if (sounds != null) sounds.play(kind); }

    @SuppressWarnings("deprecation")
    public void vibrate(int ms) { try { if (vibrator != null) vibrator.vibrate(ms); } catch (Throwable ignored) { } }

    public void prompt(String title, String hint, String initial, final TextCallback cb) {
        final EditText input = new EditText(this);
        input.setHint(hint);
        input.setText(initial);
        input.setSingleLine(true);
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        new AlertDialog.Builder(this).setTitle(title).setView(input)
            .setPositiveButton("Connect", new DialogInterface.OnClickListener() { public void onClick(DialogInterface d, int w) { cb.onText(input.getText().toString()); } })
            .setNegativeButton("Cancel", null).show();
    }

    public void invalidate() { if (view != null) view.postInvalidateOnAnimation(); }
    public void toast(String s) { Toast.makeText(this, s, Toast.LENGTH_SHORT).show(); }
    public void exit() { finish(); }
}
