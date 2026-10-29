// Chess AI: iterative-deepening negamax, alpha-beta, quiescence, MVV-LVA, killers,
// piece-square evaluation. Deterministic node budget (no clocks needed).
#pragma once
#include "engine.hpp"

namespace ce {

struct Level { const char *name; int depth; long nodes; int noise; int blunderPct; };
static const Level LEVELS[5] = {
  {"Beginner", 1, 2000,    350, 30},
  {"Easy",     2, 20000,   150, 8},
  {"Medium",   3, 120000,  40,  0},
  {"Hard",     4, 600000,  0,   0},
  {"Expert",   6, 2500000, 0,   0},
};

static const Level LEVEL_HINT = {"Hint", 4, 120000, 0, 0};

namespace aidata {
static const int VAL[7] = {0, 100, 320, 330, 500, 900, 0};
static const int PST[7][64] = {
  {0},
  {0,0,0,0,0,0,0,0, 50,50,50,50,50,50,50,50, 10,10,20,30,30,20,10,10, 5,5,10,25,25,10,5,5,
   0,0,0,20,20,0,0,0, 5,-5,-10,0,0,-10,-5,5, 5,10,10,-20,-20,10,10,5, 0,0,0,0,0,0,0,0},
  {-50,-40,-30,-30,-30,-30,-40,-50, -40,-20,0,0,0,0,-20,-40, -30,0,10,15,15,10,0,-30, -30,5,15,20,20,15,5,-30,
   -30,0,15,20,20,15,0,-30, -30,5,10,15,15,10,5,-30, -40,-20,0,5,5,0,-20,-40, -50,-40,-30,-30,-30,-30,-40,-50},
  {-20,-10,-10,-10,-10,-10,-10,-20, -10,0,0,0,0,0,0,-10, -10,0,5,10,10,5,0,-10, -10,5,5,10,10,5,5,-10,
   -10,0,10,10,10,10,0,-10, -10,10,10,10,10,10,10,-10, -10,5,0,0,0,0,5,-10, -20,-10,-10,-10,-10,-10,-10,-20},
  {0,0,0,0,0,0,0,0, 5,10,10,10,10,10,10,5, -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5,
   -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5, 0,0,0,5,5,0,0,0},
  {-20,-10,-10,-5,-5,-10,-10,-20, -10,0,0,0,0,0,0,-10, -10,0,5,5,5,5,0,-10, -5,0,5,5,5,5,0,-5,
   0,0,5,5,5,5,0,-5, -10,5,5,5,5,5,0,-10, -10,0,5,0,0,0,0,-10, -20,-10,-10,-5,-5,-10,-10,-20},
  {-30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10, 20,20,0,0,0,0,20,20, 20,30,10,0,0,10,30,20},
};
static const int KING_END[64] = {
  -50,-40,-30,-20,-20,-30,-40,-50, -30,-20,-10,0,0,-10,-20,-30, -30,-10,20,30,30,20,-10,-30, -30,-10,30,40,40,30,-10,-30,
  -30,-10,30,40,40,30,-10,-30, -30,-10,20,30,30,20,-10,-30, -30,-30,0,0,0,0,-30,-30, -50,-30,-30,-30,-30,-30,-30,-50};
}

struct Searcher {
  long nodes, maxNodes;
  bool stop;
  Move killers[64][2];
  int rootDepth;
  uint32_t rng;

  Searcher() : nodes(0), maxNodes(0), stop(false), rootDepth(0), rng(2463534242u) {}
  uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

  static int evaluate(const Pos &p) {
    int score = 0, nonPawn = 0;
    for (int s = 0; s < 64; s++) {
      int pc = p.b[s];
      if (!pc) continue;
      int ty = ptype(pc);
      if (ty != PAWN && ty != KING) nonPawn += aidata::VAL[ty];
    }
    bool endgame = nonPawn < 1400;
    for (int s = 0; s < 64; s++) {
      int pc = p.b[s];
      if (!pc) continue;
      int ty = ptype(pc), c = pcolor(pc);
      int idx = c == WHITE ? (s ^ 56) : s;
      int v = aidata::VAL[ty] + ((ty == KING && endgame) ? aidata::KING_END[idx] : aidata::PST[ty][idx]);
      score += c == WHITE ? v : -v;
    }
    return p.side == WHITE ? score : -score;
  }

  int scoreMove(const Pos &p, const Move &m, int ply) {
    int sc = 0;
    if (m.flags & F_CAP) {
      int victim = (m.flags & F_EP) ? PAWN : ptype(p.b[m.to]);
      sc = 10000 + aidata::VAL[victim] * 10 - aidata::VAL[ptype(p.b[m.from])] / 10;
    } else if (ply < 64 && (m == killers[ply][0])) sc = 9000;
    else if (ply < 64 && (m == killers[ply][1])) sc = 8000;
    if (m.promo) sc += 9500 + aidata::VAL[m.promo];
    return sc;
  }

  void order(const Pos &p, Move *mv, int n, int ply) {
    int sc[256];
    for (int i = 0; i < n; i++) sc[i] = scoreMove(p, mv[i], ply);
    for (int i = 1; i < n; i++) {
      int s = sc[i]; Move m = mv[i]; int j = i - 1;
      while (j >= 0 && sc[j] < s) { sc[j + 1] = sc[j]; mv[j + 1] = mv[j]; j--; }
      sc[j + 1] = s; mv[j + 1] = m;
    }
  }

  int quiesce(const Pos &p, int alpha, int beta, int ply) {
    nodes++;
    if (maxNodes && nodes > maxNodes) { stop = true; return 0; }
    int stand = evaluate(p);
    if (ply > 40) return stand;
    if (stand >= beta) return stand;
    if (stand > alpha) alpha = stand;
    Move mv[256];
    int n = genPseudo(p, mv, true);
    order(p, mv, n, 63);
    for (int i = 0; i < n; i++) {
      Pos q = p;
      makeMove(q, mv[i]);
      if (inCheck(q, p.side)) continue;
      int sc = -quiesce(q, -beta, -alpha, ply + 1);
      if (stop) return 0;
      if (sc >= beta) return sc;
      if (sc > alpha) alpha = sc;
    }
    return alpha;
  }

  int search(const Pos &p, int depth, int alpha, int beta, int ply) {
    nodes++;
    if (maxNodes && nodes > maxNodes) { stop = true; return 0; }
    bool chk = inCheck(p, p.side);
    if (chk && ply < rootDepth + 8) depth++;
    if (depth <= 0) return quiesce(p, alpha, beta, ply);
    if (p.half >= 100) return 0;
    Move mv[256];
    int n = genPseudo(p, mv);
    order(p, mv, n, ply);
    int legal = 0, best = -100000;
    for (int i = 0; i < n; i++) {
      Pos q = p;
      makeMove(q, mv[i]);
      if (inCheck(q, p.side)) continue;
      legal++;
      int sc = -search(q, depth - 1, -beta, -alpha, ply + 1);
      if (stop) return 0;
      if (sc > best) best = sc;
      if (sc > alpha) alpha = sc;
      if (alpha >= beta) {
        if (!(mv[i].flags & F_CAP) && ply < 64) { killers[ply][1] = killers[ply][0]; killers[ply][0] = mv[i]; }
        break;
      }
    }
    if (!legal) return chk ? -30000 + ply : 0;
    return best;
  }

  // returns best move; score (side-to-move perspective) in *scoreOut
  Move think(const Pos &p, const Level &lv, int *scoreOut = 0) {
    memset(killers, 0, sizeof(killers));
    nodes = 0; maxNodes = lv.nodes; stop = false;
    Move legal[256];
    int n = genLegal(p, legal);
    if (n == 0) return Move();
    if (n == 1) { if (scoreOut) *scoreOut = 0; return legal[0]; }

    // beginners sometimes just play a random move
    if (lv.blunderPct && (int)(rnd() % 100) < lv.blunderPct) return legal[rnd() % n];

    Move best = legal[0];
    int bestScore = -100000;
    if (lv.noise > 0) {
      // score every root move, add noise, pick the best
      rootDepth = lv.depth;
      for (int i = 0; i < n; i++) {
        Pos q = p; makeMove(q, legal[i]);
        int sc = -search(q, lv.depth - 1, -100000, 100000, 1);
        if (stop) { stop = false; maxNodes = 0; sc = -evaluate(q); }
        sc += (int)(rnd() % (2 * lv.noise + 1)) - lv.noise;
        if (sc > bestScore) { bestScore = sc; best = legal[i]; }
      }
      if (scoreOut) *scoreOut = bestScore;
      return best;
    }
    for (int d = 1; d <= lv.depth; d++) {
      rootDepth = d;
      Move iterBest = best; int iterScore = -100000, alpha = -100000;
      order(p, legal, n, 0);
      // search previous best first
      for (int i = 0; i < n; i++) if (legal[i] == best && i > 0) { Move t = legal[i]; for (int j = i; j > 0; j--) legal[j] = legal[j - 1]; legal[0] = t; break; }
      for (int i = 0; i < n; i++) {
        Pos q = p; makeMove(q, legal[i]);
        int sc = -search(q, d - 1, -100000, -alpha, 1);
        if (stop) break;
        if (sc > iterScore) { iterScore = sc; iterBest = legal[i]; }
        if (sc > alpha) alpha = sc;
      }
      if (stop) break;
      best = iterBest; bestScore = iterScore;
      if (bestScore > 29000) break; // forced mate found
    }
    if (scoreOut) *scoreOut = bestScore;
    return best;
  }
};

// centipawn evaluation from White's point of view (shallow search, for the eval bar). Mates -> +-30000.
inline int evalWhiteCp(const Pos &p, long nodes = 40000) {
  if (!hasLegal(p)) return inCheck(p, p.side) ? (p.side == WHITE ? -30000 : 30000) : 0;
  Searcher s; s.maxNodes = nodes; s.rootDepth = 2;
  int sc = s.search(p, 2, -100000, 100000, 0);
  if (s.stop) sc = Searcher::evaluate(p);
  return p.side == WHITE ? sc : -sc;
}

inline Move aiMoveWith(const Pos &p, const Level &lv, int *score = 0) {
  static Searcher s;
  s.rng ^= (uint32_t)(p.full * 2654435761u) ^ (uint32_t)p.half;
  return s.think(p, lv, score);
}

inline Move aiMove(const Pos &p, int level, int *score = 0) {
  static Searcher s;
  if (level < 0) level = 0;
  if (level > 4) level = 4;
  s.rng ^= (uint32_t)(p.full * 2654435761u) ^ (uint32_t)p.half;
  return s.think(p, LEVELS[level], score);
}

} // namespace ce
