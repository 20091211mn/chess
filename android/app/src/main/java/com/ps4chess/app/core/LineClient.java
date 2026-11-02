package com.ps4chess.app.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

/**
 * Plain TCP client for the PS4: one message per line. The PS4 greets every connection with
 * {"t":"hello","app":"PS4CHESS",...}. No HTTP, no WebSocket - just a socket.
 */
public final class LineClient {
    public interface Handler { void onLine(String line); void onClose(String why); }

    private Socket sock;
    private InputStream in;
    private OutputStream out;
    private volatile boolean closed;
    private final byte[] buf = new byte[4096];
    private int bufLen, bufPos;

    /** Connects and returns the PS4's greeting line. Throws if there is no PS4 chess server behind the address. */
    public String connect(String host, int port, int timeoutMs) throws IOException {
        sock = new Socket();
        sock.connect(new InetSocketAddress(host, port), timeoutMs);
        sock.setTcpNoDelay(true);
        sock.setSoTimeout(timeoutMs);
        in = sock.getInputStream();
        out = sock.getOutputStream();
        String hello = readLine();
        if (hello == null || !hello.contains("PS4CHESS")) throw new IOException("not a PS4 Chess server");
        sock.setSoTimeout(0);
        return hello;
    }

    public synchronized void sendLine(String line) throws IOException {
        if (closed || out == null) throw new IOException("closed");
        out.write((line + "\n").getBytes("UTF-8"));
        out.flush();
    }

    /** Next line without its terminator, or null at end of stream. */
    public String readLine() throws IOException {
        ByteArrayOutputStream cur = new ByteArrayOutputStream();
        for (;;) {
            if (bufPos >= bufLen) {
                bufLen = in.read(buf, 0, buf.length);
                bufPos = 0;
                if (bufLen < 0) { bufLen = 0; return cur.size() > 0 ? cur.toString("UTF-8") : null; }
            }
            byte b = buf[bufPos++];
            if (b == '\n') { String s = cur.toString("UTF-8"); return s.endsWith("\r") ? s.substring(0, s.length() - 1) : s; }
            cur.write(b);
            if (cur.size() > 1_000_000) throw new IOException("line too long");
        }
    }

    /** Blocks, dispatching incoming lines, until the connection ends. */
    public void readLoop(Handler h) {
        String why = "closed";
        try {
            String line;
            while (!closed && (line = readLine()) != null) if (line.length() > 0) h.onLine(line);
            if (!closed) why = "the PS4 closed the connection";
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
