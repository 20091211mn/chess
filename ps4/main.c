#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#  else
#    include <SDL.h>
#  endif
#else
#  include <SDL2/SDL.h>
#endif
#include "chess_engine.h"

#define W 1920
#define H 1080
#define CELL 100
#define OX ((W - 8*CELL) / 2)
#define OY ((H - 8*CELL) / 2)

static const char *GLY[6][5] = {
  {"01110","01010","01110","00100","01110"}, /* P */
  {"10001","11001","10101","10011","10001"}, /* N */
  {"11110","10001","11110","10001","11110"}, /* B */
  {"11110","10001","11110","10100","10010"}, /* R */
  {"01110","10001","10101","10010","01101"}, /* Q */
  {"10001","10010","11100","10010","10001"}  /* K */
};

static int glyph_index(char p) {
  switch (p | 32) {
    case 'p': return 0; case 'n': return 1; case 'b': return 2;
    case 'r': return 3; case 'q': return 4; default: return 5;
  }
}

static void rect(SDL_Renderer *rd, int x, int y, int w, int h,
                 int r, int g, int b) {
  SDL_Rect q; q.x = x; q.y = y; q.w = w; q.h = h;
  SDL_SetRenderDrawColor(rd, r, g, b, 255);
  SDL_RenderFillRect(rd, &q);
}

static void frame(SDL_Renderer *rd, int x, int y, int w, int h, int t,
                  int r, int g, int b) {
  rect(rd, x, y, w, t, r, g, b);
  rect(rd, x, y + h - t, w, t, r, g, b);
  rect(rd, x, y, t, h, r, g, b);
  rect(rd, x + w - t, y, t, h, r, g, b);
}

static void piece(SDL_Renderer *rd, int x, int y, char p) {
  int white = ce_white(p), i, j, s = 10;
  int gi = glyph_index(p);
  int fr = white ? 235 : 40, fg = white ? 235 : 40, fb = white ? 235 : 40;
  int lr = white ? 20 : 240, lg = white ? 20 : 240, lb = white ? 20 : 240;
  rect(rd, x + 12, y + 12, CELL - 24, CELL - 24, fr, fg, fb);
  frame(rd, x + 12, y + 12, CELL - 24, CELL - 24, 3, lr, lg, lb);
  for (i = 0; i < 5; i++)
    for (j = 0; j < 5; j++)
      if (GLY[gi][i][j] == '1')
        rect(rd, x + 25 + j * s, y + 25 + i * s, s, s, lr, lg, lb);
}

int main(int argc, char *argv[]) {
  SDL_Window *win; SDL_Renderer *rd; SDL_Event e;
  int cx = 4, cy = 6, sr = -1, sc = -1, run = 1;
  int axl = 0, axr = 0, axu = 0, axd = 0;
  (void)argc; (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) return 1;
  if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);
  win = SDL_CreateWindow("chess", 0, 0, W, H, 0);
  rd = SDL_CreateRenderer(win, -1, 0);
  ce_reset();

  while (run) {
    int dx = 0, dy = 0, ok = 0, cancel = 0;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) run = 0;
      if (e.type == SDL_JOYHATMOTION) {
        if (e.jhat.value & SDL_HAT_LEFT) dx = -1;
        if (e.jhat.value & SDL_HAT_RIGHT) dx = 1;
        if (e.jhat.value & SDL_HAT_UP) dy = -1;
        if (e.jhat.value & SDL_HAT_DOWN) dy = 1;
      }
      if (e.type == SDL_JOYAXISMOTION && e.jaxis.axis < 2) {
        int v = e.jaxis.value;
        if (e.jaxis.axis == 0) {
          if (v < -16000 && !axl) { dx = -1; axl = 1; }
          if (v > 16000 && !axr) { dx = 1; axr = 1; }
          if (v > -8000) axl = 0;
          if (v < 8000) axr = 0;
        } else {
          if (v < -16000 && !axu) { dy = -1; axu = 1; }
          if (v > 16000 && !axd) { dy = 1; axd = 1; }
          if (v > -8000) axu = 0;
          if (v < 8000) axd = 0;
        }
      }
      if (e.type == SDL_JOYBUTTONDOWN) {
        if (e.jbutton.button == 0) ok = 1;      /* Cross (mapping may vary) */
        else if (e.jbutton.button == 1) cancel = 1; /* Circle */
      }
    }
    cx += dx; cy += dy;
    if (cx < 0) cx = 0; if (cx > 7) cx = 7;
    if (cy < 0) cy = 0; if (cy > 7) cy = 7;

    if (ok) {
      if (g_winner) { ce_reset(); sr = sc = -1; }
      else if (sr < 0) {
        char p = g_board[cy][cx];
        if (p != '.' && ce_white(p) == g_white_turn) { sr = cy; sc = cx; }
      } else {
        if (!ce_move(sr, sc, cy, cx)) {
          char p = g_board[cy][cx];
          if (p != '.' && ce_white(p) == g_white_turn) { sr = cy; sc = cx; }
          else { sr = sc = -1; }
        } else { sr = sc = -1; }
      }
    }
    if (cancel) { sr = sc = -1; }

    rect(rd, 0, 0, W, H, 30, 30, 36);
    {
      int r, c;
      for (r = 0; r < 8; r++) for (c = 0; c < 8; c++) {
        int light = (r + c) % 2 == 0;
        rect(rd, OX + c*CELL, OY + r*CELL, CELL, CELL,
             light ? 238 : 118, light ? 238 : 150, light ? 210 : 86);
        if (g_board[r][c] != '.')
          piece(rd, OX + c*CELL, OY + r*CELL, g_board[r][c]);
      }
    }
    if (sr >= 0) frame(rd, OX + sc*CELL, OY + sr*CELL, CELL, CELL, 6, 60, 200, 80);
    frame(rd, OX + cx*CELL, OY + cy*CELL, CELL, CELL, 6, 250, 210, 0);
    /* turn indicator / winner banner */
    if (g_winner)
      rect(rd, OX, OY - 60, 8*CELL, 40, g_winner == 1 ? 235 : 40,
           g_winner == 1 ? 235 : 40, g_winner == 1 ? 235 : 40);
    else
      rect(rd, OX, OY - 40, 60, 20, g_white_turn ? 235 : 40,
           g_white_turn ? 235 : 40, g_white_turn ? 235 : 40);
    SDL_RenderPresent(rd);
    SDL_Delay(16);
  }
  SDL_Quit();
  return 0;
}
