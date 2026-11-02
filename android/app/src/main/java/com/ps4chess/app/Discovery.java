package com.ps4chess.app;

import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/** Finds the PS4 chess server on the local network. Pure Java - no Android classes - so it can be tested on a PC. */
public final class Discovery {
    public static final int PORT = 7788;

    public interface Listener {
        void onFound(String ip, int port);
        void onDone(int totalFound);
    }

    private Discovery() {}

    /** True if ip:port is a PS4 chess server (it greets every connection with a line containing PS4CHESS). */
    public static boolean ping(String ip, int port, int timeoutMs) {
        Socket s = new Socket();
        try {
            s.connect(new InetSocketAddress(ip, port), timeoutMs);
            s.setSoTimeout(timeoutMs);
            InputStream in = s.getInputStream();
            byte[] buf = new byte[256];
            StringBuilder sb = new StringBuilder();
            int n;
            while (sb.indexOf("\n") < 0 && sb.length() < 512 && (n = in.read(buf)) > 0) sb.append(new String(buf, 0, n, "US-ASCII"));
            return sb.indexOf("PS4CHESS") >= 0;
        } catch (Exception e) {
            return false;
        } finally {
            try { s.close(); } catch (Exception ignored) { }
        }
    }

    /** "192.168.1." style prefixes of this device's IPv4 networks (Wi-Fi, hotspot, ...). */
    public static List<String> localPrefixes(boolean includeLoopback) {
        List<String> out = new ArrayList<String>();
        try {
            Enumeration<NetworkInterface> ifs = NetworkInterface.getNetworkInterfaces();
            while (ifs != null && ifs.hasMoreElements()) {
                NetworkInterface ni = ifs.nextElement();
                if (!ni.isUp() || (ni.isLoopback() && !includeLoopback)) continue;
                for (InterfaceAddress ia : ni.getInterfaceAddresses()) {
                    InetAddress a = ia.getAddress();
                    if (!(a instanceof Inet4Address)) continue;
                    byte[] b = a.getAddress();
                    String p = (b[0] & 255) + "." + (b[1] & 255) + "." + (b[2] & 255) + ".";
                    if (!out.contains(p)) out.add(p);
                }
            }
        } catch (Exception ignored) { }
        return out;
    }

    /** Directed broadcast addresses of every IPv4 interface, plus the limited broadcast address. */
    public static List<String> broadcastTargets() {
        List<String> out = new ArrayList<String>();
        try {
            Enumeration<NetworkInterface> ifs = NetworkInterface.getNetworkInterfaces();
            while (ifs != null && ifs.hasMoreElements()) {
                NetworkInterface ni = ifs.nextElement();
                if (!ni.isUp() || ni.isLoopback()) continue;
                for (InterfaceAddress ia : ni.getInterfaceAddresses()) {
                    InetAddress b = ia.getBroadcast();
                    if (b != null && !out.contains(b.getHostAddress())) out.add(b.getHostAddress());
                }
            }
        } catch (Exception ignored) { }
        if (!out.contains("255.255.255.255")) out.add("255.255.255.255");
        return out;
    }

    /**
     * Searches in the background: UDP broadcast probes plus a TCP scan of every given /24 prefix.
     * Listener callbacks come from worker threads; each host is reported once.
     */
    public static void search(final int port, final List<String> prefixes, final List<String> udpTargets, final Listener l) {
        final Set<String> seen = Collections.newSetFromMap(new ConcurrentHashMap<String, Boolean>());
        final ExecutorService pool = Executors.newFixedThreadPool(48);
        Thread coordinator = new Thread(new Runnable() {
            public void run() {
                final Object lock = new Object();
                final int[] count = {0};
                final Listener safe = new Listener() {
                    public void onFound(String ip, int p) {
                        if (seen.add(ip)) { synchronized (lock) { count[0]++; } l.onFound(ip, p); }
                    }
                    public void onDone(int t) { }
                };
                pool.submit(new Runnable() { public void run() { udpProbe(port, udpTargets, safe); } });
                for (final String prefix : prefixes) {
                    for (int i = 1; i <= 254; i++) {
                        final String ip = prefix + i;
                        pool.submit(new Runnable() {
                            public void run() { if (ping(ip, port, 350)) safe.onFound(ip, port); }
                        });
                    }
                }
                pool.shutdown();
                try { pool.awaitTermination(30, TimeUnit.SECONDS); } catch (InterruptedException ignored) { }
                int total; synchronized (lock) { total = count[0]; }
                l.onDone(total);
            }
        }, "ps4chess-search");
        coordinator.setDaemon(true);
        coordinator.start();
    }

    private static void udpProbe(int udpPort, List<String> targets, Listener l) {
        DatagramSocket ds = null;
        try {
            ds = new DatagramSocket();
            ds.setBroadcast(true);
            ds.setSoTimeout(250);
            byte[] probe = "PS4CHESS?".getBytes("US-ASCII");
            long end = System.currentTimeMillis() + 2600;
            long nextSend = 0;
            byte[] buf = new byte[256];
            while (System.currentTimeMillis() < end) {
                if (System.currentTimeMillis() >= nextSend) {
                    for (String t : targets) {
                        try { ds.send(new DatagramPacket(probe, probe.length, InetAddress.getByName(t), udpPort)); } catch (Exception ignored) { }
                    }
                    nextSend = System.currentTimeMillis() + 700;
                }
                try {
                    DatagramPacket p = new DatagramPacket(buf, buf.length);
                    ds.receive(p);
                    int port = parseReplyPort(new String(p.getData(), 0, p.getLength(), "US-ASCII"));
                    if (port > 0) l.onFound(p.getAddress().getHostAddress(), port);
                } catch (SocketTimeoutException ignored) { }
            }
        } catch (Exception ignored) {
        } finally {
            if (ds != null) ds.close();
        }
    }

    /** "PS4CHESS|8080|name" -> 8080, otherwise -1. */
    public static int parseReplyPort(String s) {
        if (s == null || !s.startsWith("PS4CHESS|")) return -1;
        String[] parts = s.split("\\|");
        if (parts.length < 2) return -1;
        try {
            int p = Integer.parseInt(parts[1].trim());
            return p > 0 && p < 65536 ? p : -1;
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /** "192.168.1.20" or "192.168.1.20:8080" -> {host, port}. Returns null for junk. */
    public static String[] parseAddress(String text) {
        if (text == null) return null;
        String t = text.trim().replace("http://", "").replace("https://", "");
        int slash = t.indexOf('/');
        if (slash >= 0) t = t.substring(0, slash);
        if (t.length() == 0) return null;
        String host = t, port = String.valueOf(PORT);
        int colon = t.lastIndexOf(':');
        if (colon > 0) {
            host = t.substring(0, colon);
            port = t.substring(colon + 1);
        }
        try {
            int p = Integer.parseInt(port);
            if (p < 1 || p > 65535 || host.length() == 0) return null;
            for (int i = 0; i < host.length(); i++) {
                char c = host.charAt(i);
                if (!(Character.isLetterOrDigit(c) || c == '.' || c == '-')) return null;
            }
            return new String[] {host, String.valueOf(p)};
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
