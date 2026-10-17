#include <stdio.h>
#include <stdlib.h>
#include "../core/engine.hpp"
using namespace ce;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void perftTest(const char *name, const char *fen, int d, uint64_t expect) {
  Pos p; setFen(p, fen);
  uint64_t got = perft(p, d);
  printf("perft %-8s d%d = %llu %s\n", name, d, (unsigned long long)got, got == expect ? "OK" : "WRONG");
  if (got != expect) fails++;
}

int main() {
  const char *st = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  perftTest("start", st, 1, 20); perftTest("start", st, 3, 8902); perftTest("start", st, 5, 4865609);
  const char *kiwi = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
  perftTest("kiwipete", kiwi, 1, 48); perftTest("kiwipete", kiwi, 2, 2039);
  perftTest("kiwipete", kiwi, 3, 97862); perftTest("kiwipete", kiwi, 4, 4085603);
  const char *p3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
  perftTest("pos3", p3, 1, 14); perftTest("pos3", p3, 3, 2812); perftTest("pos3", p3, 5, 674624);
  const char *p4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
  perftTest("pos4", p4, 1, 6); perftTest("pos4", p4, 3, 9467); perftTest("pos4", p4, 4, 422333);
  const char *p5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
  perftTest("pos5", p5, 1, 44); perftTest("pos5", p5, 3, 62379); perftTest("pos5", p5, 4, 2103487);
  const char *p6 = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
  perftTest("pos6", p6, 1, 46); perftTest("pos6", p6, 3, 89890); perftTest("pos6", p6, 4, 3894594);

  { Pos p; setStart(p); CHECK(toFen(p) == st);
    Pos q; setFen(q, kiwi); CHECK(toFen(q) == kiwi); }

  { Game g; const char *mv[] = {"e2e4","e7e5","f1c4","b8c6","d1h5","g8f6","h5f7"};
    for (int i = 0; i < 7; i++) CHECK(g.playUci(mv[i]));
    CHECK(g.over() && g.result == R_WHITE && g.reason == "checkmate");
    CHECK(g.san.back() == "Qxf7#"); CHECK(g.san[0] == "e4"); CHECK(g.san[2] == "Bc4");
    CHECK(!g.playUci("a7a6"));
    CHECK(g.undo()); CHECK(!g.over()); CHECK(g.pos.side == WHITE);
    CHECK(g.playUci("h5f7")); }

  { Game g; CHECK(!g.playUci("e7e5")); CHECK(!g.playUci("e2e5")); CHECK(!g.playUci("e1e2")); CHECK(g.playUci("e2e4")); CHECK(!g.playUci("e4e5")); }

  { Game g; g.loadFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    CHECK(g.playUci("e1g1")); CHECK(g.pos.b[5] == mkp(ROOK, WHITE) && g.pos.b[6] == mkp(KING, WHITE));
    CHECK(g.playUci("e8c8")); CHECK(g.pos.b[59] == mkp(ROOK, BLACK) && g.pos.b[58] == mkp(KING, BLACK));
    CHECK(g.san[0] == "O-O"); CHECK(g.san[1] == "O-O-O+" || g.san[1] == "O-O-O"); }
  { Game g; g.loadFen("r3k2r/8/8/8/8/5r2/8/R3K2R w KQkq - 0 1");
    CHECK(!g.playUci("e1g1")); CHECK(g.playUci("e1c1")); }
  { Game g; g.loadFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"); CHECK(g.playUci("h1h2")); CHECK(g.playUci("a8a7")); CHECK(g.playUci("h2h1"));
    CHECK(g.playUci("a7a8")); CHECK(!g.playUci("e1g1")); CHECK(g.playUci("e1c1")); }

  { Game g; g.loadFen("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3");
    CHECK(g.playUci("d4e3")); CHECK(g.pos.b[sqOf(4,3)] == EMPTY); CHECK(g.san[0] == "dxe3"); }
  { Game g; CHECK(g.playUci("e2e4")); CHECK(g.playUci("a7a6")); CHECK(g.playUci("e4e5")); CHECK(g.playUci("d7d5"));
    CHECK(g.playUci("e5d6")); CHECK(g.pos.b[sqOf(3,4)] == EMPTY); }
  { Game g; CHECK(g.playUci("e2e4")); CHECK(g.playUci("a7a6")); CHECK(g.playUci("e4e5")); CHECK(g.playUci("d7d5"));
    CHECK(g.playUci("b1c3")); CHECK(g.playUci("a6a5")); CHECK(!g.playUci("e5d6")); }

  { Game g; g.loadFen("8/P6k/8/8/8/8/8/K7 w - - 0 1");
    CHECK(!g.playUci("a7a8")); CHECK(g.playUci("a7a8n")); CHECK(g.pos.b[56] == mkp(KNIGHT, WHITE)); CHECK(g.san[0] == "a8=N"); g.undo();
    CHECK(g.playUci("a7a8q")); CHECK(g.san[0] == "a8=Q"); }
  { Game g; g.loadFen("7k/P7/8/8/8/8/8/K7 w - - 0 1"); CHECK(g.playUci("a7a8q")); CHECK(g.san[0] == "a8=Q+"); }

  { Game g; g.loadFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"); CHECK(g.over() && g.result == R_DRAW && g.reason == "stalemate"); }
  { Game g; g.loadFen("8/8/8/4k3/8/8/3KB3/8 w - - 0 1"); CHECK(g.over() && g.reason == "insufficient material"); }
  { Game g; g.loadFen("8/8/8/4k3/8/8/3KN3/8 w - - 0 1"); CHECK(g.over()); }
  { Game g; g.loadFen("8/8/8/4k3/8/8/3KP3/8 w - - 0 1"); CHECK(!g.over()); }
  { Game g; g.loadFen("8/8/2b5/4k3/8/8/3KB3/8 w - - 0 1"); CHECK(g.over()); }
  { Game g; g.loadFen("8/8/3b4/4k3/8/8/3KB3/8 w - - 0 1"); CHECK(!g.over()); }
  { Game g; g.loadFen("4k3/8/8/8/8/8/R7/4K3 w - - 99 80"); CHECK(!g.over()); CHECK(g.playUci("a2a3")); CHECK(g.over() && g.reason == "50-move rule"); }
  { Game g; const char *s[] = {"g1f3","g8f6","f3g1","f6g8","g1f3","g8f6","f3g1","f6g8"};
    for (int i = 0; i < 8; i++) { CHECK(!g.over()); CHECK(g.playUci(s[i])); }
    CHECK(g.over() && g.reason == "threefold repetition"); }

  { Game g; g.loadFen("4k3/8/8/8/8/8/3K4/R6R w - - 0 1"); Move m; CHECK(parseUci(g.pos, "a1d1", m)); CHECK(toSan(g.pos, m) == "Rad1");
    CHECK(parseUci(g.pos, "h1d1", m)); CHECK(toSan(g.pos, m) == "Rhd1"); }
  { Game g; g.loadFen("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1"); Move m; CHECK(parseUci(g.pos, "a1a3", m)); CHECK(toSan(g.pos, m) == "R1a3"); }

  { Game g; g.loadFen("4k3/4r3/8/8/8/8/4N3/4K3 w - - 0 1"); CHECK(!g.playUci("e2c3")); CHECK(g.playUci("e1d1")); }
  { Game g; g.loadFen("4k3/8/8/8/8/8/8/r3K3 w - - 0 1"); CHECK(g.check()); Move L[256]; int n = g.legal(L);
    for (int i = 0; i < n; i++) CHECK(L[i].from == 4); }

  { Game g; g.reset(60000, 2000); CHECK(!g.tick(5000)); CHECK(g.clk[0] == 60000);
    CHECK(g.playUci("e2e4")); CHECK(g.clk[0] == 60000); CHECK(!g.tick(10000)); CHECK(g.clk[1] == 50000);
    CHECK(g.playUci("e7e5")); CHECK(g.clk[1] == 52000); CHECK(g.tick(70000)); CHECK(g.over() && g.result == R_BLACK && g.reason == "timeout"); }
  { Game g; g.reset(60000, 0); g.playUci("e2e4"); g.playUci("e7e5"); g.tick(1000); g.undo(); CHECK(g.clk[0] == 60000 && g.clk[1] == 60000); }

  { srand(12345); int games = 300, totalPlies = 0;
    for (int gi = 0; gi < games; gi++) {
      Game g;
      while (!g.over() && g.plies() < 400) {
        Move L[256]; int n = g.legal(L); CHECK(n > 0);
        Move m = L[rand() % n];
        CHECK(g.play(m));
        CHECK(kingSq(g.pos, WHITE) >= 0 && kingSq(g.pos, BLACK) >= 0);
        CHECK(!inCheck(g.pos, g.pos.side ^ 1));
      }
      totalPlies += g.plies();
      while (g.undo()) {}
      CHECK(toFen(g.pos) == st);
    }
    printf("random playouts: %d games, %d plies\n", games, totalPlies); }

  printf(fails ? "\n%d FAILURES\n" : "\nALL ENGINE TESTS PASSED\n", fails);
  return fails ? 1 : 0;
}
