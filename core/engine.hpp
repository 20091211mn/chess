// Chess rules engine: full legal move generation, castling, en passant, promotion,
// check/checkmate/stalemate, threefold repetition, 50-move rule, insufficient material,
// SAN/UCI notation, FEN, clocks. Header-only, C++11, no exceptions, no iostream.
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>

namespace ce {

enum { EMPTY = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6 };
enum { WHITE = 0, BLACK = 1 };
enum { F_CAP = 1, F_EP = 2, F_CASTLE = 4, F_DOUBLE = 8 };
enum { CR_WK = 1, CR_WQ = 2, CR_BK = 4, CR_BQ = 8 };

inline int mkp(int t, int c) { return t | (c << 3); }
inline int ptype(int p) { return p & 7; }
inline int pcolor(int p) { return p >> 3; }
inline int fileOf(int s) { return s & 7; }
inline int rankOf(int s) { return s >> 3; }
inline int sqOf(int f, int r) { return r * 8 + f; }

struct Move {
  uint8_t from, to, promo, flags;
  Move() : from(0), to(0), promo(0), flags(0) {}
  Move(int f, int t, int p = 0, int fl = 0) : from((uint8_t)f), to((uint8_t)t), promo((uint8_t)p), flags((uint8_t)fl) {}
  bool operator==(const Move &o) const { return from == o.from && to == o.to && promo == o.promo; }
  bool isNull() const { return from == to; }
};

struct Pos {
  int8_t b[64];
  int side;
  int castle;
  int ep;   // en-passant target square or -1
  int half; // halfmove clock
  int full; // fullmove number
};

struct Tables {
  int mb120[120];
  int mb64[64];
  Tables() {
    for (int i = 0; i < 120; i++) mb120[i] = -1;
    for (int s = 0; s < 64; s++) {
      mb64[s] = 21 + (s >> 3) * 10 + (s & 7);
      mb120[mb64[s]] = s;
    }
  }
};
inline const Tables &T() { static Tables t; return t; }

static const int KN_OFF[8] = {-21, -19, -12, -8, 8, 12, 19, 21};
static const int BI_OFF[4] = {-11, -9, 9, 11};
static const int RO_OFF[4] = {-10, -1, 1, 10};
static const int KI_OFF[8] = {-11, -10, -9, -1, 1, 9, 10, 11};

inline bool attacked(const Pos &p, int sq, int by) {
  const Tables &t = T();
  int s120 = t.mb64[sq];
  if (by == WHITE) {
    int a = t.mb120[s120 - 9], b = t.mb120[s120 - 11];
    if (a >= 0 && p.b[a] == mkp(PAWN, WHITE)) return true;
    if (b >= 0 && p.b[b] == mkp(PAWN, WHITE)) return true;
  } else {
    int a = t.mb120[s120 + 9], b = t.mb120[s120 + 11];
    if (a >= 0 && p.b[a] == mkp(PAWN, BLACK)) return true;
    if (b >= 0 && p.b[b] == mkp(PAWN, BLACK)) return true;
  }
  for (int i = 0; i < 8; i++) {
    int n = t.mb120[s120 + KN_OFF[i]];
    if (n >= 0 && p.b[n] == mkp(KNIGHT, by)) return true;
    int k = t.mb120[s120 + KI_OFF[i]];
    if (k >= 0 && p.b[k] == mkp(KING, by)) return true;
  }
  for (int i = 0; i < 4; i++) {
    int d = BI_OFF[i], x = s120 + d, q;
    while ((q = t.mb120[x]) >= 0) {
      int pc = p.b[q];
      if (pc) { if (pcolor(pc) == by && (ptype(pc) == BISHOP || ptype(pc) == QUEEN)) return true; break; }
      x += d;
    }
    d = RO_OFF[i]; x = s120 + d;
    while ((q = t.mb120[x]) >= 0) {
      int pc = p.b[q];
      if (pc) { if (pcolor(pc) == by && (ptype(pc) == ROOK || ptype(pc) == QUEEN)) return true; break; }
      x += d;
    }
  }
  return false;
}

inline int kingSq(const Pos &p, int c) {
  int k = mkp(KING, c);
  for (int i = 0; i < 64; i++) if (p.b[i] == k) return i;
  return -1;
}
inline bool inCheck(const Pos &p, int c) {
  int k = kingSq(p, c);
  return k >= 0 && attacked(p, k, c ^ 1);
}

inline void addPawnMove(Move *out, int &n, int from, int to, int flags) {
  int r = rankOf(to);
  if (r == 0 || r == 7) {
    out[n++] = Move(from, to, QUEEN, flags);
    out[n++] = Move(from, to, ROOK, flags);
    out[n++] = Move(from, to, BISHOP, flags);
    out[n++] = Move(from, to, KNIGHT, flags);
  } else out[n++] = Move(from, to, 0, flags);
}

// pseudo-legal moves (king may be left in check). out must hold >= 256 moves.
inline int genPseudo(const Pos &p, Move *out, bool capsOnly = false) {
  const Tables &t = T();
  int n = 0, us = p.side, them = us ^ 1;
  for (int s = 0; s < 64; s++) {
    int pc = p.b[s];
    if (!pc || pcolor(pc) != us) continue;
    int ty = ptype(pc), s120 = t.mb64[s];
    if (ty == PAWN) {
      int dir = us == WHITE ? 10 : -10;
      int startRank = us == WHITE ? 1 : 6;
      int f1 = t.mb120[s120 + dir];
      if (f1 >= 0 && !p.b[f1]) {
        int pr = rankOf(f1);
        bool promo = pr == 0 || pr == 7;
        if (!capsOnly || promo) addPawnMove(out, n, s, f1, 0);
        if (!capsOnly && rankOf(s) == startRank) {
          int f2 = t.mb120[s120 + 2 * dir];
          if (f2 >= 0 && !p.b[f2]) out[n++] = Move(s, f2, 0, F_DOUBLE);
        }
      }
      for (int side = -1; side <= 1; side += 2) {
        int c = t.mb120[s120 + dir + side];
        if (c < 0) continue;
        if (p.b[c] && pcolor(p.b[c]) == them) addPawnMove(out, n, s, c, F_CAP);
        else if (c == p.ep && !p.b[c]) out[n++] = Move(s, c, 0, F_CAP | F_EP);
      }
    } else if (ty == KNIGHT || ty == KING) {
      const int *off = ty == KNIGHT ? KN_OFF : KI_OFF;
      for (int i = 0; i < 8; i++) {
        int q = t.mb120[s120 + off[i]];
        if (q < 0) continue;
        if (!p.b[q]) { if (!capsOnly) out[n++] = Move(s, q); }
        else if (pcolor(p.b[q]) == them) out[n++] = Move(s, q, 0, F_CAP);
      }
      if (ty == KING && !capsOnly) {
        int homeR = us == WHITE ? 0 : 7;
        if (s == sqOf(4, homeR) && !attacked(p, s, them)) {
          int ks = us == WHITE ? CR_WK : CR_BK, qs = us == WHITE ? CR_WQ : CR_BQ;
          if ((p.castle & ks) && p.b[sqOf(7, homeR)] == mkp(ROOK, us) &&
              !p.b[sqOf(5, homeR)] && !p.b[sqOf(6, homeR)] &&
              !attacked(p, sqOf(5, homeR), them) && !attacked(p, sqOf(6, homeR), them))
            out[n++] = Move(s, sqOf(6, homeR), 0, F_CASTLE);
          if ((p.castle & qs) && p.b[sqOf(0, homeR)] == mkp(ROOK, us) &&
              !p.b[sqOf(1, homeR)] && !p.b[sqOf(2, homeR)] && !p.b[sqOf(3, homeR)] &&
              !attacked(p, sqOf(2, homeR), them) && !attacked(p, sqOf(3, homeR), them))
            out[n++] = Move(s, sqOf(2, homeR), 0, F_CASTLE);
        }
      }
    } else {
      bool diag = ty == BISHOP || ty == QUEEN, orth = ty == ROOK || ty == QUEEN;
      for (int pass = 0; pass < 2; pass++) {
        if (pass == 0 && !diag) continue;
        if (pass == 1 && !orth) continue;
        const int *off = pass == 0 ? BI_OFF : RO_OFF;
        for (int i = 0; i < 4; i++) {
          int d = off[i], x = s120 + d, q;
          while ((q = t.mb120[x]) >= 0) {
            if (!p.b[q]) { if (!capsOnly) out[n++] = Move(s, q); }
            else { if (pcolor(p.b[q]) == them) out[n++] = Move(s, q, 0, F_CAP); break; }
            x += d;
          }
        }
      }
    }
  }
  return n;
}

inline void makeMove(Pos &p, const Move &m) {
  int us = p.side, pc = p.b[m.from];
  int captured = p.b[m.to];
  p.half++;
  if (ptype(pc) == PAWN || captured) p.half = 0;
  p.b[m.to] = pc;
  p.b[m.from] = EMPTY;
  if (m.flags & F_EP) {
    int capSq = us == WHITE ? m.to - 8 : m.to + 8;
    p.b[capSq] = EMPTY;
    p.half = 0;
  }
  if (m.promo) p.b[m.to] = (int8_t)mkp(m.promo, us);
  if (m.flags & F_CASTLE) {
    int r = rankOf(m.to);
    if (fileOf(m.to) == 6) { p.b[sqOf(5, r)] = p.b[sqOf(7, r)]; p.b[sqOf(7, r)] = EMPTY; }
    else { p.b[sqOf(3, r)] = p.b[sqOf(0, r)]; p.b[sqOf(0, r)] = EMPTY; }
  }
  if (ptype(pc) == KING) p.castle &= us == WHITE ? ~(CR_WK | CR_WQ) : ~(CR_BK | CR_BQ);
  int sqs[2] = {m.from, m.to};
  for (int i = 0; i < 2; i++) {
    switch (sqs[i]) {
      case 0: p.castle &= ~CR_WQ; break;
      case 7: p.castle &= ~CR_WK; break;
      case 56: p.castle &= ~CR_BQ; break;
      case 63: p.castle &= ~CR_BK; break;
      case 4: p.castle &= ~(CR_WK | CR_WQ); break;
      case 60: p.castle &= ~(CR_BK | CR_BQ); break;
    }
  }
  p.ep = (m.flags & F_DOUBLE) ? (us == WHITE ? m.from + 8 : m.from - 8) : -1;
  if (us == BLACK) p.full++;
  p.side = us ^ 1;
}

inline int genLegal(const Pos &p, Move *out) {
  Move tmp[256];
  int n = genPseudo(p, tmp), k = 0;
  for (int i = 0; i < n; i++) {
    Pos q = p;
    makeMove(q, tmp[i]);
    if (!inCheck(q, p.side)) out[k++] = tmp[i];
  }
  return k;
}

inline bool setFen(Pos &p, const std::string &fen) {
  memset(&p, 0, sizeof(p));
  p.ep = -1; p.full = 1;
  size_t i = 0;
  int r = 7, f = 0;
  for (; i < fen.size() && fen[i] != ' '; i++) {
    char c = fen[i];
    if (c == '/') { r--; f = 0; continue; }
    if (c >= '1' && c <= '8') { f += c - '0'; continue; }
    int col = (c >= 'a') ? BLACK : WHITE;
    int ty = 0;
    switch (c | 32) {
      case 'p': ty = PAWN; break; case 'n': ty = KNIGHT; break; case 'b': ty = BISHOP; break;
      case 'r': ty = ROOK; break; case 'q': ty = QUEEN; break; case 'k': ty = KING; break;
      default: return false;
    }
    if (r < 0 || f > 7) return false;
    p.b[sqOf(f, r)] = (int8_t)mkp(ty, col);
    f++;
  }
  if (i >= fen.size()) return false;
  i++;
  p.side = (i < fen.size() && fen[i] == 'b') ? BLACK : WHITE;
  i += 2;
  p.castle = 0;
  for (; i < fen.size() && fen[i] != ' '; i++) {
    switch (fen[i]) { case 'K': p.castle |= CR_WK; break; case 'Q': p.castle |= CR_WQ; break;
                      case 'k': p.castle |= CR_BK; break; case 'q': p.castle |= CR_BQ; break; }
  }
  i++;
  if (i < fen.size() && fen[i] != '-') {
    if (i + 1 < fen.size()) p.ep = sqOf(fen[i] - 'a', fen[i + 1] - '1');
    i += 2;
  } else i++;
  if (i < fen.size()) { i++; p.half = atoi(fen.c_str() + i); }
  while (i < fen.size() && fen[i] != ' ') i++;
  if (i < fen.size()) { i++; p.full = atoi(fen.c_str() + i); if (p.full < 1) p.full = 1; }
  return kingSq(p, WHITE) >= 0 && kingSq(p, BLACK) >= 0;
}

inline char pieceChar(int pc) {
  static const char *w = " PNBRQK";
  char c = w[ptype(pc)];
  return pcolor(pc) == BLACK ? (char)(c | 32) : c;
}

inline std::string toFen(const Pos &p) {
  std::string s;
  for (int r = 7; r >= 0; r--) {
    int e = 0;
    for (int f = 0; f < 8; f++) {
      int pc = p.b[sqOf(f, r)];
      if (!pc) { e++; continue; }
      if (e) { s += (char)('0' + e); e = 0; }
      s += pieceChar(pc);
    }
    if (e) s += (char)('0' + e);
    if (r) s += '/';
  }
  s += p.side == WHITE ? " w " : " b ";
  if (!p.castle) s += '-';
  else {
    if (p.castle & CR_WK) s += 'K';
    if (p.castle & CR_WQ) s += 'Q';
    if (p.castle & CR_BK) s += 'k';
    if (p.castle & CR_BQ) s += 'q';
  }
  s += ' ';
  if (p.ep < 0) s += '-';
  else { s += (char)('a' + fileOf(p.ep)); s += (char)('1' + rankOf(p.ep)); }
  s += ' ' + std::to_string(p.half) + ' ' + std::to_string(p.full);
  return s;
}

inline void setStart(Pos &p) {
  setFen(p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

inline std::string sqName(int s) {
  std::string r; r += (char)('a' + fileOf(s)); r += (char)('1' + rankOf(s)); return r;
}
inline std::string toUci(const Move &m) {
  std::string s = sqName(m.from) + sqName(m.to);
  if (m.promo) s += (char)("  nbrq"[m.promo]);
  return s;
}
inline bool parseUci(const Pos &p, const std::string &u, Move &out) {
  if (u.size() < 4) return false;
  int f1 = u[0] - 'a', r1 = u[1] - '1', f2 = u[2] - 'a', r2 = u[3] - '1';
  if (f1 < 0 || f1 > 7 || f2 < 0 || f2 > 7 || r1 < 0 || r1 > 7 || r2 < 0 || r2 > 7) return false;
  int promo = 0;
  if (u.size() >= 5) {
    switch (u[4] | 32) { case 'q': promo = QUEEN; break; case 'r': promo = ROOK; break;
                         case 'b': promo = BISHOP; break; case 'n': promo = KNIGHT; break; default: return false; }
  }
  Move L[256];
  int n = genLegal(p, L);
  for (int i = 0; i < n; i++)
    if (L[i].from == sqOf(f1, r1) && L[i].to == sqOf(f2, r2) && L[i].promo == promo) { out = L[i]; return true; }
  return false;
}

inline bool hasLegal(const Pos &p) {
  Move L[256];
  return genLegal(p, L) > 0;
}

inline std::string toSan(const Pos &p, const Move &m) {
  std::string s;
  int pc = p.b[m.from], ty = ptype(pc);
  if (m.flags & F_CASTLE) s = fileOf(m.to) == 6 ? "O-O" : "O-O-O";
  else {
    bool cap = (m.flags & F_CAP) != 0;
    if (ty == PAWN) {
      if (cap) { s += (char)('a' + fileOf(m.from)); s += 'x'; }
    } else {
      s += " PNBRQK"[ty];
      Move L[256];
      int n = genLegal(p, L);
      bool amb = false, sameFile = false, sameRank = false;
      for (int i = 0; i < n; i++) {
        if (L[i].from == m.from || L[i].to != m.to) continue;
        if (ptype(p.b[L[i].from]) != ty) continue;
        amb = true;
        if (fileOf(L[i].from) == fileOf(m.from)) sameFile = true;
        if (rankOf(L[i].from) == rankOf(m.from)) sameRank = true;
      }
      if (amb) {
        if (!sameFile) s += (char)('a' + fileOf(m.from));
        else if (!sameRank) s += (char)('1' + rankOf(m.from));
        else s += sqName(m.from);
      }
      if (cap) s += 'x';
    }
    s += sqName(m.to);
    if (m.promo) { s += '='; s += " PNBRQK"[m.promo]; }
  }
  Pos q = p;
  makeMove(q, m);
  if (inCheck(q, q.side)) s += hasLegal(q) ? "+" : "#";
  return s;
}

inline bool insufficientMaterial(const Pos &p) {
  int minors = 0, bishopsLight = 0, bishopsDark = 0, knights = 0;
  for (int s = 0; s < 64; s++) {
    int pc = p.b[s];
    if (!pc) continue;
    int ty = ptype(pc);
    if (ty == PAWN || ty == ROOK || ty == QUEEN) return false;
    if (ty == KNIGHT) { minors++; knights++; }
    if (ty == BISHOP) { minors++; if (((fileOf(s) + rankOf(s)) & 1) == 0) bishopsDark++; else bishopsLight++; }
  }
  if (minors <= 1) return true;
  if (knights == 0 && (bishopsLight == 0 || bishopsDark == 0)) return true;
  return false;
}

inline uint64_t posKey(const Pos &p) {
  uint64_t h = 1469598103934665603ULL;
  for (int i = 0; i < 64; i++) { h ^= (uint64_t)(p.b[i] + 1); h *= 1099511628211ULL; }
  h ^= (uint64_t)p.side + 77; h *= 1099511628211ULL;
  h ^= (uint64_t)p.castle + 99; h *= 1099511628211ULL;
  int ep = -1;
  if (p.ep >= 0) {
    Move L[256];
    int n = genLegal(p, L);
    for (int i = 0; i < n; i++) if ((L[i].flags & F_EP)) { ep = p.ep; break; }
  }
  h ^= (uint64_t)(ep + 5); h *= 1099511628211ULL;
  return h;
}

inline uint64_t perft(const Pos &p, int depth) {
  Move L[256];
  int n = genLegal(p, L);
  if (depth <= 1) return (uint64_t)n;
  uint64_t total = 0;
  for (int i = 0; i < n; i++) { Pos q = p; makeMove(q, L[i]); total += perft(q, depth - 1); }
  return total;
}

enum Result { R_NONE = 0, R_WHITE = 1, R_BLACK = 2, R_DRAW = 3 };

struct Game {
  Pos pos;
  std::vector<Pos> hist;
  std::vector<Move> moves;
  std::vector<std::string> san;
  std::vector<uint64_t> keys;
  std::vector<int> times[2];
  int result;
  std::string reason;
  int64_t clk[2];
  int64_t initMs, incMs;

  Game() { reset(); }

  void reset(int64_t initTimeMs = 0, int64_t incrementMs = 0) {
    setStart(pos);
    hist.clear(); moves.clear(); san.clear(); keys.clear();
    times[0].clear(); times[1].clear();
    keys.push_back(posKey(pos));
    result = R_NONE; reason.clear();
    initMs = initTimeMs; incMs = incrementMs;
    clk[0] = clk[1] = initTimeMs;
  }
  bool loadFen(const std::string &fen) {
    if (!setFen(pos, fen)) return false;
    hist.clear(); moves.clear(); san.clear(); keys.clear();
    times[0].clear(); times[1].clear();
    keys.push_back(posKey(pos));
    result = R_NONE; reason.clear();
    updateStatus();
    return true;
  }
  bool over() const { return result != R_NONE; }
  int plies() const { return (int)moves.size(); }
  bool check() const { return inCheck(pos, pos.side); }
  int legal(Move *out) const { return genLegal(pos, out); }

  void updateStatus() {
    if (!hasLegal(pos)) {
      if (inCheck(pos, pos.side)) { result = pos.side == WHITE ? R_BLACK : R_WHITE; reason = "checkmate"; }
      else { result = R_DRAW; reason = "stalemate"; }
      return;
    }
    if (insufficientMaterial(pos)) { result = R_DRAW; reason = "insufficient material"; return; }
    if (pos.half >= 100) { result = R_DRAW; reason = "50-move rule"; return; }
    int cnt = 0;
    uint64_t k = keys.back();
    for (size_t i = 0; i < keys.size(); i++) if (keys[i] == k) cnt++;
    if (cnt >= 3) { result = R_DRAW; reason = "threefold repetition"; return; }
  }

  bool play(const Move &m) {
    if (over()) return false;
    Move L[256];
    int n = genLegal(pos, L);
    bool ok = false;
    Move real;
    for (int i = 0; i < n; i++) if (L[i] == m) { ok = true; real = L[i]; break; }
    if (!ok) return false;
    san.push_back(toSan(pos, real));
    hist.push_back(pos);
    moves.push_back(real);
    times[0].push_back((int)clk[0]); times[1].push_back((int)clk[1]);
    int mover = pos.side;
    makeMove(pos, real);
    if (initMs > 0 && moves.size() > 1) clk[mover] += incMs;
    keys.push_back(posKey(pos));
    updateStatus();
    return true;
  }
  bool playUci(const std::string &u) {
    Move m;
    if (!parseUci(pos, u, m)) return false;
    return play(m);
  }
  bool undo() {
    if (moves.empty()) return false;
    pos = hist.back(); hist.pop_back(); moves.pop_back(); san.pop_back(); keys.pop_back();
    clk[0] = times[0].back(); clk[1] = times[1].back();
    times[0].pop_back(); times[1].pop_back();
    result = R_NONE; reason.clear();
    return true;
  }
  void resign(int side) {
    if (over()) return;
    result = side == WHITE ? R_BLACK : R_WHITE; reason = "resignation";
  }
  // advance the running clock; returns true if a flag fell
  bool tick(int64_t ms) {
    if (initMs <= 0 || over() || moves.empty()) return false;
    int s = pos.side;
    clk[s] -= ms;
    if (clk[s] <= 0) {
      clk[s] = 0;
      bool oppHasMaterial = false;
      for (int i = 0; i < 64; i++) {
        int pc = pos.b[i];
        if (pc && pcolor(pc) == (s ^ 1) && ptype(pc) != KING) oppHasMaterial = true;
      }
      if (!oppHasMaterial) { result = R_DRAW; reason = "timeout vs insufficient material"; }
      else { result = s == WHITE ? R_BLACK : R_WHITE; reason = "timeout"; }
      return true;
    }
    return false;
  }
  std::string resultText() const {
    switch (result) {
      case R_WHITE: return "White wins";
      case R_BLACK: return "Black wins";
      case R_DRAW: return "Draw";
    }
    return "";
  }
};

} // namespace ce
