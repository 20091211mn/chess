package com.ps4chess.app.core;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** One state message from the PS4 ({"t":"state",...}). */
public final class LanState {
    public String fen = Engine.START_FEN, turn = "w", last = "", reason = "", you = "s", err = "";
    public final List<String> legal = new ArrayList<String>(), san = new ArrayList<String>();
    public boolean check;
    public int res, plies, draw = -1, clients;
    public long wt = -1, bt = -1;
    public boolean hasEval;
    public int evalCp;

    private static String str(Map<String, Object> m, String k, String d) { Object o = m.get(k); return o instanceof String ? (String) o : d; }
    private static int num(Map<String, Object> m, String k, int d) { Object o = m.get(k); return o instanceof Double ? (int) Math.round((Double) o) : d; }
    private static long lnum(Map<String, Object> m, String k, long d) { Object o = m.get(k); return o instanceof Double ? Math.round((Double) o) : d; }

    @SuppressWarnings("unchecked")
    public static LanState parse(Map<String, Object> m) {
        if (m == null || !"state".equals(m.get("t"))) return null;
        LanState s = new LanState();
        s.fen = str(m, "fen", s.fen); s.turn = str(m, "turn", "w"); s.last = str(m, "last", ""); s.reason = str(m, "reason", "");
        s.you = str(m, "you", "s"); s.err = str(m, "err", "");
        s.check = Boolean.TRUE.equals(m.get("check"));
        s.res = num(m, "res", 0); s.plies = num(m, "plies", 0); s.draw = num(m, "draw", -1); s.clients = num(m, "clients", 0);
        s.wt = lnum(m, "wt", -1); s.bt = lnum(m, "bt", -1);
        if (m.get("ev") instanceof Double) { s.hasEval = true; s.evalCp = num(m, "ev", 0); }
        Object l = m.get("legal"), a = m.get("san");
        if (l instanceof List) for (Object o : (List<Object>) l) if (o instanceof String) s.legal.add((String) o);
        if (a instanceof List) for (Object o : (List<Object>) a) if (o instanceof String) s.san.add((String) o);
        return s;
    }
}
