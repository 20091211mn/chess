package com.ps4chess.app.core;

import java.util.ArrayList;
import java.util.List;

/**
 * Chess rules: legal move generation (0x88-free 120-square mailbox), castling, en passant, promotion,
 * FEN, SAN/UCI, draws. A faithful port of the C++ engine used on the PS4 (verified with the same perft numbers).
 * A move is packed in an int: from | to << 6 | promo << 12 | flags << 16.
 */
public final class Engine {
    private Engine() {}

    public static final int EMPTY = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6;
    public static final int WHITE = 0, BLACK = 1;
    public static final int F_CAP = 1, F_EP = 2, F_CASTLE = 4, F_DOUBLE = 8;
    public static final int CR_WK = 1, CR_WQ = 2, CR_BK = 4, CR_BQ = 8;
    public static final String START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    public static int piece(int type, int color) { return type | (color << 3); }
    public static int type(int pc) { return pc & 7; }
    public static int color(int pc) { return pc >> 3; }
    public static int file(int s) { return s & 7; }
    public static int rank(int s) { return s >> 3; }
    public static int sq(int f, int r) { return r * 8 + f; }

    public static int mv(int from, int to, int promo, int flags) { return from | (to << 6) | (promo << 12) | (flags << 16); }
    public static int from(int m) { return m & 63; }
    public static int to(int m) { return (m >> 6) & 63; }
    public static int promo(int m) { return (m >> 12) & 15; }
    public static int flags(int m) { return (m >> 16) & 15; }

    private static final int[] MB120 = new int[120], MB64 = new int[64];
    private static final int[] KN = {-21, -19, -12, -8, 8, 12, 19, 21}, BI = {-11, -9, 9, 11}, RO = {-10, -1, 1, 10}, KI = {-11, -10, -9, -1, 1, 9, 10, 11};
    static {
        for (int i = 0; i < 120; i++) MB120[i] = -1;
        for (int s = 0; s < 64; s++) { MB64[s] = 21 + (s >> 3) * 10 + (s & 7); MB120[MB64[s]] = s; }
    }

    public static boolean attacked(Pos p, int sq, int by) {
        int s = MB64[sq];
        if (by == WHITE) {
            int a = MB120[s - 9], b = MB120[s - 11];
            if (a >= 0 && p.b[a] == piece(PAWN, WHITE)) return true;
            if (b >= 0 && p.b[b] == piece(PAWN, WHITE)) return true;
        } else {
            int a = MB120[s + 9], b = MB120[s + 11];
            if (a >= 0 && p.b[a] == piece(PAWN, BLACK)) return true;
            if (b >= 0 && p.b[b] == piece(PAWN, BLACK)) return true;
        }
        for (int i = 0; i < 8; i++) {
            int n = MB120[s + KN[i]];
            if (n >= 0 && p.b[n] == piece(KNIGHT, by)) return true;
            int k = MB120[s + KI[i]];
            if (k >= 0 && p.b[k] == piece(KING, by)) return true;
        }
        for (int i = 0; i < 4; i++) {
            int d = BI[i], x = s + d, q;
            while ((q = MB120[x]) >= 0) {
                int pc = p.b[q];
                if (pc != 0) { if (color(pc) == by && (type(pc) == BISHOP || type(pc) == QUEEN)) return true; break; }
                x += d;
            }
            d = RO[i]; x = s + d;
            while ((q = MB120[x]) >= 0) {
                int pc = p.b[q];
                if (pc != 0) { if (color(pc) == by && (type(pc) == ROOK || type(pc) == QUEEN)) return true; break; }
                x += d;
            }
        }
        return false;
    }

    public static int kingSq(Pos p, int c) {
        int k = piece(KING, c);
        for (int i = 0; i < 64; i++) if (p.b[i] == k) return i;
        return -1;
    }

    public static boolean inCheck(Pos p, int c) {
        int k = kingSq(p, c);
        return k >= 0 && attacked(p, k, c ^ 1);
    }

    private static int addPawn(int[] out, int n, int from, int to, int flags) {
        int r = rank(to);
        if (r == 0 || r == 7) {
            out[n++] = mv(from, to, QUEEN, flags); out[n++] = mv(from, to, ROOK, flags);
            out[n++] = mv(from, to, BISHOP, flags); out[n++] = mv(from, to, KNIGHT, flags);
        } else out[n++] = mv(from, to, 0, flags);
        return n;
    }

    /** Pseudo-legal moves (the king may be left in check). out needs room for 256. */
    public static int genPseudo(Pos p, int[] out, boolean capsOnly) {
        int n = 0, us = p.side, them = us ^ 1;
        for (int s = 0; s < 64; s++) {
            int pc = p.b[s];
            if (pc == 0 || color(pc) != us) continue;
            int ty = type(pc), s120 = MB64[s];
            if (ty == PAWN) {
                int dir = us == WHITE ? 10 : -10, startRank = us == WHITE ? 1 : 6;
                int f1 = MB120[s120 + dir];
                if (f1 >= 0 && p.b[f1] == 0) {
                    int pr = rank(f1);
                    boolean promo = pr == 0 || pr == 7;
                    if (!capsOnly || promo) n = addPawn(out, n, s, f1, 0);
                    if (!capsOnly && rank(s) == startRank) {
                        int f2 = MB120[s120 + 2 * dir];
                        if (f2 >= 0 && p.b[f2] == 0) out[n++] = mv(s, f2, 0, F_DOUBLE);
                    }
                }
                for (int side = -1; side <= 1; side += 2) {
                    int c = MB120[s120 + dir + side];
                    if (c < 0) continue;
                    if (p.b[c] != 0 && color(p.b[c]) == them) n = addPawn(out, n, s, c, F_CAP);
                    else if (c == p.ep && p.b[c] == 0) out[n++] = mv(s, c, 0, F_CAP | F_EP);
                }
            } else if (ty == KNIGHT || ty == KING) {
                int[] off = ty == KNIGHT ? KN : KI;
                for (int i = 0; i < 8; i++) {
                    int q = MB120[s120 + off[i]];
                    if (q < 0) continue;
                    if (p.b[q] == 0) { if (!capsOnly) out[n++] = mv(s, q, 0, 0); }
                    else if (color(p.b[q]) == them) out[n++] = mv(s, q, 0, F_CAP);
                }
                if (ty == KING && !capsOnly) {
                    int hr = us == WHITE ? 0 : 7;
                    if (s == sq(4, hr) && !attacked(p, s, them)) {
                        int ks = us == WHITE ? CR_WK : CR_BK, qs = us == WHITE ? CR_WQ : CR_BQ;
                        if ((p.castle & ks) != 0 && p.b[sq(7, hr)] == piece(ROOK, us) && p.b[sq(5, hr)] == 0 && p.b[sq(6, hr)] == 0
                                && !attacked(p, sq(5, hr), them) && !attacked(p, sq(6, hr), them))
                            out[n++] = mv(s, sq(6, hr), 0, F_CASTLE);
                        if ((p.castle & qs) != 0 && p.b[sq(0, hr)] == piece(ROOK, us) && p.b[sq(1, hr)] == 0 && p.b[sq(2, hr)] == 0 && p.b[sq(3, hr)] == 0
                                && !attacked(p, sq(2, hr), them) && !attacked(p, sq(3, hr), them))
                            out[n++] = mv(s, sq(2, hr), 0, F_CASTLE);
                    }
                }
            } else {
                boolean diag = ty == BISHOP || ty == QUEEN, orth = ty == ROOK || ty == QUEEN;
                for (int pass = 0; pass < 2; pass++) {
                    if (pass == 0 && !diag) continue;
                    if (pass == 1 && !orth) continue;
                    int[] off = pass == 0 ? BI : RO;
                    for (int i = 0; i < 4; i++) {
                        int d = off[i], x = s120 + d, q;
                        while ((q = MB120[x]) >= 0) {
                            if (p.b[q] == 0) { if (!capsOnly) out[n++] = mv(s, q, 0, 0); }
                            else { if (color(p.b[q]) == them) out[n++] = mv(s, q, 0, F_CAP); break; }
                            x += d;
                        }
                    }
                }
            }
        }
        return n;
    }

    /** q = p with move m applied. */
    public static void make(Pos p, int m, Pos q) {
        q.copyFrom(p);
        int us = p.side, from = from(m), to = to(m), fl = flags(m), pc = q.b[from], captured = q.b[to];
        q.half++;
        if (type(pc) == PAWN || captured != 0) q.half = 0;
        q.b[to] = (byte) pc; q.b[from] = 0;
        if ((fl & F_EP) != 0) { q.b[us == WHITE ? to - 8 : to + 8] = 0; q.half = 0; }
        if (promo(m) != 0) q.b[to] = (byte) piece(promo(m), us);
        if ((fl & F_CASTLE) != 0) {
            int r = rank(to);
            if (file(to) == 6) { q.b[sq(5, r)] = q.b[sq(7, r)]; q.b[sq(7, r)] = 0; }
            else { q.b[sq(3, r)] = q.b[sq(0, r)]; q.b[sq(0, r)] = 0; }
        }
        if (type(pc) == KING) q.castle &= us == WHITE ? ~(CR_WK | CR_WQ) : ~(CR_BK | CR_BQ);
        for (int i = 0; i < 2; i++) {
            switch (i == 0 ? from : to) {
                case 0: q.castle &= ~CR_WQ; break;
                case 7: q.castle &= ~CR_WK; break;
                case 56: q.castle &= ~CR_BQ; break;
                case 63: q.castle &= ~CR_BK; break;
                case 4: q.castle &= ~(CR_WK | CR_WQ); break;
                case 60: q.castle &= ~(CR_BK | CR_BQ); break;
                default: break;
            }
        }
        q.ep = (fl & F_DOUBLE) != 0 ? (us == WHITE ? from + 8 : from - 8) : -1;
        if (us == BLACK) q.full++;
        q.side = us ^ 1;
    }

    public static int genLegal(Pos p, int[] out) {
        int[] tmp = new int[256];
        int n = genPseudo(p, tmp, false), k = 0;
        Pos q = new Pos();
        for (int i = 0; i < n; i++) {
            make(p, tmp[i], q);
            if (!inCheck(q, p.side)) out[k++] = tmp[i];
        }
        return k;
    }

    public static boolean hasLegal(Pos p) { return genLegal(p, new int[256]) > 0; }

    // ------------------------------------------------------------ FEN
    public static Pos fromFen(String fen) {
        Pos p = new Pos();
        String[] t = fen.trim().split("\\s+");
        if (t.length < 2) return null;
        int r = 7, f = 0;
        for (int i = 0; i < t[0].length(); i++) {
            char c = t[0].charAt(i);
            if (c == '/') { r--; f = 0; continue; }
            if (c >= '1' && c <= '8') { f += c - '0'; continue; }
            int col = Character.isUpperCase(c) ? WHITE : BLACK, ty;
            switch (Character.toLowerCase(c)) {
                case 'p': ty = PAWN; break; case 'n': ty = KNIGHT; break; case 'b': ty = BISHOP; break;
                case 'r': ty = ROOK; break; case 'q': ty = QUEEN; break; case 'k': ty = KING; break;
                default: return null;
            }
            if (r < 0 || f > 7) return null;
            p.b[sq(f, r)] = (byte) piece(ty, col); f++;
        }
        p.side = t[1].equals("b") ? BLACK : WHITE;
        if (t.length > 2) for (char c : t[2].toCharArray()) {
            if (c == 'K') p.castle |= CR_WK; else if (c == 'Q') p.castle |= CR_WQ; else if (c == 'k') p.castle |= CR_BK; else if (c == 'q') p.castle |= CR_BQ;
        }
        if (t.length > 3 && t[3].length() == 2) p.ep = sq(t[3].charAt(0) - 'a', t[3].charAt(1) - '1');
        try {
            if (t.length > 4) p.half = Integer.parseInt(t[4]);
            if (t.length > 5) p.full = Math.max(1, Integer.parseInt(t[5]));
        } catch (NumberFormatException e) { return null; }
        return kingSq(p, WHITE) >= 0 && kingSq(p, BLACK) >= 0 ? p : null;
    }

    public static char pieceChar(int pc) {
        char c = " PNBRQK".charAt(type(pc));
        return color(pc) == BLACK ? Character.toLowerCase(c) : c;
    }

    public static String toFen(Pos p) {
        StringBuilder s = new StringBuilder();
        for (int r = 7; r >= 0; r--) {
            int e = 0;
            for (int f = 0; f < 8; f++) {
                int pc = p.b[sq(f, r)];
                if (pc == 0) { e++; continue; }
                if (e > 0) { s.append(e); e = 0; }
                s.append(pieceChar(pc));
            }
            if (e > 0) s.append(e);
            if (r > 0) s.append('/');
        }
        s.append(p.side == WHITE ? " w " : " b ");
        if (p.castle == 0) s.append('-');
        else {
            if ((p.castle & CR_WK) != 0) s.append('K'); if ((p.castle & CR_WQ) != 0) s.append('Q');
            if ((p.castle & CR_BK) != 0) s.append('k'); if ((p.castle & CR_BQ) != 0) s.append('q');
        }
        s.append(' ');
        if (p.ep < 0) s.append('-'); else s.append(sqName(p.ep));
        return s.append(' ').append(p.half).append(' ').append(p.full).toString();
    }

    public static Pos start() { return fromFen(START_FEN); }

    // ------------------------------------------------------------ notation
    public static String sqName(int s) { return "" + (char) ('a' + file(s)) + (char) ('1' + rank(s)); }

    public static String uci(int m) {
        String s = sqName(from(m)) + sqName(to(m));
        return promo(m) != 0 ? s + "  nbrq".charAt(promo(m)) : s;
    }

    /** Legal move matching "e2e4" / "e7e8q", or -1. */
    public static int parseUci(Pos p, String u) {
        if (u == null || u.length() < 4) return -1;
        int f1 = u.charAt(0) - 'a', r1 = u.charAt(1) - '1', f2 = u.charAt(2) - 'a', r2 = u.charAt(3) - '1', promo = 0;
        if (f1 < 0 || f1 > 7 || f2 < 0 || f2 > 7 || r1 < 0 || r1 > 7 || r2 < 0 || r2 > 7) return -1;
        if (u.length() >= 5) {
            switch (Character.toLowerCase(u.charAt(4))) {
                case 'q': promo = QUEEN; break; case 'r': promo = ROOK; break; case 'b': promo = BISHOP; break; case 'n': promo = KNIGHT; break;
                default: return -1;
            }
        }
        int[] L = new int[256];
        int n = genLegal(p, L);
        for (int i = 0; i < n; i++) if (from(L[i]) == sq(f1, r1) && to(L[i]) == sq(f2, r2) && promo(L[i]) == promo) return L[i];
        return -1;
    }

    public static String san(Pos p, int m) {
        StringBuilder s = new StringBuilder();
        int pc = p.b[from(m)], ty = type(pc), fl = flags(m);
        if ((fl & F_CASTLE) != 0) s.append(file(to(m)) == 6 ? "O-O" : "O-O-O");
        else {
            boolean cap = (fl & F_CAP) != 0;
            if (ty == PAWN) { if (cap) s.append((char) ('a' + file(from(m)))).append('x'); }
            else {
                s.append(" PNBRQK".charAt(ty));
                int[] L = new int[256];
                int n = genLegal(p, L);
                boolean amb = false, sameFile = false, sameRank = false;
                for (int i = 0; i < n; i++) {
                    if (from(L[i]) == from(m) || to(L[i]) != to(m) || type(p.b[from(L[i])]) != ty) continue;
                    amb = true;
                    if (file(from(L[i])) == file(from(m))) sameFile = true;
                    if (rank(from(L[i])) == rank(from(m))) sameRank = true;
                }
                if (amb) { if (!sameFile) s.append((char) ('a' + file(from(m)))); else if (!sameRank) s.append((char) ('1' + rank(from(m)))); else s.append(sqName(from(m))); }
                if (cap) s.append('x');
            }
            s.append(sqName(to(m)));
            if (promo(m) != 0) s.append('=').append(" PNBRQK".charAt(promo(m)));
        }
        Pos q = new Pos();
        make(p, m, q);
        if (inCheck(q, q.side)) s.append(hasLegal(q) ? "+" : "#");
        return s.toString();
    }

    // ------------------------------------------------------------ draws & helpers
    public static boolean insufficientMaterial(Pos p) {
        int minors = 0, light = 0, dark = 0, knights = 0;
        for (int s = 0; s < 64; s++) {
            int pc = p.b[s];
            if (pc == 0) continue;
            int ty = type(pc);
            if (ty == PAWN || ty == ROOK || ty == QUEEN) return false;
            if (ty == KNIGHT) { minors++; knights++; }
            if (ty == BISHOP) { minors++; if (((file(s) + rank(s)) & 1) == 0) dark++; else light++; }
        }
        if (minors <= 1) return true;
        return knights == 0 && (light == 0 || dark == 0);
    }

    public static long posKey(Pos p) {
        long h = 1469598103934665603L;
        for (int i = 0; i < 64; i++) { h ^= (long) (p.b[i] + 1); h *= 1099511628211L; }
        h ^= (long) p.side + 77; h *= 1099511628211L;
        h ^= (long) p.castle + 99; h *= 1099511628211L;
        int ep = -1;
        if (p.ep >= 0) {
            int[] L = new int[256]; int n = genLegal(p, L);
            for (int i = 0; i < n; i++) if ((flags(L[i]) & F_EP) != 0) { ep = p.ep; break; }
        }
        h ^= (long) (ep + 5); h *= 1099511628211L;
        return h;
    }

    public static long perft(Pos p, int depth) {
        int[] L = new int[256];
        int n = genLegal(p, L);
        if (depth <= 1) return n;
        long total = 0;
        Pos q = new Pos();
        for (int i = 0; i < n; i++) { make(p, L[i], q); total += perft(q.copy(), depth - 1); }
        return total;
    }

    public static List<Integer> legalList(Pos p) {
        int[] L = new int[256]; int n = genLegal(p, L);
        List<Integer> out = new ArrayList<Integer>();
        for (int i = 0; i < n; i++) out.add(L[i]);
        return out;
    }
}
