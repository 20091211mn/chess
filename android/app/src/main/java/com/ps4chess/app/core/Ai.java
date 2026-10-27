package com.ps4chess.app.core;

/** Iterative-deepening negamax with alpha-beta, quiescence, MVV-LVA and killer moves. Deterministic node budgets. */
public final class Ai {
    public static final class Level {
        public final String name; public final int depth, noise, blunderPct; public final long nodes;
        Level(String n, int d, long nodes, int noise, int blunder) { name = n; depth = d; this.nodes = nodes; this.noise = noise; blunderPct = blunder; }
    }
    public static final Level[] LEVELS = {
        new Level("Beginner", 1, 2000, 350, 30), new Level("Easy", 2, 20000, 150, 8), new Level("Medium", 3, 120000, 40, 0),
        new Level("Hard", 4, 600000, 0, 0), new Level("Expert", 6, 2500000, 0, 0)};

    private static final int[] VAL = {0, 100, 320, 330, 500, 900, 0};
    private static final int[][] PST = {
        new int[64],
        {0,0,0,0,0,0,0,0, 50,50,50,50,50,50,50,50, 10,10,20,30,30,20,10,10, 5,5,10,25,25,10,5,5, 0,0,0,20,20,0,0,0, 5,-5,-10,0,0,-10,-5,5, 5,10,10,-20,-20,10,10,5, 0,0,0,0,0,0,0,0},
        {-50,-40,-30,-30,-30,-30,-40,-50, -40,-20,0,0,0,0,-20,-40, -30,0,10,15,15,10,0,-30, -30,5,15,20,20,15,5,-30, -30,0,15,20,20,15,0,-30, -30,5,10,15,15,10,5,-30, -40,-20,0,5,5,0,-20,-40, -50,-40,-30,-30,-30,-30,-40,-50},
        {-20,-10,-10,-10,-10,-10,-10,-20, -10,0,0,0,0,0,0,-10, -10,0,5,10,10,5,0,-10, -10,5,5,10,10,5,5,-10, -10,0,10,10,10,10,0,-10, -10,10,10,10,10,10,10,-10, -10,5,0,0,0,0,5,-10, -20,-10,-10,-10,-10,-10,-10,-20},
        {0,0,0,0,0,0,0,0, 5,10,10,10,10,10,10,5, -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5, 0,0,0,5,5,0,0,0},
        {-20,-10,-10,-5,-5,-10,-10,-20, -10,0,0,0,0,0,0,-10, -10,0,5,5,5,5,0,-10, -5,0,5,5,5,5,0,-5, 0,0,5,5,5,5,0,-5, -10,5,5,5,5,5,0,-10, -10,0,5,0,0,0,0,-10, -20,-10,-10,-5,-5,-10,-10,-20},
        {-30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30, -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10, 20,20,0,0,0,0,20,20, 20,30,10,0,0,10,30,20}};
    private static final int[] KING_END = {
        -50,-40,-30,-20,-20,-30,-40,-50, -30,-20,-10,0,0,-10,-20,-30, -30,-10,20,30,30,20,-10,-30, -30,-10,30,40,40,30,-10,-30,
        -30,-10,30,40,40,30,-10,-30, -30,-10,20,30,30,20,-10,-30, -30,-30,0,0,0,0,-30,-30, -50,-30,-30,-30,-30,-30,-30,-50};

    private long nodes, maxNodes;
    private boolean stop;
    private final int[][] killers = new int[64][2];
    private int rootDepth;
    private long rng = 88172645463325252L;
    private final Pos[] stack = new Pos[80];
    private final int[][] movesAt = new int[80][256];

    public Ai() { for (int i = 0; i < stack.length; i++) stack[i] = new Pos(); }

    private int rnd() { rng ^= rng << 13; rng ^= rng >>> 7; rng ^= rng << 17; return (int) (rng >>> 33); }

    public static int evaluate(Pos p) {
        int score = 0, nonPawn = 0;
        for (int s = 0; s < 64; s++) {
            int pc = p.b[s];
            if (pc == 0) continue;
            int ty = Engine.type(pc);
            if (ty != Engine.PAWN && ty != Engine.KING) nonPawn += VAL[ty];
        }
        boolean endgame = nonPawn < 1400;
        for (int s = 0; s < 64; s++) {
            int pc = p.b[s];
            if (pc == 0) continue;
            int ty = Engine.type(pc), c = Engine.color(pc), idx = c == Engine.WHITE ? (s ^ 56) : s;
            int v = VAL[ty] + ((ty == Engine.KING && endgame) ? KING_END[idx] : PST[ty][idx]);
            score += c == Engine.WHITE ? v : -v;
        }
        return p.side == Engine.WHITE ? score : -score;
    }

    private int scoreMove(Pos p, int m, int ply) {
        int sc = 0, fl = Engine.flags(m);
        if ((fl & Engine.F_CAP) != 0) {
            int victim = (fl & Engine.F_EP) != 0 ? Engine.PAWN : Engine.type(p.b[Engine.to(m)]);
            sc = 10000 + VAL[victim] * 10 - VAL[Engine.type(p.b[Engine.from(m)])] / 10;
        } else if (ply < 64 && m == killers[ply][0]) sc = 9000;
        else if (ply < 64 && m == killers[ply][1]) sc = 8000;
        if (Engine.promo(m) != 0) sc += 9500 + VAL[Engine.promo(m)];
        return sc;
    }

    private void order(Pos p, int[] mv, int n, int ply) {
        int[] sc = new int[n];
        for (int i = 0; i < n; i++) sc[i] = scoreMove(p, mv[i], ply);
        for (int i = 1; i < n; i++) {
            int s = sc[i], m = mv[i], j = i - 1;
            while (j >= 0 && sc[j] < s) { sc[j + 1] = sc[j]; mv[j + 1] = mv[j]; j--; }
            sc[j + 1] = s; mv[j + 1] = m;
        }
    }

    private int quiesce(Pos p, int alpha, int beta, int ply) {
        nodes++;
        if (maxNodes > 0 && nodes > maxNodes) { stop = true; return 0; }
        int stand = evaluate(p);
        if (ply > 40) return stand;
        if (stand >= beta) return stand;
        if (stand > alpha) alpha = stand;
        int[] mv = movesAt[ply];
        int n = Engine.genPseudo(p, mv, true);
        order(p, mv, n, 63);
        Pos q = stack[ply + 1];
        for (int i = 0; i < n; i++) {
            Engine.make(p, mv[i], q);
            if (Engine.inCheck(q, p.side)) continue;
            int sc = -quiesce(q, -beta, -alpha, ply + 1);
            if (stop) return 0;
            if (sc >= beta) return sc;
            if (sc > alpha) alpha = sc;
        }
        return alpha;
    }

    private int search(Pos p, int depth, int alpha, int beta, int ply) {
        nodes++;
        if (maxNodes > 0 && nodes > maxNodes) { stop = true; return 0; }
        boolean chk = Engine.inCheck(p, p.side);
        if (chk && ply < rootDepth + 8) depth++;
        if (depth <= 0 || ply >= 70) return quiesce(p, alpha, beta, ply);
        if (p.half >= 100) return 0;
        int[] mv = movesAt[ply];
        int n = Engine.genPseudo(p, mv, false);
        order(p, mv, n, ply);
        Pos q = stack[ply + 1];
        int legal = 0, best = -100000;
        for (int i = 0; i < n; i++) {
            int m = mv[i];
            Engine.make(p, m, q);
            if (Engine.inCheck(q, p.side)) continue;
            legal++;
            int sc = -search(q, depth - 1, -beta, -alpha, ply + 1);
            if (stop) return 0;
            if (sc > best) best = sc;
            if (sc > alpha) alpha = sc;
            if (alpha >= beta) {
                if ((Engine.flags(m) & Engine.F_CAP) == 0 && ply < 64) { killers[ply][1] = killers[ply][0]; killers[ply][0] = m; }
                break;
            }
        }
        if (legal == 0) return chk ? -30000 + ply : 0;
        return best;
    }

    /** Best move for the side to move at the given level, or -1 if there is none. */
    public int think(Pos root, Level lv) {
        for (int[] k : killers) { k[0] = 0; k[1] = 0; }
        nodes = 0; maxNodes = lv.nodes; stop = false;
        int[] legal = new int[256];
        int n = Engine.genLegal(root, legal);
        if (n == 0) return -1;
        if (n == 1) return legal[0];
        if (lv.blunderPct > 0 && rnd() % 100 < lv.blunderPct) return legal[rnd() % n];
        int best = legal[0], bestScore = -100000;
        Pos q = stack[1];
        stack[0].copyFrom(root);
        if (lv.noise > 0) {
            rootDepth = lv.depth;
            for (int i = 0; i < n; i++) {
                Engine.make(stack[0], legal[i], q);
                int sc = -search(q, lv.depth - 1, -100000, 100000, 1);
                if (stop) { stop = false; maxNodes = 0; sc = -evaluate(q); }
                sc += rnd() % (2 * lv.noise + 1) - lv.noise;
                if (sc > bestScore) { bestScore = sc; best = legal[i]; }
            }
            return best;
        }
        for (int d = 1; d <= lv.depth; d++) {
            rootDepth = d;
            int iterBest = best, iterScore = -100000, alpha = -100000;
            order(root, legal, n, 0);
            for (int i = 0; i < n; i++) if (legal[i] == best && i > 0) { int t = legal[i]; for (int j = i; j > 0; j--) legal[j] = legal[j - 1]; legal[0] = t; break; }
            for (int i = 0; i < n; i++) {
                Engine.make(stack[0], legal[i], q);
                int sc = -search(q, d - 1, -100000, -alpha, 1);
                if (stop) break;
                if (sc > iterScore) { iterScore = sc; iterBest = legal[i]; }
                if (sc > alpha) alpha = sc;
            }
            if (stop) break;
            best = iterBest; bestScore = iterScore;
            if (bestScore > 29000) break;
        }
        return best;
    }

    /** Evaluation in centipawns from White's point of view (shallow search, for the evaluation bar). */
    public int evalWhiteCp(Pos p, long nodeBudget) {
        if (!Engine.hasLegal(p)) return Engine.inCheck(p, p.side) ? (p.side == Engine.WHITE ? -30000 : 30000) : 0;
        nodes = 0; maxNodes = nodeBudget; stop = false; rootDepth = 2;
        for (int[] k : killers) { k[0] = 0; k[1] = 0; }
        stack[0].copyFrom(p);
        int sc = search(stack[0], 2, -100000, 100000, 0);
        if (stop) sc = evaluate(p);
        return p.side == Engine.WHITE ? sc : -sc;
    }
}
