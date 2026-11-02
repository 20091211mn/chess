package com.ps4chess.app.core;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Keeps a plain TCP connection to the PS4 alive: heartbeat, automatic reconnect, state/history callbacks.
 * Callbacks arrive on worker threads - marshal them to the UI thread yourself.
 */
public final class LanSession {
    public interface Listener {
        void onState(LanState s);
        void onHistory(List<String> fens);
        void onStatus(String text, boolean connected);
    }

    private final Listener l;
    private volatile boolean active;
    private volatile LineClient ws;
    private volatile String host = "";
    private volatile int port = 7788;
    private Thread worker;
    private int generation;

    public LanSession(Listener l) { this.l = l; }

    public synchronized void start(final String h, final int p) {
        stop();
        host = h; port = p; active = true;
        final int gen = ++generation;
        worker = new Thread(new Runnable() { public void run() { loop(gen); } }, "ps4chess-lan");
        worker.setDaemon(true);
        worker.start();
    }

    public synchronized void stop() {
        active = false; generation++;
        LineClient c = ws;
        if (c != null) c.close();
        ws = null;
    }

    public boolean isActive() { return active; }

    public void send(String msg) {
        LineClient c = ws;
        if (c == null) return;
        try { c.sendLine(msg); } catch (Exception e) { c.close(); }
    }

    private void loop(final int gen) {
        long backoff = 700;
        while (active && gen == generation) {
            final LineClient c = new LineClient();
            try {
                l.onStatus("Connecting to " + host + " ...", false);
                c.connect(host, port, 3500);
                ws = c;
                backoff = 700;
                l.onStatus("Connected to PS4", true);
                final long[] lastRx = {System.currentTimeMillis()};
                Thread hb = new Thread(new Runnable() {
                    public void run() {
                        try {
                            while (!c.isClosed() && gen == generation) {
                                Thread.sleep(4000);
                                c.sendLine("ping");
                                if (System.currentTimeMillis() - lastRx[0] > 11000) { c.close(); break; }
                            }
                        } catch (Exception e) { c.close(); }
                    }
                }, "ps4chess-hb");
                hb.setDaemon(true); hb.start();
                c.sendLine("hello");
                c.readLoop(new LineClient.Handler() {
                    public void onLine(String t) {
                        lastRx[0] = System.currentTimeMillis();
                        Map<String, Object> m = MiniJson.object(t);
                        if (m == null) return;
                        Object type = m.get("t");
                        if ("state".equals(type)) { LanState s = LanState.parse(m); if (s != null) l.onState(s); }
                        else if ("hist".equals(type) && m.get("fens") instanceof List) {
                            List<String> fens = new ArrayList<String>();
                            for (Object o : (List<?>) m.get("fens")) if (o instanceof String) fens.add((String) o);
                            l.onHistory(fens);
                        }
                    }
                    public void onClose(String why) { }
                });
                c.close();
                if (active && gen == generation) l.onStatus("Connection lost - reconnecting ...", false);
            } catch (Exception e) {
                c.close();
                if (active && gen == generation) l.onStatus("Cannot reach the PS4 (" + e.getMessage() + ") - retrying ...", false);
            }
            ws = null;
            try { Thread.sleep(backoff); } catch (InterruptedException ignored) { }
            backoff = Math.min(3000, backoff * 2);
        }
    }
}
