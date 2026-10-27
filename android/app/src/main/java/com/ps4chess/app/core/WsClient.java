package com.ps4chess.app.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.security.MessageDigest;
import java.security.SecureRandom;

/** Minimal WebSocket client (RFC 6455, text frames only). Blocking reads: call readLoop() on its own thread. */
public final class WsClient {
    public interface Handler { void onText(String text); void onClose(String why); }

    private static final String GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    private Socket sock;
    private InputStream in;
    private OutputStream out;
    private volatile boolean closed;

    public static String base64(byte[] d) {
        final String T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < d.length; i += 3) {
            int b0 = d[i] & 255, b1 = i + 1 < d.length ? d[i + 1] & 255 : 0, b2 = i + 2 < d.length ? d[i + 2] & 255 : 0;
            sb.append(T.charAt(b0 >> 2)).append(T.charAt(((b0 & 3) << 4) | (b1 >> 4)));
            sb.append(i + 1 < d.length ? T.charAt(((b1 & 15) << 2) | (b2 >> 6)) : '=');
            sb.append(i + 2 < d.length ? T.charAt(b2 & 63) : '=');
        }
        return sb.toString();
    }

    public static String accept(String key) throws Exception {
        return base64(MessageDigest.getInstance("SHA-1").digest((key + GUID).getBytes("US-ASCII")));
    }

    /** Opens the socket and performs the upgrade handshake. Throws on any failure. */
    public void connect(String host, int port, String path, int timeoutMs) throws Exception {
        sock = new Socket();
        sock.connect(new InetSocketAddress(host, port), timeoutMs);
        sock.setTcpNoDelay(true);
        sock.setSoTimeout(timeoutMs);
        in = sock.getInputStream(); out = sock.getOutputStream();
        byte[] kb = new byte[16];
        new SecureRandom().nextBytes(kb);
        String key = base64(kb);
        out.write(("GET " + path + " HTTP/1.1\r\nHost: " + host + ":" + port + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key
                + "\r\nSec-WebSocket-Version: 13\r\n\r\n").getBytes("US-ASCII"));
        out.flush();
        StringBuilder head = new StringBuilder();
        while (head.indexOf("\r\n\r\n") < 0) {
            int c = in.read();
            if (c < 0) throw new IOException("connection closed during handshake");
            head.append((char) c);
            if (head.length() > 8192) throw new IOException("handshake too long");
        }
        String h = head.toString();
        if (!h.startsWith("HTTP/1.1 101")) throw new IOException("server refused the WebSocket upgrade");
        if (!h.contains(accept(key))) throw new IOException("bad Sec-WebSocket-Accept");
        sock.setSoTimeout(0);
    }

    public synchronized void sendText(String text) throws IOException {
        if (closed || out == null) throw new IOException("closed");
        byte[] pl = text.getBytes("UTF-8");
        ByteArrayOutputStream f = new ByteArrayOutputStream();
        f.write(0x81);
        if (pl.length < 126) f.write(0x80 | pl.length);
        else { f.write(0x80 | 126); f.write(pl.length >> 8); f.write(pl.length & 255); }
        byte[] mask = new byte[4];
        new SecureRandom().nextBytes(mask);
        f.write(mask);
        for (int i = 0; i < pl.length; i++) f.write(pl[i] ^ mask[i & 3]);
        out.write(f.toByteArray());
        out.flush();
    }

    private void readFully(byte[] b, int n) throws IOException {
        int got = 0;
        while (got < n) { int r = in.read(b, got, n - got); if (r < 0) throw new IOException("closed"); got += r; }
    }

    /** Blocks, dispatching incoming messages, until the connection ends. */
    public void readLoop(Handler h) {
        String why = "closed";
        StringBuilder frag = new StringBuilder();
        try {
            while (!closed) {
                byte[] hd = new byte[2];
                readFully(hd, 2);
                int op = hd[0] & 15; boolean fin = (hd[0] & 0x80) != 0, masked = (hd[1] & 0x80) != 0;
                long len = hd[1] & 127;
                if (len == 126) { byte[] e = new byte[2]; readFully(e, 2); len = ((e[0] & 255) << 8) | (e[1] & 255); }
                else if (len == 127) { byte[] e = new byte[8]; readFully(e, 8); len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | (e[i] & 255); }
                if (len > 2_000_000) throw new IOException("frame too large");
                byte[] mk = new byte[4];
                if (masked) readFully(mk, 4);
                byte[] pl = new byte[(int) len];
                readFully(pl, (int) len);
                if (masked) for (int i = 0; i < pl.length; i++) pl[i] ^= mk[i & 3];
                if (op == 8) { why = "server closed the connection"; break; }
                if (op == 9) { synchronized (this) { out.write(0x8A); out.write(0x80 | Math.min(pl.length, 125)); out.write(new byte[4]); out.write(pl, 0, Math.min(pl.length, 125)); out.flush(); } continue; }
                if (op == 1 || op == 0) {
                    frag.append(new String(pl, "UTF-8"));
                    if (fin) { String t = frag.toString(); frag.setLength(0); h.onText(t); }
                }
            }
        } catch (Exception e) {
            why = closed ? "closed" : String.valueOf(e.getMessage());
        }
        close();
        h.onClose(why);
    }

    public void close() {
        closed = true;
        try { if (sock != null) sock.close(); } catch (Exception ignored) { }
    }

    public boolean isClosed() { return closed; }
}
