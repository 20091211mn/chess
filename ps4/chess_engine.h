#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H
/* Valid in both C and C++ */
static char g_board[8][8];
static int g_white_turn = 1;
static int g_winner = 0; /* 0 none, 1 white, 2 black */

static int ce_white(char p) { return p >= 'A' && p <= 'Z'; }
static int ce_abs(int x) { return x < 0 ? -x : x; }

static void ce_reset(void) {
  const char *s[8] = {"rnbqkbnr","pppppppp","........","........",
                      "........","........","PPPPPPPP","RNBQKBNR"};
  int r, c;
  for (r = 0; r < 8; r++) for (c = 0; c < 8; c++) g_board[r][c] = s[r][c];
  g_white_turn = 1; g_winner = 0;
}

static int ce_clear(int r1, int c1, int r2, int c2) {
  int dr = (r2 > r1) - (r2 < r1), dc = (c2 > c1) - (c2 < c1);
  r1 += dr; c1 += dc;
  while (r1 != r2 || c1 != c2) {
    if (g_board[r1][c1] != '.') return 0;
    r1 += dr; c1 += dc;
  }
  return 1;
}

static int ce_valid(int r1, int c1, int r2, int c2) {
  char p, t; int dr, dc, ar, ac, d, st;
  if (r1<0||r1>7||c1<0||c1>7||r2<0||r2>7||c2<0||c2>7) return 0;
  p = g_board[r1][c1]; t = g_board[r2][c2];
  if (p == '.' || ce_white(p) != g_white_turn) return 0;
  if (t != '.' && ce_white(t) == ce_white(p)) return 0;
  dr = r2 - r1; dc = c2 - c1; ar = ce_abs(dr); ac = ce_abs(dc);
  if (!ar && !ac) return 0;
  switch (p | 32) {
    case 'p':
      d = ce_white(p) ? -1 : 1; st = ce_white(p) ? 6 : 1;
      if (dc == 0 && t == '.')
        return dr == d || (dr == 2*d && r1 == st && g_board[r1+d][c1] == '.');
      return ac == 1 && dr == d && t != '.';
    case 'n': return ar * ac == 2;
    case 'b': return ar == ac && ce_clear(r1, c1, r2, c2);
    case 'r': return (dr == 0 || dc == 0) && ce_clear(r1, c1, r2, c2);
    case 'q': return (ar == ac || dr == 0 || dc == 0) && ce_clear(r1, c1, r2, c2);
    case 'k': return ar <= 1 && ac <= 1;
  }
  return 0;
}

static int ce_move(int r1, int c1, int r2, int c2) {
  char p, t;
  if (g_winner || !ce_valid(r1, c1, r2, c2)) return 0;
  p = g_board[r1][c1]; t = g_board[r2][c2];
  g_board[r2][c2] = p; g_board[r1][c1] = '.';
  if (p == 'P' && r2 == 0) g_board[r2][c2] = 'Q';
  if (p == 'p' && r2 == 7) g_board[r2][c2] = 'q';
  if (t == 'k') g_winner = 1;
  if (t == 'K') g_winner = 2;
  g_white_turn = !g_white_turn;
  return 1;
}
#endif
