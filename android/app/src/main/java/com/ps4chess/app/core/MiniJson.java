package com.ps4chess.app.core;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tiny JSON reader (objects -> Map, arrays -> List, numbers -> Double, strings, Boolean, null). No dependencies. */
public final class MiniJson {
    private final String s;
    private int i;

    private MiniJson(String s) { this.s = s; }

    public static Object parse(String text) {
        try {
            MiniJson j = new MiniJson(text);
            j.ws();
            Object v = j.value();
            j.ws();
            return j.i == j.s.length() ? v : null;
        } catch (RuntimeException e) {
            return null;
        }
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> object(String text) {
        Object o = parse(text);
        return o instanceof Map ? (Map<String, Object>) o : null;
    }

    private void ws() { while (i < s.length() && Character.isWhitespace(s.charAt(i))) i++; }

    private Object value() {
        char c = s.charAt(i);
        if (c == '{') return obj();
        if (c == '[') return arr();
        if (c == '"') return str();
        if (s.startsWith("true", i)) { i += 4; return Boolean.TRUE; }
        if (s.startsWith("false", i)) { i += 5; return Boolean.FALSE; }
        if (s.startsWith("null", i)) { i += 4; return null; }
        int st = i;
        while (i < s.length() && "+-0123456789.eE".indexOf(s.charAt(i)) >= 0) i++;
        if (st == i) throw new IllegalStateException("bad json");
        return Double.valueOf(s.substring(st, i));
    }

    private Map<String, Object> obj() {
        Map<String, Object> m = new HashMap<String, Object>();
        i++; ws();
        if (s.charAt(i) == '}') { i++; return m; }
        for (;;) {
            ws(); String k = str(); ws();
            if (s.charAt(i++) != ':') throw new IllegalStateException("colon");
            ws(); m.put(k, value()); ws();
            char c = s.charAt(i++);
            if (c == '}') return m;
            if (c != ',') throw new IllegalStateException("comma");
        }
    }

    private List<Object> arr() {
        List<Object> l = new ArrayList<Object>();
        i++; ws();
        if (s.charAt(i) == ']') { i++; return l; }
        for (;;) {
            ws(); l.add(value()); ws();
            char c = s.charAt(i++);
            if (c == ']') return l;
            if (c != ',') throw new IllegalStateException("comma");
        }
    }

    private String str() {
        if (s.charAt(i) != '"') throw new IllegalStateException("quote");
        StringBuilder b = new StringBuilder();
        i++;
        for (;;) {
            char c = s.charAt(i++);
            if (c == '"') return b.toString();
            if (c != '\\') { b.append(c); continue; }
            char e = s.charAt(i++);
            switch (e) {
                case 'n': b.append('\n'); break; case 't': b.append('\t'); break; case 'r': b.append('\r'); break;
                case 'b': b.append('\b'); break; case 'f': b.append('\f'); break;
                case 'u': b.append((char) Integer.parseInt(s.substring(i, i + 4), 16)); i += 4; break;
                default: b.append(e);
            }
        }
    }
}
