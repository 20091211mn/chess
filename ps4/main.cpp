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
#include <cstdlib>
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
#include "textrender.hpp"
#include "qr.hpp"
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
#define BX 560
#define BY 124
#define BOARD_PX (8 * CELL)
#define LAN_PORT 8080

struct Col { Uint8 r, g, b, a; };
static Col C(int r, int g, int b, int a = 255) { Col c = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a}; return c; }

// chess.com dark palette
static const Col BG = {38, 37, 34, 255}, PANEL = {49, 46, 43, 255}, CARD = {33, 32, 29, 255}, LINE = {64, 61, 57, 255};
static const Col TXT = {255, 255, 255, 255}, SUB = {186, 186, 186, 255}, MUT = {152, 151, 149, 255}, GREEN = {129, 182, 76, 255}, GREEN_D = {95, 138, 58, 255};
static const Col RED = {224, 78, 70, 255}, GOLD = {255, 205, 0, 255};

struct Theme { const char *name; Col light, dark; };
static const Theme THEMES[] = {{"Green", {235, 236, 208, 255}, {119, 149, 86, 255}}, {"Brown", {240, 217, 181, 255}, {181, 136, 99, 255}},
                               {"Blue", {222, 227, 230, 255}, {140, 162, 173, 255}}, {"Purple", {240, 241, 240, 255}, {132, 118, 186, 255}}};
#define NTHEMES 4

// ===================================================================== graphics
enum { F_XS = 0, F_S, F_M, F_MB, F_L, F_XL, F_T };
static SDL_Renderer *R = 0;
static SDL_Texture *texAtlas = 0, *texPiece[12], *texDisc = 0, *texRing = 0, *texArrow = 0;
static bool texOK = false;
static txt::Atlas ATLAS;

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
  txt::build(ATLAS);
  texAtlas = makeTex(ATLAS.w, ATLAS.h, ATLAS.px);
  bool ok = texAtlas != 0;
  for (int c = 0; c < 2; c++) for (int t = 1; t <= 6; t++) {
    std::vector<uint32_t> px; gfx::renderPiece(px, CELL, t, c);
    texPiece[c * 6 + t - 1] = makeTex(CELL, CELL, px);
    if (!texPiece[c * 6 + t - 1]) ok = false;
  }
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
  // board-sized texture for the hint arrow (updated on demand)
  SDL_Texture *ta = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, BOARD_PX, BOARD_PX);
  if (ta) { std::vector<uint32_t> z((size_t)BOARD_PX * BOARD_PX, 0); SDL_UpdateTexture(ta, 0, z.data(), BOARD_PX * 4); SDL_SetTextureBlendMode(ta, SDL_BLENDMODE_BLEND); }
  texArrow = ta;
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
static void drawDisc(int cx, int cy, int dw, int dh, Col c, bool ring) {
  SDL_Texture *t = ring ? texRing : texDisc;
  if (texOK) {
    SDL_Rect dst; dst.x = cx - dw / 2; dst.y = cy - dh / 2; dst.w = dw; dst.h = dh;
    SDL_SetTextureColorMod(t, c.r, c.g, c.b); SDL_SetTextureAlphaMod(t, c.a);
    SDL_RenderCopy(R, t, 0, &dst);
  } else if (ring) frameRect(cx - dw / 2, cy - dh / 2, dw, dh, 4, c);
  else fillRect(cx - dw / 4, cy - dh / 4, dw / 2, dh / 2, c);
}
// rounded rectangle (opaque colours; corners are discs)
static void fillRound(int x, int y, int w, int h, int r, Col c) {
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  fillRect(x + r, y, w - 2 * r, h, c);
  fillRect(x, y + r, r, h - 2 * r, c);
  fillRect(x + w - r, y + r, r, h - 2 * r, c);
  drawDisc(x + r, y + r, 2 * r, 2 * r, c, false); drawDisc(x + w - r, y + r, 2 * r, 2 * r, c, false);
  drawDisc(x + r, y + h - r, 2 * r, 2 * r, c, false); drawDisc(x + w - r, y + h - r, 2 * r, 2 * r, c, false);
}

static int textW(const char *s, int face) {
  if (texOK) return txt::width(ATLAS, face, s);
  return (int)strlen(s) * 10;
}
// y = top of the text line box
static void drawText(int x, int y, const char *s, int face, Col c) {
  if (!texOK) {
    for (int i = 0; s[i]; i++) {
      int g = (unsigned char)s[i] - 32; if (g < 0 || g > 94) g = '?' - 32;
      for (int cx = 0; cx < 5; cx++) for (int r = 0; r < 8; r++) if ((FONT5x7[g][cx] >> r) & 1) fillRect(x + i * 10 + cx * 2, y + r * 2, 2, 2, c);
    }
    return;
  }
  const txt::Face &F = ATLAS.face[face];
  int pen = x;
  SDL_SetTextureColorMod(texAtlas, c.r, c.g, c.b); SDL_SetTextureAlphaMod(texAtlas, c.a);
  for (; *s; s++) {
    int g = (unsigned char)*s - 32; if (g < 0 || g > 94) g = '?' - 32;
    const txt::Rect &r = F.g[g];
    if (r.w) {
      SDL_Rect src; src.x = r.x; src.y = r.y; src.w = r.w; src.h = r.h;
      SDL_Rect dst; dst.x = pen + r.bx; dst.y = y + F.asc + r.by; dst.w = r.w; dst.h = r.h;
      SDL_RenderCopy(R, texAtlas, &src, &dst);
    }
    pen += r.adv;
  }
}
static void textC(int cx, int y, const char *s, int face, Col c) { drawText(cx - textW(s, face) / 2, y, s, face, c); }
static void textR(int rx, int y, const char *s, int face, Col c) { drawText(rx - textW(s, face), y, s, face, c); }
static int lineH(int face) { return texOK ? ATLAS.face[face].lh : 16; }

static void drawPieceAt(int pc, int x, int y, int sz, int alpha = 255, bool shadow = false) {
  int ty = ptype(pc), col = pcolor(pc);
  if (shadow && texOK) drawDisc(x + sz / 2, y + sz * 86 / 100, sz * 62 / 100, sz * 14 / 100, C(0, 0, 0, 55 * alpha / 255), false);
  if (texOK) {
    SDL_Texture *t = texPiece[col * 6 + ty - 1];
    SDL_Rect dst; dst.x = x; dst.y = y; dst.w = sz; dst.h = sz;
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    SDL_RenderCopy(R, t, 0, &dst);
  } else {
    fillRect(x + sz / 8, y + sz / 8, sz * 3 / 4, sz * 3 / 4, col ? C(60, 60, 64, alpha) : C(245, 245, 245, alpha));
    char s[2] = {pieceChar(pc), 0};
    textC(x + sz / 2, y + sz / 4, s, F_L, col ? C(240, 240, 240, alpha) : C(20, 20, 20, alpha));
  }
}

// ===================================================================== input
static const int BTN_SEL[] = {0}, BTN_BACK[] = {1}, BTN_FLIP[] = {2}, BTN_UNDO[] = {3}, BTN_MENU[] = {6, 7, 8, 9};

struct Pad {
  SDL_Joystick *j;
  bool btn[32], prev[32], evBtn[32];
  int hat, ax[6], nb, na, lastDx, lastDy, evHat, evAx[6], lastBtn;
  Uint32 nextRep;
  Pad() : j(0), hat(0), nb(0), na(0), lastDx(0), lastDy(0), evHat(0), lastBtn(-1), nextRep(0) {
    memset(btn, 0, sizeof(btn)); memset(prev, 0, sizeof(prev)); memset(ax, 0, sizeof(ax));
    memset(evBtn, 0, sizeof(evBtn)); memset(evAx, 0, sizeof(evAx));
  }
  // events are used in addition to polling: some SDL ports only report input through events
  void onEvent(const SDL_Event &e) {
    if (e.type == SDL_JOYBUTTONDOWN && e.jbutton.button < 32) { evBtn[e.jbutton.button] = true; lastBtn = e.jbutton.button; }
    else if (e.type == SDL_JOYBUTTONUP && e.jbutton.button < 32) evBtn[e.jbutton.button] = false;
    else if (e.type == SDL_JOYHATMOTION) evHat = e.jhat.value;
    else if (e.type == SDL_JOYAXISMOTION && e.jaxis.axis < 6) evAx[e.jaxis.axis] = e.jaxis.value;
  }
  void poll(Uint32 now, int &mx, int &my) {
    mx = my = 0;
    if (!j && SDL_NumJoysticks() > 0) j = SDL_JoystickOpen(0);
    memcpy(prev, btn, sizeof(btn));
    memset(btn, 0, sizeof(btn)); hat = 0;
    if (j) {
      SDL_JoystickUpdate();
      nb = SDL_JoystickNumButtons(j); if (nb > 32) nb = 32;
      for (int i = 0; i < nb; i++) btn[i] = SDL_JoystickGetButton(j, i) != 0;
      na = SDL_JoystickNumAxes(j); if (na > 6) na = 6;
      for (int i = 0; i < na; i++) ax[i] = SDL_JoystickGetAxis(j, i);
      if (SDL_JoystickNumHats(j) > 0) hat = SDL_JoystickGetHat(j, 0);
    }
    for (int i = 0; i < 32; i++) { if (evBtn[i]) btn[i] = true; if (btn[i] && !prev[i]) lastBtn = i; }
    hat |= evHat;
    for (int i = 0; i < 6; i++) if (!ax[i]) ax[i] = evAx[i];
    if (na < 2) na = 2;
    int dx = 0, dy = 0;
    if (hat & SDL_HAT_LEFT) dx = -1;
    if (hat & SDL_HAT_RIGHT) dx = 1;
    if (hat & SDL_HAT_UP) dy = -1;
    if (hat & SDL_HAT_DOWN) dy = 1;
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
enum Dlg { D_NONE = 0, D_PAUSE, D_CONFIRM };
enum Confirm { C_RESIGN = 1, C_DRAW_RECV, C_MENU };

struct TimeCtl { const char *name; const char *kind; int secs; int inc; };
static const TimeCtl TIMES[] = {{"Unlimited", "", 0, 0}, {"1 min", "Bullet", 60, 0}, {"3 min", "Blitz", 180, 0}, {"3 | 2", "Blitz", 180, 2},
                                {"5 min", "Blitz", 300, 0}, {"5 | 3", "Blitz", 300, 3}, {"10 min", "Rapid", 600, 0}, {"15 | 10", "Rapid", 900, 10}, {"30 min", "Rapid", 1800, 0}};
#define NTIMES ((int)(sizeof(TIMES) / sizeof(TIMES[0])))
#define MENU_ROWS 10

struct Anim { bool active; int from, to, piece, cap; Uint32 t0; };

struct App {
  Screen screen;
  int menuSel, mode, side, level, timeIdx, theme, hints, evalOn, autoflip;
  Game game;
  int humanSide;
  bool flipped, pendingFlip, pendingFlipVal;
  int curCol, curRow, sel;
  int lastFrom, lastTo;
  Anim anim[2];
  bool promoOpen; int promoIdx; std::vector<Move> promoMoves;
  int dlg, dlgSel, confirmKind;
  int overSel;
  int aiState; Uint32 aiWait;
  Uint32 backDownAt; bool backLong;
  Uint32 now, lastFrame;
  int padTestStart;
  std::string lanIp, lanError, toast;
  Uint32 toastUntil;
  bool prevOver; Uint32 lastSync;
  int evalCp, evalPly; float evalDisp;
  int hintFrom, hintTo; bool hintFlip; Uint32 hintUntil;
  qr::Code qrc; std::string qrText;
#ifdef ENABLE_LAN
  LanRoom room;
#endif
  App() : screen(S_MENU), menuSel(9), mode(M_ROBOT), side(0), level(2), timeIdx(0), theme(0), hints(1), evalOn(1), autoflip(0), humanSide(WHITE),
          flipped(false), pendingFlip(false), pendingFlipVal(false), curCol(4), curRow(6), sel(-1), lastFrom(-1), lastTo(-1),
          promoOpen(false), promoIdx(0), dlg(D_NONE), dlgSel(0), confirmKind(0), overSel(0), aiState(0), aiWait(0), backDownAt(0), backLong(false),
          now(0), lastFrame(0), padTestStart(0), toastUntil(0), prevOver(false), lastSync(0), evalCp(0), evalPly(-1), evalDisp(0.5f),
          hintFrom(-1), hintTo(-1), hintFlip(false), hintUntil(0) {
    anim[0].active = anim[1].active = false;
  }
};
static App app;
static Pad pad;
static unsigned g_frames = 0;

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
static void showToast(const std::string &s) { app.toast = s; app.toastUntil = app.now + 2600; }

static const char *MODE_NAMES[] = {"Play a Friend (local)", "Play vs Robot", "Play on Phone (LAN)"};
static const char *SIDE_NAMES[] = {"White", "Black", "Random"};

static std::string lanUrl() {
  if (app.lanIp.empty()) return "";
  return "http://" + app.lanIp + ":" + std::to_string(LAN_PORT);
}
static void ensureQr() {
  std::string u = lanUrl();
  if (u == app.qrText) return;
  app.qrText = u; app.qrc = qr::Code();
  if (!u.empty()) qr::encode(u, app.qrc);
}

static void lanBroadcast() {
#ifdef ENABLE_LAN
  if (app.mode == M_LAN && app.room.srv.running()) { app.room.extra.evalCp = app.evalCp; app.room.extra.hasEval = true; app.room.broadcastState(app.game); }
#endif
}

#ifdef ENABLE_LAN
static Uint32 g_lanRetryAt = 0;
// starts the server one step per frame so the screen always shows what is about to happen
static void lanEnsure() {
  if (app.room.srv.running()) return;
  if (g_lanRetryAt && app.now < g_lanRetryAt) return;
  g_lanRetryAt = 0;
  if (app.room.srv.stage() == 0) {
    std::string html; for (int i = 0; WEB_INDEX_PARTS[i]; i++) html += WEB_INDEX_PARTS[i];
    app.room.srv.html = html;
    app.room.srv.addRoute("/manifest.webmanifest", "application/manifest+json",
      "{\"name\":\"PS4 Chess\",\"short_name\":\"Chess\",\"start_url\":\"/\",\"display\":\"standalone\",\"background_color\":\"#262522\",\"theme_color\":\"#262522\","
      "\"icons\":[{\"src\":\"/icon.svg\",\"sizes\":\"any\",\"type\":\"image/svg+xml\",\"purpose\":\"any\"}]}");
    app.room.srv.addRoute("/icon.svg", "image/svg+xml",
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='18' fill='#769656'/>"
      "<text x='50' y='72' font-size='70' text-anchor='middle' fill='#fff'>&#9822;</text></svg>");
  }
  std::string err;
  int r = app.room.srv.startStep(LAN_PORT, &err);
  if (r == 0) { app.lanError = err; app.room.srv.stop(); g_lanRetryAt = app.now + 3000; }
  else if (r == 2) { app.lanError.clear(); app.lanIp = net::localIp(); }
}
#endif

// ===================================================================== hint arrow
static void buildArrow(int from, int to) {
  if (!texArrow) return;
  int x0, y0, x1, y1; sqPix(from, x0, y0); sqPix(to, x1, y1);
  float sx = (float)(x0 - BX + CELL / 2), sy = (float)(y0 - BY + CELL / 2), ex = (float)(x1 - BX + CELL / 2), ey = (float)(y1 - BY + CELL / 2);
  float dx = ex - sx, dy = ey - sy, len = sqrtf(dx * dx + dy * dy);
  if (len < 1) return;
  dx /= len; dy /= len;
  float nx = -dy, ny = dx, shaft = 11.f, head = 28.f, headLen = 46.f;
  float bxp = ex - dx * headLen, byp = ey - dy * headLen;
  std::vector<gfx::Poly> v;
  float pts[14] = {sx + dx * 22 + nx * shaft, sy + dy * 22 + ny * shaft, bxp + nx * shaft, byp + ny * shaft, bxp + nx * head, byp + ny * head,
                   ex - dx * 6, ey - dy * 6, bxp - nx * head, byp - ny * head, bxp - nx * shaft, byp - ny * shaft, sx + dx * 22 - nx * shaft, sy + dy * 22 - ny * shaft};
  gfx::poly(v, pts, 7);
  int minx = BOARD_PX, miny = BOARD_PX, maxx = 0, maxy = 0;
  for (int i = 0; i < 7; i++) {
    minx = std::min(minx, (int)pts[i * 2] - 2); maxx = std::max(maxx, (int)pts[i * 2] + 2);
    miny = std::min(miny, (int)pts[i * 2 + 1] - 2); maxy = std::max(maxy, (int)pts[i * 2 + 1] + 2);
  }
  minx = std::max(0, minx); miny = std::max(0, miny); maxx = std::min(BOARD_PX - 1, maxx); maxy = std::min(BOARD_PX - 1, maxy);
  std::vector<uint32_t> buf((size_t)BOARD_PX * BOARD_PX, 0);
  for (int y = miny; y <= maxy; y++) for (int x = minx; x <= maxx; x++) {
    int a = 0;
    for (int sy2 = 0; sy2 < 2; sy2++) for (int sx2 = 0; sx2 < 2; sx2++) if (gfx::inside(v, x + 0.25f + sx2 * 0.5f, y + 0.25f + sy2 * 0.5f)) a++;
    if (a) buf[(size_t)y * BOARD_PX + x] = ((uint32_t)(a * 205 / 4) << 24) | (255u << 16) | (170u << 8) | 0u;
  }
  SDL_UpdateTexture(texArrow, 0, buf.data(), BOARD_PX * 4);
}

// ===================================================================== game logic
static void startAnim(const Pos &before, const Move &m) {
  Uint32 now = app.now;
  int pc = before.b[m.from];
  app.anim[0].active = true; app.anim[0].from = m.from; app.anim[0].to = m.to; app.anim[0].t0 = now;
  app.anim[0].piece = pc;
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
static void clearHint() { app.hintFrom = app.hintTo = -1; app.hintUntil = 0; }

static void applyMove(const Move &m) {
  Pos before = app.game.pos;
  if (!app.game.play(m)) return;
  startAnim(before, m);
  app.sel = -1; clearHint();
  if (app.mode == M_LOCAL && app.autoflip) { app.pendingFlip = true; app.pendingFlipVal = app.game.pos.side == BLACK; }
#ifdef ENABLE_LAN
  if (app.mode == M_LAN) app.room.last = toUci(m);
#endif
  lanBroadcast();
}

static void startGame() {
  app.humanSide = WHITE;
  if (app.mode != M_LOCAL) app.humanSide = app.side == 0 ? WHITE : app.side == 1 ? BLACK : (int)((app.now / 7) & 1);
  const TimeCtl &t = TIMES[app.timeIdx];
  app.game.reset((int64_t)t.secs * 1000, (int64_t)t.inc * 1000);
  app.flipped = app.mode != M_LOCAL && app.humanSide == BLACK;
  app.pendingFlip = false; app.sel = -1; app.lastFrom = app.lastTo = -1;
  app.anim[0].active = app.anim[1].active = false;
  app.promoOpen = false; app.dlg = D_NONE; app.overSel = 0; app.aiState = 0; app.prevOver = false;
  app.curCol = 4; app.curRow = 6; app.lanError.clear();
  app.evalPly = -1; app.evalCp = 0; app.evalDisp = 0.5f; clearHint();
  app.screen = S_GAME;
#ifdef ENABLE_LAN
  if (app.mode == M_LAN) {
    app.room.phoneSide = app.humanSide ^ 1; app.room.last.clear();
    app.room.lockMsg.clear();
    lanEnsure();
    lanBroadcast();
  } else app.room.stop();
#endif
}
static void startRematch() {
  if (app.mode != M_LOCAL) app.side = (app.humanSide == WHITE) ? 1 : 0; // colours swap, like chess.com
  startGame();
}

static void toMenu() {
  app.screen = S_MENU; app.dlg = D_NONE;
#ifdef ENABLE_LAN
  if (app.mode != M_LAN) app.room.stop();
  else app.room.lockMsg = "Waiting for the game to start on the PS4...";
  app.lanIp = net::localIp();
#endif
}

static void doUndo() {
  if (app.mode == M_LAN || app.game.plies() == 0) return;
  app.game.undo();
  if (app.mode == M_ROBOT && app.game.plies() > 0 && app.game.pos.side != app.humanSide) app.game.undo();
  app.anim[0].active = app.anim[1].active = false;
  app.sel = -1; app.aiState = 0; app.promoOpen = false; app.evalPly = -1; clearHint();
  if (app.game.plies()) { app.lastFrom = app.game.moves.back().from; app.lastTo = app.game.moves.back().to; }
  else app.lastFrom = app.lastTo = -1;
}

static void updateEval() {
  if (app.evalPly == app.game.plies()) return;
  app.evalPly = app.game.plies();
  Game &g = app.game;
  if (!app.evalOn && app.mode != M_LAN) return;
  if (g.over()) app.evalCp = g.result == R_WHITE ? 30000 : g.result == R_BLACK ? -30000 : 0;
  else app.evalCp = evalWhiteCp(g.pos, 30000);
  lanBroadcast();
}

static void requestHint() {
  if (!isHumanTurn() || app.game.over()) return;
  Move m = aiMove(app.game.pos, 4);
  if (m.isNull()) return;
  app.hintFrom = m.from; app.hintTo = m.to; app.hintFlip = app.flipped; app.hintUntil = app.now + 7000;
  buildArrow(m.from, m.to);
}

static void humanOfferDraw() {
  Game &g = app.game;
  if (g.over()) return;
  if (g.drawOffer >= 0) { showToast("A draw offer is already pending"); return; }
  if (app.mode == M_ROBOT) {
    int cp = evalWhiteCp(g.pos), robotCp = (app.humanSide ^ 1) == WHITE ? cp : -cp;
    g.offerDraw(app.humanSide);
    bool accept = robotCp <= 30;
    g.answerDraw(accept);
    showToast(accept ? "Robot accepted the draw" : "Robot declined the draw");
  } else if (app.mode == M_LOCAL) {
    g.offerDraw(g.pos.side);
    app.dlg = D_CONFIRM; app.confirmKind = C_DRAW_RECV; app.dlgSel = 1;
  } else {
    g.offerDraw(app.humanSide);
    showToast("Draw offer sent to the phone");
  }
  lanBroadcast();
}

static void openConfirm(int kind) { app.dlg = D_CONFIRM; app.confirmKind = kind; app.dlgSel = 1; }

static void confirmDone(bool yes) {
  Game &g = app.game;
  int kind = app.confirmKind;
  app.dlg = D_NONE;
  if (kind == C_RESIGN) { if (yes && !g.over()) { g.resign(app.mode == M_LOCAL ? g.pos.side : app.humanSide); lanBroadcast(); } }
  else if (kind == C_DRAW_RECV) {
    if (g.drawOffer >= 0 && !g.over()) { g.answerDraw(yes); if (!yes) showToast("Draw declined"); lanBroadcast(); }
  } else if (kind == C_MENU) { if (yes) toMenu(); }
}

static void onSelect() {
  Game &g = app.game;
  if (app.promoOpen) { applyMove(app.promoMoves[app.promoIdx]); app.promoOpen = false; return; }
  if (!isHumanTurn() || anyAnim()) return;
  int sq = squareAt(app.curCol, app.curRow), pc = g.pos.b[sq];
  if (app.sel >= 0) {
    Move L[256]; int n = g.legal(L);
    std::vector<Move> c;
    for (int i = 0; i < n; i++) if (L[i].from == app.sel && L[i].to == sq) c.push_back(L[i]);
    if (!c.empty()) {
      if (c.size() > 1) { app.promoOpen = true; app.promoMoves = c; app.promoIdx = 0; }
      else applyMove(c[0]);
      return;
    }
  }
  if (pc && pcolor(pc) == g.pos.side && hasMovesFrom(sq)) app.sel = (app.sel == sq) ? -1 : sq;
  else app.sel = -1;
}

static void onBack() {
  if (app.promoOpen) { app.promoOpen = false; return; }
  app.sel = -1; clearHint();
}

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
    case 4: app.theme = (app.theme + d + NTHEMES) % NTHEMES; break;
    case 5: app.hints = !app.hints; break;
    case 6: app.evalOn = !app.evalOn; break;
    case 7: if (app.mode == M_LOCAL) app.autoflip = !app.autoflip; break;
  }
}

static void updateMenu(int mx, int my) {
#ifdef ENABLE_LAN
  // the phone page is reachable as soon as LAN mode is selected (no need to press START first)
  if (app.mode == M_LAN) {
    lanEnsure();
    app.room.lockMsg = "Waiting for the game to start on the PS4...";
    LanEvents ev; app.room.update(app.game, ev);
  } else if (app.room.srv.running()) app.room.stop();
#endif
  if (my) app.menuSel = (app.menuSel + my + MENU_ROWS) % MENU_ROWS;
  if (mx) changeMenu(app.menuSel, mx);
  if (PRESSED(pad, BTN_SEL)) {
    if (app.menuSel == 9) startGame();
    else if (app.menuSel == 8) { app.screen = S_PADTEST; app.padTestStart = (int)app.now; }
    else changeMenu(app.menuSel, 1);
  }
}

static void updateDialog(int mx, int my) {
  if (app.dlg == D_PAUSE) {
    if (my) app.dlgSel = (app.dlgSel + my + 6) % 6;
    if (PRESSED(pad, BTN_BACK)) { app.dlg = D_NONE; return; }
    if (PRESSED(pad, BTN_SEL)) {
      switch (app.dlgSel) {
        case 0: app.dlg = D_NONE; break;
        case 1: app.dlg = D_NONE; requestHint(); break;
        case 2: app.dlg = D_NONE; humanOfferDraw(); break;
        case 3: openConfirm(C_RESIGN); break;
        case 4: app.flipped = !app.flipped; app.dlg = D_NONE; break;
        case 5: openConfirm(C_MENU); break;
      }
    }
  } else if (app.dlg == D_CONFIRM) {
    if (mx) app.dlgSel ^= 1;
    if (PRESSED(pad, BTN_BACK)) confirmDone(false);
    else if (PRESSED(pad, BTN_SEL)) confirmDone(app.dlgSel == 0);
  }
}

static void updateGame(int mx, int my) {
  Game &g = app.game;
  Uint32 now = app.now;
  Uint32 dt = now - app.lastFrame; if (dt > 1000) dt = 1000;

  for (int i = 0; i < 2; i++) if (app.anim[i].active && now - app.anim[i].t0 >= 230) app.anim[i].active = false;
  if (!anyAnim() && app.pendingFlip) { app.flipped = app.pendingFlipVal; app.pendingFlip = false; }
  if (app.hintFrom >= 0) {
    if (now > app.hintUntil) clearHint();
    else if (app.hintFlip != app.flipped) { app.hintFlip = app.flipped; buildArrow(app.hintFrom, app.hintTo); }
  }
  if (app.dlg != D_PAUSE && !g.over()) g.tick((int64_t)dt);

#ifdef ENABLE_LAN
  if (app.mode == M_LAN && app.room.srv.running()) {
    LanEvents ev;
    Pos before = g.pos;
    int pliesBefore = g.plies();
    bool wasConnected = app.room.connected();
    app.room.update(g, ev);
    if (ev.moved && g.plies() == pliesBefore + 1) { startAnim(before, g.moves.back()); app.room.last = toUci(g.moves.back()); app.sel = -1; clearHint(); }
    if (ev.newGameReq) { startRematch(); return; }
    if (ev.drawOffered && !g.over()) { app.promoOpen = false; openConfirm(C_DRAW_RECV); }
    if (ev.drawAnswered && !g.over()) showToast("Phone declined the draw");
    if (ev.connected && !wasConnected) showToast("Phone connected");
    if (ev.disconnected) showToast("Phone disconnected");
    if (ev.connected || ev.disconnected || ev.moved) lanBroadcast();
    if (g.drawOffer == app.room.phoneSide && app.dlg == D_NONE && !g.over()) openConfirm(C_DRAW_RECV);
    if (now - app.lastSync > 5000) { app.lastSync = now; lanBroadcast(); }
  }
#endif
  if (app.dlg == D_CONFIRM && app.confirmKind == C_DRAW_RECV && g.drawOffer < 0) app.dlg = D_NONE; // offer withdrawn or answered elsewhere
  if (g.over() && !app.prevOver) { app.prevOver = true; app.dlg = D_NONE; app.overSel = 0; clearHint(); lanBroadcast(); }
  if (!g.over()) app.prevOver = false;
  if (!anyAnim() && app.aiState < 2) updateEval();
  {
    float target = !app.evalOn && app.mode != M_LAN ? 0.5f : 1.f / (1.f + expf(-(float)std::max(-3000, std::min(3000, app.evalCp)) / 280.f));
    if (app.evalCp >= 29000) target = 1.f; if (app.evalCp <= -29000) target = 0.f;
    app.evalDisp += (target - app.evalDisp) * std::min(1.f, (float)dt / 220.f);
  }

  // ---- input
  if (app.dlg != D_NONE) { updateDialog(mx, my); return; }
  if (g.over()) {
    if (anyAnim()) return;
    if (mx) app.overSel ^= 1;
    if (PRESSED(pad, BTN_BACK)) { toMenu(); return; }
    if (PRESSED(pad, BTN_SEL)) { if (app.overSel == 0) startRematch(); else toMenu(); }
    return;
  }
  if (app.promoOpen) { if (mx) app.promoIdx = (app.promoIdx + mx + (int)app.promoMoves.size()) % (int)app.promoMoves.size(); }
  else if (mx || my) moveCursor(mx, my);
  if (PRESSED(pad, BTN_SEL)) onSelect();
  if (PRESSED(pad, BTN_BACK)) { app.backDownAt = now; app.backLong = false; }
  if (HELD(pad, BTN_BACK) && !app.backLong && app.backDownAt && now - app.backDownAt > 1000) { app.backLong = true; app.dlg = D_PAUSE; app.dlgSel = 0; }
  if (RELEASED(pad, BTN_BACK)) { if (!app.backLong) onBack(); app.backDownAt = 0; }
  if (PRESSED(pad, BTN_FLIP)) app.flipped = !app.flipped;
  if (PRESSED(pad, BTN_UNDO)) doUndo();
  if (PRESSED(pad, BTN_MENU)) { app.dlg = D_PAUSE; app.dlgSel = 0; }

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
static std::string clockText(int64_t ms, bool timed) {
  if (!timed) return "--:--";
  int64_t s = (ms + 999) / 1000; if (s < 0) s = 0;
  char b[32];
  if (ms < 10000 && ms > 0) snprintf(b, sizeof(b), "0:%02d.%d", (int)(ms / 1000), (int)((ms % 1000) / 100));
  else snprintf(b, sizeof(b), "%d:%02d", (int)(s / 60), (int)(s % 60));
  return b;
}

static std::string nameFor(int color) {
  char b[64];
  if (app.mode == M_LOCAL) return color == WHITE ? "White" : "Black";
  if (app.mode == M_ROBOT) {
    if (color == app.humanSide) return "You";
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
  static const int init[7] = {0, 8, 2, 2, 2, 1, 0}, val[7] = {0, 1, 3, 3, 5, 9, 0};
  int cntW[7] = {0}, cntB[7] = {0};
  for (int s = 0; s < 64; s++) { int pc = app.game.pos.b[s]; if (!pc) continue; (pcolor(pc) == WHITE ? cntW : cntB)[ptype(pc)]++; }
  diff = 0;
  for (int t = 1; t <= 5; t++) {
    capW[t] = std::max(0, init[t] - cntB[t]); capB[t] = std::max(0, init[t] - cntW[t]);
    diff += capW[t] * val[t] - capB[t] * val[t];
  }
}

static void drawPlayerCard(int color, int y) {
  Game &g = app.game;
  fillRound(BX, y, BOARD_PX, 80, 12, PANEL);
  fillRound(BX + 12, y + 12, 56, 56, 10, THEMES[app.theme].dark);
  drawPieceAt(mkp(KING, color), BX + 12, y + 12, 56);
  std::string nm = nameFor(color);
  drawText(BX + 84, y + 8, nm.c_str(), F_MB, TXT);
  int capW[7] = {0}, capB[7] = {0}, diff = 0; capturedInfo(capW, capB, diff);
  int *mine = color == WHITE ? capW : capB;
  int x = BX + 84;
  for (int t = 5; t >= 1; t--) for (int k = 0; k < mine[t]; k++) { drawPieceAt(mkp(t, color ^ 1), x, y + 40, 32); x += t == 1 ? 11 : 19; }
  int adv = color == WHITE ? diff : -diff;
  if (adv > 0) { char b[16]; snprintf(b, sizeof(b), "+%d", adv); drawText(x + 14, y + 44, b, F_S, SUB); }
  bool timed = g.initMs > 0;
  bool active = !g.over() && g.plies() > 0 && g.pos.side == color;
  bool low = timed && g.clk[color] < 20000;
  Col bgc = active ? (low ? RED : C(255, 255, 255)) : C(33, 32, 29);
  Col fgc = active ? (low ? C(255, 255, 255) : C(38, 37, 34)) : (low ? RED : TXT);
  fillRound(BX + BOARD_PX - 206, y + 12, 194, 56, 10, bgc);
  std::string ct = clockText(g.clk[color], timed);
  textC(BX + BOARD_PX - 109, y + 12 + (56 - lineH(F_L)) / 2, ct.c_str(), F_L, fgc);
}

static void drawEvalBar() {
  if (!app.evalOn && app.mode != M_LAN) return;
  int x = BX - 46, y = BY, w = 28, h = BOARD_PX;
  fillRound(x, y, w, h, 8, C(70, 67, 63));
  int whiteH = (int)(app.evalDisp * h + 0.5f);
  bool whiteBottom = !app.flipped;
  if (whiteH > 0) {
    int wy = whiteBottom ? y + h - whiteH : y;
    // keep the rounded outline: inset by 3px
    fillRect(x + 3, std::max(y + 3, wy), w - 6, std::min(whiteH, h - 6), C(245, 245, 245));
  }
  int cp = app.evalCp;
  char b[16];
  if (std::abs(cp) >= 29000) snprintf(b, sizeof(b), "M"); else snprintf(b, sizeof(b), "%.1f", std::abs(cp) / 100.0);
  bool whiteAhead = cp >= 0;
  bool atBottom = whiteAhead == whiteBottom;
  int ty = atBottom ? y + h - 28 : y + 6;
  textC(x + w / 2, ty, b, F_XS, whiteAhead ? C(40, 40, 38) : C(235, 235, 235));
}

static void drawBoard() {
  Game &g = app.game;
  const Theme &th = THEMES[app.theme];
  fillRound(BX - 8, BY - 8, BOARD_PX + 16, BOARD_PX + 16, 10, C(22, 21, 19));
  int kingSqr = g.check() ? kingSq(g.pos, g.pos.side) : -1;
  for (int row = 0; row < 8; row++) for (int col = 0; col < 8; col++) {
    int sq = squareAt(col, row), x = BX + col * CELL, y = BY + row * CELL;
    bool light = ((fileOf(sq) + rankOf(sq)) & 1) == 1;
    fillRect(x, y, CELL, CELL, light ? th.light : th.dark);
    if (sq == app.lastFrom || sq == app.lastTo) fillRect(x, y, CELL, CELL, C(255, 255, 50, 105));
    if (sq == app.sel) fillRect(x, y, CELL, CELL, C(20, 110, 40, 135));
    if (sq == kingSqr) drawDisc(x + CELL / 2, y + CELL / 2, CELL, CELL, C(235, 55, 45, 175), false);
    Col cc = light ? th.dark : th.light;
    if (row == 7) { char s[2] = {(char)('a' + fileOf(sq)), 0}; textR(x + CELL - 6, y + CELL - 24, s, F_XS, cc); }
    if (col == 0) { char s[2] = {(char)('1' + rankOf(sq)), 0}; drawText(x + 5, y + 2, s, F_XS, cc); }
  }
  for (int sq = 0; sq < 64; sq++) {
    int pc = g.pos.b[sq]; if (!pc) continue;
    bool skip = false;
    for (int i = 0; i < 2; i++) if (app.anim[i].active && app.anim[i].to == sq) skip = true;
    if (skip) continue;
    int x, y; sqPix(sq, x, y);
    drawPieceAt(pc, x, y, CELL, 255, true);
  }
  if (app.hints && app.sel >= 0 && !app.promoOpen) {
    Move L[256]; int n = g.legal(L);
    for (int i = 0; i < n; i++) if (L[i].from == app.sel) {
      int x, y; sqPix(L[i].to, x, y);
      if (g.pos.b[L[i].to] || (L[i].flags & F_EP)) drawDisc(x + CELL / 2, y + CELL / 2, CELL - 4, CELL - 4, C(20, 85, 0, 105), true);
      else drawDisc(x + CELL / 2, y + CELL / 2, 32, 32, C(20, 85, 0, 100), false);
    }
  }
  if (app.hintFrom >= 0 && texArrow) {
    SDL_Rect dst; dst.x = BX; dst.y = BY; dst.w = BOARD_PX; dst.h = BOARD_PX;
    SDL_SetTextureAlphaMod(texArrow, 255);
    SDL_RenderCopy(R, texArrow, 0, &dst);
  }
  for (int i = 0; i < 2; i++) if (app.anim[i].active) {
    float p = (float)(app.now - app.anim[i].t0) / 230.f; if (p > 1) p = 1;
    float e = 1.f - (1.f - p) * (1.f - p) * (1.f - p);
    int x0, y0, x1, y1; sqPix(app.anim[i].from, x0, y0); sqPix(app.anim[i].to, x1, y1);
    if (app.anim[i].cap && p < 0.8f) { int cx, cy; sqPix(app.anim[i].to, cx, cy); drawPieceAt(app.anim[i].cap, cx, cy, CELL, (int)(255 * (1.f - p / 0.8f))); }
    drawPieceAt(app.anim[i].piece, x0 + (int)((x1 - x0) * e), y0 + (int)((y1 - y0) * e), CELL, 255, true);
  }
  if (!g.over() && app.dlg == D_NONE) {
    int x = BX + app.curCol * CELL, y = BY + app.curRow * CELL;
    Col cc = isHumanTurn() ? GOLD : C(150, 150, 150);
    frameRect(x, y, CELL, CELL, 5, cc);
  }
}

static void statusText(std::string &s, std::string &sub, Col &c) {
  Game &g = app.game;
  c = TXT; sub.clear();
  char b[96];
  if (app.mode == M_ROBOT) snprintf(b, sizeof(b), "Robot - %s  |  %s", LEVELS[app.level].name, TIMES[app.timeIdx].name);
  else if (app.mode == M_LAN) snprintf(b, sizeof(b), "Phone (LAN)  |  %s", TIMES[app.timeIdx].name);
  else snprintf(b, sizeof(b), "Local game  |  %s", TIMES[app.timeIdx].name);
  sub = b;
  if (g.over()) { s = g.resultText(); c = GOLD; return; }
  std::string chk = g.check() ? "Check! " : "";
  if (app.mode == M_ROBOT) s = isAiTurn() ? "Robot is thinking" : chk + "Your move";
  else if (app.mode == M_LAN) {
#ifdef ENABLE_LAN
    s = isHumanTurn() ? chk + "Your move" : (app.room.connected() ? chk + "Phone to move" : "Waiting for phone");
#else
    s = "LAN unavailable";
#endif
  } else s = chk + (g.pos.side == WHITE ? "White to move" : "Black to move");
  if (g.check()) c = C(255, 140, 130);
}

static void chip(int x, int y, const char *key, const char *label) {
  int w = textW(key, F_XS) + 22;
  fillRound(x, y, std::max(w, 44), 32, 8, LINE);
  textC(x + std::max(w, 44) / 2, y + 3, key, F_XS, TXT);
  drawText(x + std::max(w, 44) + 14, y + 1, label, F_S, SUB);
}

static void drawLeft() {
  Game &g = app.game;
  fillRound(40, 124, 440, 150, 14, PANEL);
  std::string st, sub; Col sc; statusText(st, sub, sc);
  drawText(64, 144, st.c_str(), F_L, sc);
  drawText(64, 192, sub.c_str(), F_S, MUT);
  char mv[48]; snprintf(mv, sizeof(mv), "Move %d", g.pos.full); drawText(64, 226, mv, F_S, SUB);
  if (isAiTurn() && !g.over()) {
    for (int i = 0; i < 3; i++) { int a = 90 + (int)(165 * (0.5f + 0.5f * sinf((float)app.now / 180.f - i * 0.9f))); drawDisc(64 + textW(st.c_str(), F_L) + 20 + i * 18, 162 + 14, 10, 10, C(255, 255, 255, a), false); }
  }
  int y = 292;
#ifdef ENABLE_LAN
  if (app.mode == M_LAN) {
    ensureQr();
    fillRound(40, y, 440, 338, 14, PANEL);
    int qrN = app.qrc.n, mod = qrN ? 190 / qrN : 0;
    if (qrN) {
      int qs = mod * qrN, qx = 40 + 20, qy = y + 20;
      fillRound(qx - 8, qy - 8, qs + 16, qs + 16, 8, C(255, 255, 255));
      for (int yy = 0; yy < qrN; yy++) for (int xx = 0; xx < qrN; xx++) if (app.qrc.dark(xx, yy)) fillRect(qx + xx * mod, qy + yy * mod, mod, mod, C(20, 20, 20));
    }
    drawText(40 + 250, y + 22, "Scan to play", F_MB, TXT);
    drawText(40 + 250, y + 60, "Phone and PS4", F_S, SUB); drawText(40 + 250, y + 86, "on the same Wi-Fi", F_S, SUB);
    std::string url = lanUrl();
    drawText(64, y + 240, url.empty() ? "<PS4 IP>:8080" : url.c_str(), F_S, C(120, 200, 255));
    bool on = app.room.connected();
    drawDisc(72, y + 290, 14, 14, on ? GREEN : MUT, false);
    std::string stt = !app.lanError.empty() ? "Server: " + app.lanError : on ? "Phone connected" : "Waiting for phone...";
    drawText(92, y + 276, stt.c_str(), F_S, on ? GREEN : (!app.lanError.empty() ? RED : MUT));
    y += 354;
  }
#endif
  fillRound(40, y, 440, 956 - y + 0, 14, PANEL);
  drawText(64, y + 14, "Controls", F_MB, TXT);
  int ty = y + 60;
  chip(64, ty, "D-pad", "Move cursor"); ty += 42;
  chip(64, ty, "X", "Select / move"); ty += 42;
  chip(64, ty, "O", "Cancel"); ty += 42;
  chip(64, ty, "Hold O", "Menu"); ty += 42;
  chip(64, ty, "SQ", "Flip board"); ty += 42;
  chip(64, ty, "TRI", "Undo"); ty += 42;
}

static void drawRight() {
  Game &g = app.game;
  fillRound(1440, 124, 440, 832, 14, PANEL);
  drawText(1466, 140, "Moves", F_MB, TXT);
  int total = (int)g.san.size(), rows = (total + 1) / 2, maxRows = 19, first = std::max(0, rows - maxRows);
  int ry = 192;
  for (int r = first; r < rows; r++) {
    if ((r & 1) == 0) fillRect(1452, ry - 4, 416, 38, C(43, 41, 38));
    char num[16]; snprintf(num, sizeof(num), "%d.", r + 1);
    drawText(1466, ry, num, F_S, MUT);
    bool lastW = (2 * r == total - 1), lastB = (2 * r + 1 == total - 1);
    if (lastW) fillRound(1522, ry - 3, 150, 34, 8, GREEN_D);
    drawText(1534, ry, g.san[2 * r].c_str(), F_M, TXT);
    if (2 * r + 1 < total) { if (lastB) fillRound(1690, ry - 3, 150, 34, 8, GREEN_D); drawText(1702, ry, g.san[2 * r + 1].c_str(), F_M, TXT); }
    ry += 38;
  }
  if (total == 0) drawText(1466, 200, "No moves yet", F_S, MUT);
}

static void panelBox(int x, int y, int w, int h) {
  fillRect(0, 0, SCR_W, SCR_H, C(0, 0, 0, 140));
  fillRound(x - 4, y - 4, w + 8, h + 8, 18, C(20, 19, 17));
  fillRound(x, y, w, h, 16, PANEL);
}

static void button(int x, int y, int w, int h, const char *label, bool sel, bool primary) {
  Col bg = primary ? GREEN : C(70, 67, 63);
  if (sel) fillRound(x - 5, y - 5, w + 10, h + 10, 14, GOLD);
  fillRound(x, y, w, h, 10, bg);
  textC(x + w / 2, y + (h - lineH(F_MB)) / 2, label, F_MB, TXT);
}

static void drawOverlays() {
  Game &g = app.game;
  if (app.promoOpen) {
    int w = 4 * 130 + 40, x = SCR_W / 2 - w / 2, y = 420;
    panelBox(x, y, w, 230);
    textC(SCR_W / 2, y + 16, "Promote to", F_MB, TXT);
    for (int i = 0; i < (int)app.promoMoves.size(); i++) {
      int px = x + 20 + i * 130, py = y + 80;
      if (i == app.promoIdx) fillRound(px - 5, py - 5, 130, 130, 14, GOLD);
      fillRound(px, py, 120, 120, 10, THEMES[app.theme].dark);
      drawPieceAt(mkp(app.promoMoves[i].promo, g.pos.side), px + 8, py + 8, 104);
    }
  }
  if (g.over() && !anyAnim() && app.dlg == D_NONE) {
    int w = 720, x = SCR_W / 2 - w / 2, y = 360;
    panelBox(x, y, w, 360);
    std::string t = g.resultText();
    if (app.mode != M_LOCAL && g.result != R_DRAW) t = (g.result == R_WHITE) == (app.humanSide == WHITE) ? "You Won!" : "You Lost";
    textC(SCR_W / 2, y + 36, t.c_str(), F_XL, g.result == R_DRAW ? TXT : GOLD);
    std::string why = g.result == R_DRAW ? "Draw by " + g.reason : (g.result == R_WHITE ? "White" : "Black") + std::string(" won by ") + g.reason;
    textC(SCR_W / 2, y + 120, why.c_str(), F_M, SUB);
    button(x + 50, y + 220, 290, 80, "Rematch", app.overSel == 0, true);
    button(x + w - 340, y + 220, 290, 80, "Main menu", app.overSel == 1, false);
  }
  if (app.dlg == D_PAUSE) {
    int w = 560, h = 6 * 74 + 110, x = SCR_W / 2 - w / 2, y = (SCR_H - h) / 2;
    panelBox(x, y, w, h);
    textC(SCR_W / 2, y + 22, "Game Paused", F_L, TXT);
    const char *it[6] = {"Resume", "Show hint", "Offer a draw", "Resign", "Flip board", "Main menu"};
    for (int i = 0; i < 6; i++) {
      int ry = y + 96 + i * 74;
      fillRound(x + 24, ry, w - 48, 62, 12, i == app.dlgSel ? GREEN_D : C(70, 67, 63));
      if (i == app.dlgSel) frameRect(x + 24, ry, w - 48, 62, 0, GOLD);
      textC(SCR_W / 2, ry + (62 - lineH(F_M)) / 2, it[i], F_M, TXT);
    }
  } else if (app.dlg == D_CONFIRM) {
    int w = 780, x = SCR_W / 2 - w / 2, y = 380;
    panelBox(x, y, w, 320);
    std::string title, line2; const char *yes = "Yes", *no = "No";
    if (app.confirmKind == C_RESIGN) { title = "Resign this game?"; line2 = "Your opponent will win."; }
    else if (app.confirmKind == C_MENU) { title = "Leave this game?"; line2 = "The current game will be lost."; }
    else {
      std::string who = app.mode == M_LAN ? "The phone" : (g.drawOffer == WHITE ? "White" : "Black");
      title = who + " offers a draw"; line2 = app.mode == M_LOCAL ? std::string(g.drawOffer == WHITE ? "Black" : "White") + ": do you accept?" : "Do you accept?";
      yes = "Accept"; no = "Decline";
    }
    textC(SCR_W / 2, y + 36, title.c_str(), F_L, TXT);
    textC(SCR_W / 2, y + 100, line2.c_str(), F_M, SUB);
    button(x + 50, y + 190, 300, 80, yes, app.dlgSel == 0, true);
    button(x + w - 350, y + 190, 300, 80, no, app.dlgSel == 1, false);
  }
  if (app.toastUntil > app.now && !app.toast.empty()) {
    int w = textW(app.toast.c_str(), F_M) + 60;
    fillRound(SCR_W / 2 - w / 2, 14, w, 56, 28, C(20, 19, 17));
    textC(SCR_W / 2, 14 + (56 - lineH(F_M)) / 2, app.toast.c_str(), F_M, TXT);
  }
}

static void drawGame() {
  fillRect(0, 0, SCR_W, SCR_H, BG);
  int bottom = app.flipped ? BLACK : WHITE;
  drawPlayerCard(bottom ^ 1, 36);
  drawPlayerCard(bottom, BY + BOARD_PX + 8);
  drawEvalBar();
  drawBoard();
  drawLeft();
  drawRight();
  drawOverlays();
}

static void drawMenu() {
  fillRect(0, 0, SCR_W, SCR_H, BG);
  drawPieceAt(mkp(KING, WHITE), 112, 52, 120);
  drawText(250, 56, "PS4 Chess", F_T, TXT);
  drawText(254, 140, "Play against a friend, the robot, or your phone", F_S, MUT);
  // left card: board preview or QR code
  fillRound(112, 250, 500, 640, 18, PANEL);
#ifdef ENABLE_LAN
  if (app.mode == M_LAN) {
    ensureQr();
    int qrN = app.qrc.n, mod = qrN ? 300 / qrN : 0;
    if (qrN) {
      int qs = mod * qrN, qx = 112 + (500 - qs) / 2, qy = 290;
      fillRound(qx - 14, qy - 14, qs + 28, qs + 28, 12, C(255, 255, 255));
      for (int yy = 0; yy < qrN; yy++) for (int xx = 0; xx < qrN; xx++) if (app.qrc.dark(xx, yy)) fillRect(qx + xx * mod, qy + yy * mod, mod, mod, C(20, 20, 20));
    } else textC(112 + 250, 400, "Getting IP address...", F_M, MUT);
    textC(112 + 250, 650, "Scan with your phone camera", F_MB, TXT);
    std::string url = lanUrl();
    textC(112 + 250, 700, url.empty() ? "http://<PS4 IP>:8080" : url.c_str(), F_M, C(120, 200, 255));
    std::string st = !app.lanError.empty() ? "Server error: " + app.lanError :
      app.room.srv.running() ? (app.room.connected() ? "Phone connected" : "Server ready - waiting for phone") :
      std::string("Starting server: ") + net::Server::stageName(app.room.srv.stage());
    bool good = app.lanError.empty() && app.room.srv.running();
    drawDisc(112 + 40, 790, 16, 16, !app.lanError.empty() ? RED : good ? GREEN : GOLD, false);
    drawText(112 + 68, 774, st.c_str(), F_S, SUB);
    drawText(112 + 40, 820, "Or open the Android app - it finds this PS4 itself", F_S, MUT);
  } else
#endif
  {
    const Theme &th = THEMES[app.theme];
    const int cs = 58, px = 112 + (500 - 8 * cs) / 2, py = 282;
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) fillRect(px + c * cs, py + r * cs, cs, cs, ((r + c) & 1) ? th.dark : th.light);
    const char *back = "RNBQKBNR";
    for (int c = 0; c < 8; c++) {
      int t[4] = {ROOK, KNIGHT, BISHOP, QUEEN}; int kk = c < 4 ? t[c] : (c == 4 ? KING : c == 5 ? BISHOP : c == 6 ? KNIGHT : ROOK);
      (void)back;
      drawPieceAt(mkp(kk, BLACK), px + c * cs, py, cs); drawPieceAt(mkp(PAWN, BLACK), px + c * cs, py + cs, cs);
      drawPieceAt(mkp(PAWN, WHITE), px + c * cs, py + 6 * cs, cs); drawPieceAt(mkp(kk, WHITE), px + c * cs, py + 7 * cs, cs);
    }
    char b[64]; snprintf(b, sizeof(b), "Board: %s", th.name);
    textC(112 + 250, 780, b, F_M, SUB);
    std::string tcs = std::string(TIMES[app.timeIdx].kind) + (TIMES[app.timeIdx].kind[0] ? "  " : "") + TIMES[app.timeIdx].name;
    textC(112 + 250, 826, tcs.c_str(), F_S, MUT);
  }
  // settings rows
  const char *labels[MENU_ROWS] = {"Game mode", "Play as", "Robot level", "Time control", "Board theme", "Legal move hints", "Evaluation bar", "Auto-flip board", "Controller test", "Play"};
  std::string vals[MENU_ROWS];
  vals[0] = MODE_NAMES[app.mode];
  vals[1] = app.mode == M_LOCAL ? "-" : SIDE_NAMES[app.side];
  vals[2] = app.mode == M_ROBOT ? LEVELS[app.level].name : "-";
  vals[3] = std::string(TIMES[app.timeIdx].kind) + (TIMES[app.timeIdx].kind[0] ? "  " : "") + TIMES[app.timeIdx].name;
  vals[4] = THEMES[app.theme].name;
  vals[5] = app.hints ? "On" : "Off";
  vals[6] = app.evalOn ? "On" : "Off";
  vals[7] = app.mode == M_LOCAL ? (app.autoflip ? "On" : "Off") : "-";
  drawText(700, 176, "New Game", F_L, TXT);
  int x = 700, w = 1100, y0 = 244, rh = 70;
  for (int i = 0; i < MENU_ROWS; i++) {
    int y = y0 + i * rh;
    bool s = i == app.menuSel;
    if (i == 9) {
      if (s) fillRound(x - 5, y - 5 + 6, w + 10, 62 + 10, 16, GOLD);
      fillRound(x, y + 6, w, 62, 12, GREEN);
      textC(x + w / 2, y + 6 + (62 - lineH(F_L)) / 2, labels[i], F_L, TXT);
      continue;
    }
    fillRound(x, y, w, 62, 12, s ? C(70, 67, 63) : PANEL);
    if (s) fillRound(x, y, 8, 62, 4, GREEN);
    drawText(x + 32, y + (62 - lineH(F_M)) / 2, labels[i], F_M, s ? TXT : SUB);
    if (i == 8) { textR(x + w - 32, y + (62 - lineH(F_M)) / 2, "Open", F_MB, s ? TXT : MUT); continue; }
    std::string v = vals[i];
    bool na = v == "-";
    Col vc = na ? C(100, 99, 96) : (s ? TXT : SUB);
    int vxr = x + w - 32 - (s && !na ? 38 : 0); // right edge of the value text (arrows sit to its sides when selected)
    if (i == 4) { const Theme &th = THEMES[app.theme]; fillRound(vxr - textW(v.c_str(), F_MB) - 84, y + 14, 34, 34, 6, th.light); fillRound(vxr - textW(v.c_str(), F_MB) - 46, y + 14, 34, 34, 6, th.dark); }
    textR(vxr, y + (62 - lineH(F_M)) / 2, v.c_str(), F_MB, vc);
    if (s && !na && i != 8) { textR(x + w - 32, y + (62 - lineH(F_M)) / 2, ">", F_MB, GREEN); drawText(vxr - textW(v.c_str(), F_MB) - (i == 4 ? 128 : 34), y + (62 - lineH(F_M)) / 2, "<", F_MB, GREEN); }
  }
  chip(112, 1000, "D-pad", "Choose / change"); chip(480, 1000, "X", "Confirm");
  char fb[80]; snprintf(fb, sizeof(fb), "button %d   frame %u", pad.lastBtn, (unsigned)g_frames);
  textR(1800, 1004, fb, F_XS, C(110, 109, 106));
}

static void drawPadTest() {
  fillRect(0, 0, SCR_W, SCR_H, BG);
  textC(SCR_W / 2, 50, "Controller test", F_XL, TXT);
  textC(SCR_W / 2, 130, "Press buttons - their numbers light up. Press button 1 (O) to exit.", F_M, MUT);
  for (int i = 0; i < 32; i++) {
    int x = 360 + (i % 8) * 150, y = 220 + (i / 8) * 110;
    bool on = pad.btn[i];
    fillRound(x, y, 130, 90, 12, on ? GREEN : PANEL);
    char b[8]; snprintf(b, sizeof(b), "%d", i); textC(x + 65, y + 22, b, F_L, on ? TXT : MUT);
  }
  char s[160];
  snprintf(s, sizeof(s), "joystick: %s    buttons: %d    axes: %d", pad.j ? "found" : "NOT FOUND", pad.nb, pad.na);
  drawText(360, 700, s, F_M, TXT);
  snprintf(s, sizeof(s), "hat: %d    axis0: %d   axis1: %d   axis2: %d   axis3: %d", pad.hat, pad.ax[0], pad.ax[1], pad.ax[2], pad.ax[3]);
  drawText(360, 750, s, F_M, TXT);
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
    while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) run = false; pad.onEvent(e); }
    app.now = SDL_GetTicks();
    int mx, my;
    pad.poll(app.now, mx, my);
    if (app.screen == S_MENU) updateMenu(mx, my);
    else if (app.screen == S_GAME) updateGame(mx, my);
    else if (app.screen == S_PADTEST) {
      if (PRESSED(pad, BTN_BACK) || (int)app.now - app.padTestStart > 90000) app.screen = S_MENU;
    }
    app.lastFrame = app.now;
    g_frames++;
    if (app.screen == S_MENU) drawMenu();
    else if (app.screen == S_GAME) drawGame();
    else drawPadTest();
    SDL_RenderPresent(R);
    SDL_Delay(1);
  }
  SDL_Quit();
  return 0;
}
