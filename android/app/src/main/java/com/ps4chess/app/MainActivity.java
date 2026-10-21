package com.ps4chess.app;

import android.app.Activity;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/** Finds the PS4 chess server on the Wi-Fi by itself and shows its page full screen. */
public class MainActivity extends Activity {
    private static final String PREFS = "ps4chess";
    private static final String KEY_HOST = "host";
    private static final int BG = Color.parseColor("#262522");
    private static final int GREEN = Color.parseColor("#81b64c");
    private static final int GREY = Color.parseColor("#454340");

    private final Handler ui = new Handler(Looper.getMainLooper());
    private WebView web;
    private FrameLayout webRoot;
    private TextView loading;
    private Runnable loadTimeout;
    private LinearLayout connectRoot;
    private LinearLayout list;
    private TextView status;
    private EditText manual;
    private final List<String> found = new ArrayList<String>();
    private int searchId = 0;
    private boolean inWeb = false;
    private Runnable autoOpen;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        buildConnectScreen();
        final String last = prefs().getString(KEY_HOST, "");
        if (last.length() > 0) tryLast(last);
        else startSearch();
    }

    private SharedPreferences prefs() { return getSharedPreferences(PREFS, MODE_PRIVATE); }

    private int dp(int v) { return (int) (v * getResources().getDisplayMetrics().density + 0.5f); }

    private Button button(String text, int color) {
        Button bt = new Button(this);
        bt.setText(text);
        bt.setAllCaps(false);
        bt.setTextColor(Color.WHITE);
        bt.setTextSize(16);
        bt.setBackgroundColor(color);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(8), 0, 0);
        bt.setLayoutParams(lp);
        return bt;
    }

    private void buildConnectScreen() {
        connectRoot = new LinearLayout(this);
        connectRoot.setOrientation(LinearLayout.VERTICAL);
        connectRoot.setBackgroundColor(BG);
        connectRoot.setPadding(dp(24), dp(40), dp(24), dp(24));

        TextView title = new TextView(this);
        title.setText("PS4 Chess");
        title.setTextColor(Color.WHITE);
        title.setTextSize(30);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        connectRoot.addView(title);

        status = new TextView(this);
        status.setTextColor(Color.parseColor("#bababa"));
        status.setTextSize(16);
        status.setPadding(0, dp(8), 0, dp(12));
        connectRoot.addView(status);

        ScrollView sv = new ScrollView(this);
        list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        sv.addView(list);
        connectRoot.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        manual = new EditText(this);
        manual.setHint("PS4 address, e.g. 192.168.1.20");
        manual.setHintTextColor(Color.parseColor("#8b8987"));
        manual.setTextColor(Color.WHITE);
        manual.setSingleLine(true);
        manual.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        connectRoot.addView(manual, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        Button go = button("Connect to this address", GREEN);
        go.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                String[] a = Discovery.parseAddress(manual.getText().toString());
                if (a == null) { status.setText("That address does not look right."); return; }
                openHost(a[0], Integer.parseInt(a[1]));
            }
        });
        connectRoot.addView(go);

        Button again = button("Search again", GREY);
        again.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { startSearch(); }
        });
        connectRoot.addView(again);

        setContentView(connectRoot);
        inWeb = false;
    }

    private void showConnect(String message) {
        inWeb = false;
        if (loadTimeout != null) { ui.removeCallbacks(loadTimeout); loadTimeout = null; }
        if (web != null) web.stopLoading();
        setContentView(connectRoot);
        if (message != null) status.setText(message);
    }

    private void tryLast(final String hostPort) {
        status.setText("Connecting to your PS4 at " + hostPort + " ...");
        final String[] a = Discovery.parseAddress(hostPort);
        if (a == null) { startSearch(); return; }
        new Thread(new Runnable() {
            public void run() {
                final boolean ok = Discovery.ping(a[0], Integer.parseInt(a[1]), 900);
                ui.post(new Runnable() {
                    public void run() {
                        if (ok) openHost(a[0], Integer.parseInt(a[1]));
                        else startSearch();
                    }
                });
            }
        }).start();
    }

    private void startSearch() {
        final int id = ++searchId;
        found.clear();
        list.removeAllViews();
        if (autoOpen != null) { ui.removeCallbacks(autoOpen); autoOpen = null; }
        status.setText("Searching for your PS4 on this Wi-Fi...\nOn the PS4 choose: Play on Phone (LAN).");
        Discovery.search(Discovery.HTTP_PORT, Discovery.localPrefixes(false), Discovery.broadcastTargets(), new Discovery.Listener() {
            public void onFound(final String ip, final int port) {
                ui.post(new Runnable() { public void run() { if (id == searchId) addFound(ip, port); } });
            }
            public void onDone(final int total) {
                ui.post(new Runnable() {
                    public void run() {
                        if (id == searchId && total == 0)
                            status.setText("No PS4 found.\nCheck that the game is open, LAN mode is selected, and both devices use the same Wi-Fi. You can also type the address shown on the PS4.");
                    }
                });
            }
        });
    }

    private void addFound(final String ip, final int port) {
        String key = ip + ":" + port;
        if (found.contains(key)) return;
        found.add(key);
        Button bt = button("PS4 at " + ip + "  -  tap to play", GREEN);
        bt.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { openHost(ip, port); }
        });
        list.addView(bt);
        status.setText("Found your PS4!");
        if (autoOpen != null) { ui.removeCallbacks(autoOpen); autoOpen = null; }
        if (found.size() == 1) {
            autoOpen = new Runnable() { public void run() { openHost(ip, port); } };
            ui.postDelayed(autoOpen, 800);
        }
    }

    private void openHost(String ip, int port) {
        if (autoOpen != null) { ui.removeCallbacks(autoOpen); autoOpen = null; }
        searchId++; // stop reacting to the running search
        prefs().edit().putString(KEY_HOST, ip + ":" + port).apply();
        if (web == null) {
            web = new WebView(this);
            WebSettings s = web.getSettings();
            s.setJavaScriptEnabled(true);
            s.setDomStorageEnabled(true);
            s.setMediaPlaybackRequiresUserGesture(false);
            web.setBackgroundColor(BG);
            web.setWebViewClient(new WebViewClient() {
                @Override
                public boolean shouldOverrideUrlLoading(WebView view, String url) { return false; }

                @Override
                public void onPageFinished(WebView view, String url) {
                    if (loadTimeout != null) { ui.removeCallbacks(loadTimeout); loadTimeout = null; }
                    loading.setVisibility(View.GONE);
                }

                @SuppressWarnings("deprecation")
                @Override
                public void onReceivedError(WebView view, int code, String desc, String failingUrl) {
                    showConnect("Could not open the PS4 page (" + desc + "). Searching again...");
                    startSearch();
                }
            });
            loading = new TextView(this);
            loading.setTextColor(Color.WHITE);
            loading.setTextSize(18);
            loading.setGravity(Gravity.CENTER);
            loading.setBackgroundColor(BG);
            webRoot = new FrameLayout(this);
            webRoot.addView(web, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            webRoot.addView(loading, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }
        inWeb = true;
        loading.setText("Opening your PS4 at " + ip + " ...");
        loading.setVisibility(View.VISIBLE);
        setContentView(webRoot);
        web.loadUrl("http://" + ip + ":" + port + "/");
        if (loadTimeout != null) ui.removeCallbacks(loadTimeout);
        loadTimeout = new Runnable() {
            public void run() {
                loadTimeout = null;
                showConnect("The PS4 did not answer in time. Make sure the game is open in LAN mode, then try again.");
                startSearch();
            }
        };
        ui.postDelayed(loadTimeout, 12000);
    }

    @Override
    public void onBackPressed() {
        if (inWeb) { showConnect("Choose a PS4 or search again."); startSearch(); }
        else super.onBackPressed();
    }
}
