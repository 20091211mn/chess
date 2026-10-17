// PS4 Chess - SDL2 front end (OpenOrbis). Modes: local 2 players, vs robot, LAN with phone.
#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#  else
#    include <SDL.h>
#  endif
#else
#  include <SDL2/SDL.h>
#endif
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "engine.hpp"
#include "ai.hpp"
#include "font.hpp"
#include "pieces.hpp"
#if !defined(ENABLE_LAN) && defined(__has_include)
#  if __has_include("lan_enabled.h")
#    define ENABLE_LAN 1
#  endif
#endif
#ifdef ENABLE_LAN
#  include "lan.hpp"
#  include "web_html.h"
#endif

using namespace ce;

#define SCR_W 1920
#define SCR_H 1080
#define CELL 104
#define BX ((SCR_W - 8 * CELL) / 2)
#define BY 124
#define LAN_PORT 8080

struct Col { Uint8 r, g, b, a; };
static Col C(int r, int g, int b, int a = 255) { Col c = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a}; return c; }

// ===================================================================== graphics
static SDL_Renderer *R = 0;
static SDL_Texture *texFont = 0, *texPiece[12], *texDisc = 0, *texRing = 0;
static bool texOK = false;

static SDL_Texture *makeTex(int w, int h, const std::vector<uint32_t> &px) {
  SDL_Texture *t = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (!t) return 0;
  SDL_UpdateTexture(t, 0, px.data(), w * 4);
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  return t;
}

static void initGfx() {
  SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < 12; i++) texPiece[i] = 0;
  // font atlas: 95 glyphs of 5x8
  std::vector<uint32_t> fp(95 * 5 * 8, 0);
  for (int g = 0; g < 95; g++) for (int c = 0; c < 5; c++) for (int r = 0; r < 8; r++)
    if ((FONT5x7[g][c] >> r) & 1) fp[(size_t)r * 95 * 5 + g * 5 + c] = 0xFFFFFFFFu;
  texFont = makeTex(95 * 5, 8, fp);
  bool ok = texFont != 0;
  for (int c = 0; c < 2; c++) for (int t = 1; t <= 6; t++) {
    std::vector<uint32_t> px; gfx::renderPiece(px, CELL, t, c);
    texPiece[c * 6 + t - 1] = makeTex(CELL, CELL, px);
    if (!texPiece[c * 6 + t - 1]) ok = false;
  }
  // disc and ring (white, tinted via color mod)
  const int N = 64, SS = 4;
  std::vector<uint32_t> d(N * N, 0), rg(N * N, 0);
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int ad = 0, ar = 0;
    for (int sy = 0; sy < SS; sy++) for (int sx = 0; sx < SS; sx++) {
      float fx = x + (sx + 0.5f) / SS - N / 2.f, fy = y + (sy + 0.5f) / SS - N / 2.f, dist = sqrtf(fx * fx + fy * fy);
      if (dist <= N / 2.f - 1) ad++;
      if (dist <= N / 2.f - 1 && dist >= N / 2.f - 8) ar++;
    }
    d[y * N + x] = ((uint32_t)(ad * 255 / (SS * SS)) << 24) | 0xFFFFFF;
    rg[y * N + x] = ((uint32_t)(ar * 255 / (SS * SS)) << 24) | 0xFFFFFF;
  }
  texDisc = makeTex(N, N, d); texRing = makeTex(N, N, rg);
  texOK = ok && texDisc && texRing;
}

static void fillRect(int x, int y, int w, int h, Col c) {
  SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a);
  SDL_Rect q; q.x = x; q.y = y; q.w = w; q.h = h;
  SDL_RenderFillRect(R, &q);
}
static void frameRect(int x, int y, int w, int h, int t, Col c) {
  fillRect(x, y, w, t, c); fillRect(x, y + h - t, w, t, c); fillRect(x, y, t, h, c); fillRect(x + w - t, y, t, h, c);
}
static int textW(const char *s, int sc) { int n = (int)strlen(s); return n ? n * 6 * sc - sc : 0; }
static void drawText(int x, int y, const char *s, int sc, Col c) {
  for (int i = 0; s[i]; i++) {
    int g = (unsigned char)s[i] - 32;
    if (g < 0 || g > 94) g = '?' - 32;
    if (texOK) {
      SDL_Rect src; src.x = g * 5; src.y = 0; src.w = 5; src.h = 8;
      SDL_Rect dst; dst.x = x + i * 6 * sc; dst.y = y; dst.w = 5 * sc; dst.h = 8 * sc;
      SDL_SetTextureColorMod(texFont, c.r, c.g, c.b); SDL_SetTextureAlphaMod(texFont, c.a);
      SDL_RenderCopy(R, texFont, &src, &dst);
    } else {
      for (int cx = 0; cx < 5; cx++) for (int r = 0; r < 8; r++)
        if ((FONT5x7[g][cx] >> r) & 1) fillRect(x + i * 6 * sc + cx * sc, y + r * sc, sc, sc, c);
    }
  }
}
static void textC(int cx, int y, const char *s, int sc, Col c) { drawText(cx - textW(s, sc) / 2, y, s, sc, c); }
static void textR(int rx, int y, const char *s, int sc, Col c) { drawText(rx - textW(s, sc), y, s, sc, c); }

static void drawDisc(int cx, int cy, int d, Col c, bool ring) {
  SDL_Texture *t = ring ? texRing : texDisc;
  if (texOK) {
    SDL_Rect dst; dst.x = cx - d / 2; dst.y = cy - d / 2; dst.w = d; dst.h = d;
    SDL_SetTextureColorMod(t, c.r, c.g, c.b); SDL_SetTextureAlphaMod(t, c.a);
    SDL_RenderCopy(R, t, 0, &dst);
  } else if (ring) frameRect(cx - d / 2, cy - d / 2, d, d, 4, c);
  else fillRect(cx - d / 4, cy - d / 4, d / 2, d / 2, c);
}

// piece code (ce::mkp) at pixel x,y with size sz
static void drawPieceAt(int pc, int x, int y, int sz, int alpha = 255) {
  int ty = ptype(pc), col = pcolor(pc);
  if (texOK) {
    SDL_Texture *t = texPiece[col * 6 + ty - 1];
    SDL_Rect dst; dst.x = x; dst.y = y; dst.w = sz; dst.h = sz;
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    SDL_RenderCopy(R, t, 0, &dst);
  } else {
    fillRect(x + sz / 8, y + sz / 8, sz * 3 / 4, sz * 3 / 4, col ? C(60, 60, 64, alpha) : C(245, 245, 245, alpha));
    char s[2] = {pieceChar(pc), 0};
    textC(x + sz / 2, y + sz / 2 - 12, s, 3, col ? C(240, 240, 240, alpha) : C(20, 20, 20, alpha));
  }
}

// ===================================================================== input
static const int BTN_SEL[] = {0}, BTN_BACK[] = {1}, BTN_FLIP[] = {2}, BTN_UNDO[] = {3}, BTN_MENU[] = {6, 7, 8, 9};

struct Pad {
  SDL_Joystick *j;
  bool btn[32], prev[32];
  int hat, ax[6], nb, na, lastDx, lastDy;
  Uint32 nextRep;
  Pad() : j(0), hat(0), nb(0), na(0), lastDx(0), lastDy(0), nextRep(0) {
    memset(btn, 0, sizeof(btn)); memset(prev, 0, sizeof(prev)); memset(ax, 0, sizeof(ax));
  }
  void poll(Uint32 now, int &mx, int &my) {
    mx = my = 0;
    if (!j && SDL_NumJoysticks() > 0) j = SDL_JoystickOpen(0);
    memcpy(prev, btn, sizeof(btn));
    memset(btn, 0, sizeof(btn)); hat = 0;
    if (!j) return;
    SDL_JoystickUpdate();
    nb = SDL_JoystickNumButtons(j); if (nb > 32) nb = 32;
    for (int i = 0; i < nb; i++) btn[i] = SDL_JoystickGetButton(j, i) != 0;
    na = SDL_JoystickNumAxes(j); if (na > 6) na = 6;
    for (int i = 0; i < na; i++) ax[i] = SDL_JoystickGetAxis(j, i);
    if (SDL_JoystickNumHats(j) > 0) hat = SDL_JoystickGetHat(j, 0);
    int dx = 0, dy = 0;
    if (hat & SDL_HAT_LEFT) dx = -1; if (hat & SDL_HAT_RIGHT) dx = 1;
    if (hat & SDL_HAT_UP) dy = -1; if (hat & SDL_HAT_DOWN) dy = 1;
    if (!dx && !dy && na >= 2) {
      if (ax[0] < -16000) dx = -1; else if (ax[0] > 16000) dx = 1;
      if (ax[1] < -16000) dy = -1; else if (ax[1] > 16000) dy = 1;
    }
    if (dx || dy) {
      if (dx != lastDx || dy != lastDy) { mx = dx; my = dy; nextRep = now + 300; }
      else if (now >= nextRep) { mx = dx; my = dy; nextRep = now + 85; }
    }
    lastDx = dx; lastDy = dy;
  }
  bool down(const int *l, int n) const { for (int i = 0; i < n; i++) if (l[i] < 32 && btn[l[i]]) return true; return false; }
  bool wasDown(const int *l, int n) const { for (int i = 0; i < n; i++) if (l[i] < 32 && prev[l[i]]) return true; return false; }
  bool pressed(const int *l, int n) const { return down(l, n) && !wasDown(l, n); }
  bool released(const int *l, int n) const { return !down(l, n) && wasDown(l, n); }
};
#define PRESSED(p, L) (p).pressed(L, (int)(sizeof(L) / sizeof(int)))
#define RELEASED(p, L) (p).released(L, (int)(sizeof(L) / sizeof(int)))
#define HELD(p, L) (p).down(L, (int)(sizeof(L) / sizeof(int)))

// ===================================================================== app state
enum Screen { S_MENU, S_GAME, S_PADTEST };
enum Mode { M_LOCAL = 0, M_ROBOT = 1, M_LAN = 2 };

struct TimeCtl { const char *name; int secs; int inc; };
static const TimeCtl TIMES[] = {{"Unlimited", 0, 0}, {"1 min", 60, 0}, {"3 min", 180, 0}, {"3 | 2", 180, 2},
                                {"5 min", 300, 0}, {"5 | 3", 300, 3}, {"10 min", 600, 0}, {"15 | 10", 900, 10}, {"30 min", 1800, 0}};
#define NTIMES ((int)(sizeof(TIMES) / sizeof(TIMES[0])))

struct Anim { bool active; int from, to, piece, cap; Uint32 t0; };

struct App {
  Screen screen;
  int menuSel, mode, side, level, timeIdx, hints, autoflip;
  Game game;
  int humanSide;
  bool flipped, pendingFlip, pendingFlipVal;
  int curCol, curRow, sel;
  int lastFrom, lastTo;
  Anim anim[2];
  bool promoOpen; int promoIdx, promoTo; std::vector<Move> promoMoves;
  bool paused; int pauseSel;
  int aiState; Uint32 aiWait;
  Uint32 backDownAt; bool backLong;
  Uint32 now, lastFrame;
  int padTestStart;
  std::string lanIp, lanError;
  bool prevOver; Uint32 lastSync;
#ifdef ENABLE_LAN
  LanRoom room;
#endif
  App() : screen(S_MENU), menuSel(7), mode(M_ROBOT), side(0), level(2), timeIdx(0), hints(1), autoflip(0), humanSide(WHITE),
          flipped(false), pendingFlip(false), pendingFlipVal(false), curCol(4), curRow(6), sel(-1), lastFrom(-1), lastTo(-1),
          promoOpen(false), promoIdx(0), promoTo(0), paused(false), pauseSel(0), aiState(0), aiWait(0), backDownAt(0), backLong(false),
          now(0), lastFrame(0), padTestStart(0), prevOver(false), lastSync(0) {
    anim[0].active = anim[1].active = false;
  }
};
static App app;
static Pad pad;

static int squareAt(int col, int row) {
  int f = app.flipped ? 7 - col : col, r = app.flipped ? row : 7 - row;
  return r * 8 + f;
}
static void sqPix(int sq, int &x, int &y) {
  int f = fileOf(sq), r = rankOf(sq);
  int col = app.flipped ? 7 - f : f, row = app.flipped ? r : 7 - r;
  x = BX + col * CELL; y = BY + row * CELL;
}
static bool anyAnim() { return app.anim[0].active || app.anim[1].active; }
static bool isHumanTurn() {
  if (app.game.over()) return false;
  return app.mode == M_LOCAL || app.game.pos.side == app.humanSide;
}
static bool isAiTurn() { return app.mode == M_ROBOT && !app.game.over() && app.game.pos.side != app.humanSide; }
static bool hasMovesFrom(int sq) {
  Move L[256]; int n = app.game.legal(L);
  for (int i = 0; i < n; i++) if (L[i].from == sq) return true;
  return false;
}

static const char *MODE_NAMES[] = {"Local 2 players", "Vs Robot", "LAN (phone)"};
static const char *SIDE_NAMES[] = {"White", "Black", "Random"};

static void lanBroadcast() {
#ifdef ENABLE_LAN
  if (app.mode == M_LAN && app.room.srv.running()) app.room.broadcastState(app.game);
#endif
}

static void startAnim(const Pos &before, const Move &m) {
  Uint32 now = app.now;
  int pc = before.b[m.from];
  app.anim[0].active = true; app.anim[0].from = m.from; app.anim[0].to = m.to; app.anim[0].t0 = now;
  app.anim[0].piece = m.promo ? mkp(m.promo, pcolor(pc)) : pc;
  app.anim[0].piece = pc; // slide the original piece; promotion appears on arrival
  app.anim[0].cap = (m.flags & F_EP) ? 0 : before.b[m.to];
  app.anim[1].active = false;
  if (m.flags & F_CASTLE) {
    int r = rankOf(m.to), kside = fileOf(m.to) == 6;
    app.anim[1].active = true; app.anim[1].t0 = now;
    app.anim[1].from = sqOf(kside ? 7 : 0, r); app.anim[1].to = sqOf(kside ? 5 : 3, r);
    app.anim[1].piece = before.b[app.anim[1].from]; app.anim[1].cap = 0;
  }
  app.lastFrom = m.from; app.lastTo = m.to;
}

static void applyMove(const Move &m) {
  Pos before = app.game.pos;
  if (!app.game.play(m)) return;
  startAnim(before, m);
  app.sel = -1;
  if (app.mode == M_LOCAL && app.autoflip) { app.pendingFlip = true; app.pendingFlipVal = app.game.pos.side == BLACK; }
#ifdef ENABLE_LAN
  if (app.mode == M_LAN) app.room.last = toUci(m);
#endif
  lanBroadcast();
}

static void startGame() {
  app.humanSide = WHITE;
  if (app.mode != M_LOCAL) {
    app.humanSide = app.side == 0 ? WHITE : app.side == 1 ? BLACK : (int)((app.now / 7) & 1);
  }
  const TimeCtl &t = TIMES[app.timeIdx];
  app.game.reset((int64_t)t.secs * 1000, (int64_t)t.inc * 1000);
  app.flipped = app.mode != M_LOCAL && app.humanSide == BLACK;
  app.pendingFlip = false; app.sel = -1; app.lastFrom = app.lastTo = -1;
  app.anim[0].active = app.anim[1].active = false;
  app.promoOpen = false; app.paused = false; app.aiState = 0; app.prevOver = false;
  app.curCol = 4; app.curRow = 6; app.lanError.clear();
  app.screen = S_GAME;
#ifdef ENABLE_LAN
  if (app.mode == M_LAN) {
    app.room.phoneSide = app.humanSide ^ 1; app.room.last.clear();
    if (!app.room.srv.running()) {
      std::string html; for (int i = 0; WEB_INDEX_PARTS[i]; i++) html += WEB_INDEX_PARTS[i];
      app.room.srv.html = html;
      std::string err;
      if (!app.room.start(LAN_PORT, &err)) app.lanError = err;
    }
    app.lanIp = net::localIp();
    lanBroadcast();
  } else app.room.stop();
#endif
}

static void toMenu() {
  app.screen = S_MENU; app.paused = false;
#ifdef ENABLE_LAN
  app.room.stop();
  app.lanIp = net::localIp();
#endif
}

static void doUndo() {
  if (app.mode == M_LAN || app.game.plies() == 0) return;
  app.game.undo();
  if (app.mode == M_ROBOT && app.game.plies() > 0 && app.game.pos.side != app.humanSide) app.game.undo();
  app.anim[0].active = app.anim[1].active = false;
  app.sel = -1; app.aiState = 0; app.promoOpen = false;
  if (app.game.plies()) { app.lastFrom = app.game.moves.back().from; app.lastTo = app.game.moves.back().to; }
  else app.lastFrom = app.lastTo = -1;
}

static void onSelect() {
  Game &g = app.game;
  if (app.promoOpen) {
    applyMove(app.promoMoves[app.promoIdx]);
    app.promoOpen = false; return;
  }
  if (g.over()) { if (!anyAnim()) startGame(); return; }
  if (!isHumanTurn() || anyAnim()) return;
  int sq = squareAt(app.curCol, app.curRow), pc = g.pos.b[sq];
  if (app.sel >= 0) {
    Move L[256]; int n = g.legal(L);
    std::vector<Move> c;
    for (int i = 0; i < n; i++) if (L[i].from == app.sel && L[i].to == sq) c.push_back(L[i]);
    if (!c.empty()) {
      if (c.size() > 1) { app.promoOpen = true; app.promoMoves = c; app.promoIdx = 0; app.promoTo = sq; }
      else applyMove(c[0]);
      return;
    }
  }
  if (pc && pcolor(pc) == g.pos.side && hasMovesFrom(sq)) app.sel = (app.sel == sq) ? -1 : sq;
  else app.sel = -1;
}

static void onBack() {
  if (app.promoOpen) { app.promoOpen = false; return; }
  if (app.game.over() && !anyAnim()) { toMenu(); return; }
  app.sel = -1;
}

// ===================================================================== update
static void moveCursor(int dx, int dy) {
  app.curCol = std::max(0, std::min(7, app.curCol + dx));
  app.curRow = std::max(0, std::min(7, app.curRow + dy));
}

static void changeMenu(int row, int d) {
  switch (row) {
    case 0: app.mode = (app.mode + d + 3) % 3; break;
    case 1: if (app.mode != M_LOCAL) app.side = (app.side + d + 3) % 3; break;
    case 2: if (app.mode == M_ROBOT) app.level = std::max(0, std::min(4, app.level + d)); break;
    case 3: app.timeIdx = (app.timeIdx + d + NTIMES) % NTIMES; break;
    case 4: app.hints = !app.hints; break;
    case 5: if (app.mode == M_LOCAL) app.autoflip = !app.autoflip; break;
  }
}

static void updateMenu(int mx, int my) {
  if (my) app.menuSel = (app.menuSel + my + 8) % 8;
  if (mx) changeMenu(app.menuSel, mx);
  if (PRESSED(pad, BTN_SEL)) {
    if (app.menuSel == 7) startGame();
    else if (app.menuSel == 6) { app.screen = S_PADTEST; app.padTestStart = (int)app.now; }
    else if (app.menuSel < 6) changeMenu(app.menuSel, 1);
  }
}

static void updateGame(int mx, int my) {
  Game &g = app.game;
  Uint32 now = app.now;
  Uint32 dt = now - app.lastFrame; if (dt > 1000) dt = 1000;

  // animations
  for (int i = 0; i < 2; i++) if (app.anim[i].active && now - app.anim[i].t0 >= 230) app.anim[i].active = false;
  if (!anyAnim() && app.pendingFlip) { app.flipped = app.pendingFlipVal; app.pendingFlip = false; }

  // pause menu
  if (app.paused) {
    if (my) app.pauseSel = (app.pauseSel + my + 5) % 5;
    if (PRESSED(pad, BTN_BACK)) app.paused = false;
    if (PRESSED(pad, BTN_SEL)) {
      switch (app.pauseSel) {
        case 0: app.paused = false; break;
        case 1: app.paused = false; startGame(); break;
        case 2: if (!g.over()) { g.resign(app.mode == M_LOCAL ? g.pos.side : app.humanSide); lanBroadcast(); } app.paused = false; break;
        case 3: app.flipped = !app.flipped; app.paused = false; break;
        case 4: toMenu(); break;
      }
    }
    return;
  }

  // clocks
  if (!g.over()) { g.tick((int64_t)dt); }

  // LAN
#ifdef ENABLE_LAN
  if (app.mode == M_LAN && app.room.srv.running()) {
    LanEvents ev;
    Pos before = g.pos;
    int pliesBefore = g.plies();
    app.room.update(g, ev);
    if (ev.moved && g.plies() == pliesBefore + 1) { startAnim(before, g.moves.back()); app.room.last = toUci(g.moves.back()); app.sel = -1; app.room.broadcastState(g); }
    if (ev.resigned) app.room.broadcastState(g);
    if (ev.newGameReq) {
      app.side = (app.humanSide == WHITE) ? 1 : 0; startGame(); return;
    }
    if (ev.connected || ev.disconnected) app.room.broadcastState(g);
    if (now - app.lastSync > 5000) { app.lastSync = now; app.room.broadcastState(g); }
  }
#endif
  if (g.over() && !app.prevOver) { app.prevOver = true; lanBroadcast(); }

  // input
  if (app.promoOpen) {
    if (mx) app.promoIdx = (app.promoIdx + mx + (int)app.promoMoves.size()) % (int)app.promoMoves.size();
  } else if (mx || my) moveCursor(mx, my);
  if (PRESSED(pad, BTN_SEL)) onSelect();
  if (PRESSED(pad, BTN_BACK)) { app.backDownAt = now; app.backLong = false; }
  if (HELD(pad, BTN_BACK) && !app.backLong && app.backDownAt && now - app.backDownAt > 1000) { app.backLong = true; app.paused = true; app.pauseSel = 0; }
  if (RELEASED(pad, BTN_BACK)) { if (!app.backLong) onBack(); app.backDownAt = 0; }
  if (PRESSED(pad, BTN_FLIP)) app.flipped = !app.flipped;
  if (PRESSED(pad, BTN_UNDO)) doUndo();
  if (PRESSED(pad, BTN_MENU)) { app.paused = true; app.pauseSel = 0; }

  // robot
  if (isAiTurn() && !anyAnim() && !app.promoOpen) {
    if (app.aiState == 0) { app.aiState = 1; app.aiWait = now + 400; }
    else if (app.aiState == 1 && now >= app.aiWait) app.aiState = 2;
    else if (app.aiState == 2) {
      Move m = aiMove(g.pos, app.level);
      app.aiState = 0;
      if (!m.isNull()) applyMove(m);
    }
  } else if (!isAiTurn()) app.aiState = 0;
}

// ===================================================================== drawing
static const Col BG = {38, 36, 33, 255}, PANEL = {49, 46, 43, 255}, TXT = {232, 230, 227, 255}, MUT = {154, 152, 147, 255};
static const Col LIGHT = {238, 238, 210, 255}, DARK = {118, 150, 86, 255};

static std::string clockText(int64_t ms, bool timed) {
  if (!timed) return "--:--";
  int64_t s = (ms + 999) / 1000; if (s < 0) s = 0;
  char b[32]; snprintf(b, sizeof(b), "%d:%02d", (int)(s / 60), (int)(s % 60));
  return b;
}

static std::string nameFor(int color) {
  char b[64];
  if (app.mode == M_LOCAL) return color == WHITE ? "White" : "Black";
  if (app.mode == M_ROBOT) {
    if (color == app.humanSide) return color == WHITE ? "You (White)" : "You (Black)";
    snprintf(b, sizeof(b), "Robot - %s", LEVELS[app.level].name); return b;
  }
  if (color == app.humanSide) return "PS4 (you)";
#ifdef ENABLE_LAN
  return app.room.connected() ? "Phone" : "Phone (waiting...)";
#else
  return "Phone";
#endif
}

static void capturedInfo(int *capW, int *capB, int &diff) {
  // capW[t]: black pieces of type t captured by white; capB[t]: white pieces captured by black
  static const int init[7] = {0, 8, 2, 2, 2, 1, 0}, val[7] = {0, 1, 3, 3, 5, 9, 0};
  int cntW[7] = {0}, cntB[7] = {0};
  for (int s = 0; s < 64; s++) { int pc = app.game.pos.b[s]; if (!pc) continue; (pcolor(pc) == WHITE ? cntW : cntB)[ptype(pc)]++; }
  diff = 0;
  for (int t = 1; t <= 5; t++) {
    capW[t] = std::max(0, init[t] - cntB[t]); capB[t] = std::max(0, init[t] - cntW[t]);
    diff += capW[t] * val[t] - capB[t] * val[t];
  }
  // promotions can make counts exceed initial; clamp already handled by max()
}

static void drawBar(int color, int y) {
  fillRect(BX, y, 8 * CELL, 80, PANEL);
  std::string nm = nameFor(color);
  drawText(BX + 16, y + 8, nm.c_str(), 3, TXT);
  int capW[7] = {0}, capB[7] = {0}, diff = 0; capturedInfo(capW, capB, diff);
  int *mine = color == WHITE ? capW : capB;
  int x = BX + 16;
  for (int t = 5; t >= 1; t--) for (int k = 0; k < mine[t]; k++) { drawPieceAt(mkp(t, color ^ 1), x, y + 40, 36); x += t == 1 ? 14 : 24; }
  int adv = color == WHITE ? diff : -diff;
  if (adv > 0) { char b[16]; snprintf(b, sizeof(b), "+%d", adv); drawText(x + 14, y + 48, b, 3, MUT); }
  bool timed = app.game.initMs > 0;
  bool active = !app.game.over() && app.game.plies() > 0 && app.game.pos.side == color;
  bool low = timed && app.game.clk[color] < 20000;
  Col bgc = active ? (low ? C(190, 50, 50) : C(240, 240, 240)) : C(30, 29, 27);
  Col fgc = active ? (low ? C(255, 255, 255) : C(38, 36, 33)) : TXT;
  fillRect(BX + 8 * CELL - 210, y + 8, 200, 64, bgc);
  std::string ct = clockText(app.game.clk[color], timed);
  textC(BX + 8 * CELL - 110, y + 20, ct.c_str(), 5, fgc);
}

static void drawBoard() {
  Game &g = app.game;
  int kingSqr = g.check() ? kingSq(g.pos, g.pos.side) : -1;
  for (int row = 0; row < 8; row++) for (int col = 0; col < 8; col++) {
    int sq = squareAt(col, row), x = BX + col * CELL, y = BY + row * CELL;
    bool light = ((fileOf(sq) + rankOf(sq)) & 1) == 1;
    fillRect(x, y, CELL, CELL, light ? LIGHT : DARK);
    if (sq == app.lastFrom || sq == app.lastTo) fillRect(x, y, CELL, CELL, C(255, 255, 50, 105));
    if (sq == app.sel) fillRect(x, y, CELL, CELL, C(20, 120, 40, 140));
    if (sq == kingSqr) { drawDisc(x + CELL / 2, y + CELL / 2, CELL, C(255, 40, 40, 190), false); }
    // coordinates
    Col cc = light ? DARK : LIGHT;
    if (row == 7) { char s[2] = {(char)('a' + fileOf(sq)), 0}; drawText(x + CELL - 14, y + CELL - 18, s, 2, cc); }
    if (col == 0) { char s[2] = {(char)('1' + rankOf(sq)), 0}; drawText(x + 4, y + 4, s, 2, cc); }
  }
  // pieces (skip those currently sliding)
  for (int sq = 0; sq < 64; sq++) {
    int pc = g.pos.b[sq]; if (!pc) continue;
    bool skip = false;
    for (int i = 0; i < 2; i++) if (app.anim[i].active && app.anim[i].to == sq) skip = true;
    if (skip) continue;
    int x, y; sqPix(sq, x, y);
    drawPieceAt(pc, x, y, CELL);
  }
  // legal-move hints
  if (app.hints && app.sel >= 0 && !app.promoOpen) {
    Move L[256]; int n = g.legal(L);
    for (int i = 0; i < n; i++) if (L[i].from == app.sel) {
      int x, y; sqPix(L[i].to, x, y);
      if (g.pos.b[L[i].to] || (L[i].flags & F_EP)) drawDisc(x + CELL / 2, y + CELL / 2, CELL - 6, C(20, 85, 0, 110), true);
      else drawDisc(x + CELL / 2, y + CELL / 2, 32, C(20, 85, 0, 105), false);
    }
  }
  // sliding pieces, drawn last with easing
  for (int i = 0; i < 2; i++) if (app.anim[i].active) {
    float p = (float)(app.now - app.anim[i].t0) / 230.f; if (p > 1) p = 1;
    float e = 1.f - (1.f - p) * (1.f - p) * (1.f - p);
    int x0, y0, x1, y1; sqPix(app.anim[i].from, x0, y0); sqPix(app.anim[i].to, x1, y1);
    if (app.anim[i].cap && p < 0.8f) { int pcx = app.anim[i].to; int cx, cy; sqPix(pcx, cx, cy); drawPieceAt(app.anim[i].cap, cx, cy, CELL, (int)(255 * (1.f - p / 0.8f))); }
    drawPieceAt(app.anim[i].piece, x0 + (int)((x1 - x0) * e), y0 + (int)((y1 - y0) * e), CELL);
  }
  // cursor
  if (!app.game.over()) {
    int x = BX + app.curCol * CELL, y = BY + app.curRow * CELL;
    Col cc = isHumanTurn() ? C(255, 210, 0) : C(150, 150, 150);
    frameRect(x, y, CELL, CELL, 6, cc);
  }
}

static void statusText(std::string &s, Col &c) {
  Game &g = app.game;
  c = TXT;
  if (g.over()) { s = g.resultText() + " - " + g.reason; c = C(255, 210, 0); return; }
  if (app.paused) { s = "Paused"; return; }
  std::string chk = g.check() ? "CHECK! " : "";
  if (app.mode == M_ROBOT) s = isAiTurn() ? (app.aiState >= 1 ? "Robot is thinking..." : "Robot to move") : chk + "Your move";
  else if (app.mode == M_LAN) {
#ifdef ENABLE_LAN
    s = isHumanTurn() ? chk + "Your move" : (app.room.connected() ? chk + "Phone to move" : "Waiting for phone...");
#else
    s = "LAN unavailable";
#endif
  } else s = chk + (g.pos.side == WHITE ? "White to move" : "Black to move");
  if (g.check()) c = C(255, 120, 120);
}

static void drawSide() {
  Game &g = app.game;
  // left panel
  fillRect(40, 124, 440, 832, PANEL);
  drawText(60, 144, "PS4 CHESS", 4, TXT);
  drawText(60, 196, MODE_NAMES[app.mode], 2, MUT);
  if (app.mode == M_ROBOT) { std::string l = std::string("Level: ") + LEVELS[app.level].name; drawText(60, 224, l.c_str(), 2, MUT); }
  std::string tc = std::string("Time: ") + TIMES[app.timeIdx].name; drawText(60, 252, tc.c_str(), 2, MUT);
  std::string st; Col sc; statusText(st, sc);
  fillRect(60, 300, 400, 90, C(38, 36, 33));
  drawText(76, 322, st.c_str(), st.size() > 20 ? 2 : 3, sc);
  char mv[48]; snprintf(mv, sizeof(mv), "Move %d", g.pos.full); drawText(76, 358, mv, 2, MUT);
#ifdef ENABLE_LAN
  if (app.mode == M_LAN) {
    drawText(60, 420, "Open on your phone:", 2, MUT);
    std::string url = "http://" + (app.lanIp.empty() ? std::string("<PS4 IP>") : app.lanIp) + ":" + std::to_string(LAN_PORT);
    drawText(60, 448, url.c_str(), url.size() > 24 ? 2 : 3, C(120, 200, 255));
    drawText(60, 488, app.room.connected() ? "Phone connected" : "Waiting for phone...", 2, app.room.connected() ? C(120, 220, 120) : MUT);
    if (!app.lanError.empty()) drawText(60, 516, ("Server: " + app.lanError).c_str(), 2, C(255, 120, 120));
  }
#endif
  int y = 640;
  drawText(60, y, "CONTROLS", 2, TXT); y += 30;
  const char *help[] = {"D-pad/Stick  move", "X  select / move", "O  cancel", "Hold O  menu", "SQ  flip board", "TRI  undo"};
  for (int i = 0; i < 6; i++) { drawText(60, y, help[i], 2, MUT); y += 26; }
  // right panel: move list
  fillRect(1440, 124, 440, 832, PANEL);
  drawText(1460, 140, "MOVES", 3, TXT);
  int total = (int)g.san.size(), rows = (total + 1) / 2, maxRows = 27, first = std::max(0, rows - maxRows);
  int ry = 184;
  for (int r = first; r < rows; r++) {
    char num[16]; snprintf(num, sizeof(num), "%d.", r + 1);
    drawText(1460, ry, num, 2, MUT);
    bool lastW = (2 * r == total - 1), lastB = (2 * r + 1 == total - 1);
    if (lastW) fillRect(1500, ry - 4, 170, 26, C(80, 78, 74));
    drawText(1510, ry, g.san[2 * r].c_str(), 2, TXT);
    if (2 * r + 1 < total) { if (lastB) fillRect(1680, ry - 4, 170, 26, C(80, 78, 74)); drawText(1690, ry, g.san[2 * r + 1].c_str(), 2, TXT); }
    ry += 28;
  }
}

static void panelBox(int x, int y, int w, int h) { fillRect(x - 6, y - 6, w + 12, h + 12, C(0, 0, 0, 180)); fillRect(x, y, w, h, C(49, 46, 43)); frameRect(x, y, w, h, 3, C(120, 150, 86)); }

static void drawOverlays() {
  Game &g = app.game;
  if (app.promoOpen) {
    int w = 4 * 130 + 40, x = SCR_W / 2 - w / 2, y = 420;
    panelBox(x, y, w, 220);
    textC(SCR_W / 2, y + 16, "Promote to:", 3, TXT);
    for (int i = 0; i < (int)app.promoMoves.size(); i++) {
      int px = x + 20 + i * 130, py = y + 70;
      fillRect(px, py, 120, 120, i == app.promoIdx ? C(255, 210, 0) : C(70, 68, 64));
      drawPieceAt(mkp(app.promoMoves[i].promo, g.pos.side), px + 8, py + 8, 104);
    }
  }
  if (g.over() && !anyAnim() && !app.paused) {
    int w = 640, x = SCR_W / 2 - w / 2, y = 400;
    panelBox(x, y, w, 250);
    std::string r = g.resultText();
    textC(SCR_W / 2, y + 30, r.c_str(), 5, C(255, 210, 0));
    textC(SCR_W / 2, y + 100, g.reason.c_str(), 3, TXT);
    textC(SCR_W / 2, y + 170, "X: Rematch     O: Menu", 3, MUT);
  }
  if (app.paused) {
    int w = 520, x = SCR_W / 2 - w / 2, y = 330;
    panelBox(x, y, w, 420);
    textC(SCR_W / 2, y + 20, "PAUSED", 4, TXT);
    const char *it[5] = {"Resume", "New game", "Resign", "Flip board", "Main menu"};
    for (int i = 0; i < 5; i++) {
      if (i == app.pauseSel) fillRect(x + 20, y + 80 + i * 62, w - 40, 52, C(118, 150, 86));
      textC(SCR_W / 2, y + 92 + i * 62, it[i], 3, i == app.pauseSel ? C(255, 255, 255) : TXT);
    }
  }
}

static void drawGame() {
  fillRect(0, 0, SCR_W, SCR_H, BG);
  int bottom = app.flipped ? BLACK : WHITE;
  drawBar(bottom ^ 1, 36);
  drawBar(bottom, BY + 8 * CELL + 8);
  drawBoard();
  drawSide();
  drawOverlays();
}

static void drawMenu() {
  fillRect(0, 0, SCR_W, SCR_H, BG);
  textC(SCR_W / 2, 70, "PS4 CHESS", 9, TXT);
  // decorative pieces
  for (int i = 0; i < 6; i++) { drawPieceAt(mkp(i + 1, WHITE), 360 + i * 200, 170, 150); }
  const char *labels[8] = {"Game mode", "Play as", "Robot level", "Time control", "Show legal moves", "Auto-flip board", "Controller test", "START GAME"};
  std::string vals[8];
  vals[0] = MODE_NAMES[app.mode];
  vals[1] = app.mode == M_LOCAL ? "Both players" : SIDE_NAMES[app.side];
  vals[2] = app.mode == M_ROBOT ? LEVELS[app.level].name : "-";
  vals[3] = TIMES[app.timeIdx].name;
  vals[4] = app.hints ? "On" : "Off";
  vals[5] = app.mode == M_LOCAL ? (app.autoflip ? "On" : "Off") : "-";
  vals[6] = ""; vals[7] = "";
  int x = 460, w = 1000, y0 = 380;
  for (int i = 0; i < 8; i++) {
    int y = y0 + i * 70;
    bool s = i == app.menuSel;
    fillRect(x, y, w, 60, s ? C(118, 150, 86) : PANEL);
    if (i == 7) { textC(x + w / 2, y + 14, labels[i], 4, s ? C(255, 255, 255) : C(150, 200, 110)); continue; }
    drawText(x + 24, y + 18, labels[i], 3, s ? C(255, 255, 255) : TXT);
    if (i != 6) {
      std::string v = vals[i];
      if (s && i != 6) v = "<  " + v + "  >";
      textR(x + w - 24, y + 18, v.c_str(), 3, s ? C(255, 255, 255) : MUT);
    }
  }
  if (app.mode == M_LAN) {
#ifdef ENABLE_LAN
    std::string ip = app.lanIp.empty() ? "your PS4 IP (Settings > Network)" : app.lanIp;
    std::string t = "Phone: connect to the same Wi-Fi, open http://" + ip + ":" + std::to_string(LAN_PORT);
    textC(SCR_W / 2, y0 + 8 * 70 + 10, t.c_str(), 2, C(120, 200, 255));
#else
    textC(SCR_W / 2, y0 + 8 * 70 + 10, "LAN is not available in this build", 2, C(255, 120, 120));
#endif
  }
  textC(SCR_W / 2, SCR_H - 50, "D-pad: up/down choose, left/right change    X: confirm", 2, MUT);
}

static void drawPadTest() {
  fillRect(0, 0, SCR_W, SCR_H, BG);
  textC(SCR_W / 2, 60, "CONTROLLER TEST", 6, TXT);
  textC(SCR_W / 2, 140, "Press buttons - their numbers light up. Press button 1 (O) to exit.", 2, MUT);
  for (int i = 0; i < 32; i++) {
    int x = 360 + (i % 8) * 150, y = 220 + (i / 8) * 110;
    bool on = pad.btn[i];
    fillRect(x, y, 130, 90, on ? C(118, 150, 86) : PANEL);
    char b[8]; snprintf(b, sizeof(b), "%d", i); textC(x + 65, y + 28, b, 4, on ? C(255, 255, 255) : MUT);
  }
  char s[160];
  snprintf(s, sizeof(s), "joystick: %s   buttons: %d   axes: %d", pad.j ? "found" : "NOT FOUND", pad.nb, pad.na);
  drawText(360, 700, s, 3, TXT);
  snprintf(s, sizeof(s), "hat: %d   axis0: %d  axis1: %d  axis2: %d  axis3: %d", pad.hat, pad.ax[0], pad.ax[1], pad.ax[2], pad.ax[3]);
  drawText(360, 750, s, 3, TXT);
}

// ===================================================================== main loop
int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) return 1;
  SDL_Window *win = SDL_CreateWindow("PS4 Chess", 0, 0, SCR_W, SCR_H, 0);
  R = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
  if (!R) R = SDL_CreateRenderer(win, -1, 0);
  if (!R) return 2;
  initGfx();
  app.lastFrame = SDL_GetTicks();
#ifdef ENABLE_LAN
  app.lanIp = net::localIp();
#endif
  bool run = true;
  while (run) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) run = false;
    app.now = SDL_GetTicks();
    int mx, my;
    pad.poll(app.now, mx, my);
    if (app.screen == S_MENU) updateMenu(mx, my);
    else if (app.screen == S_GAME) updateGame(mx, my);
    else if (app.screen == S_PADTEST) {
      if (PRESSED(pad, BTN_BACK) || (int)app.now - app.padTestStart > 90000) app.screen = S_MENU;
    }
    app.lastFrame = app.now;
    if (app.screen == S_MENU) drawMenu();
    else if (app.screen == S_GAME) drawGame();
    else drawPadTest();
    SDL_RenderPresent(R);
    SDL_Delay(1);
  }
  SDL_Quit();
  return 0;
}
