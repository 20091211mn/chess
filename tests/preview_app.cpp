// Renders the real PS4 UI with a tiny software SDL so layouts can be inspected as images.
// build: g++ -std=c++11 -O2 -w -Itests/stub -Icore -Ips4 tests/preview_app.cpp -o preview && ./preview outdir
#define ENABLE_LAN 1
#include <SDL2/SDL.h>
#include <stdio.h>
#include <functional>
#include <vector>
#include <string>
#include <math.h>
#include <string.h>

struct Tex { int w, h; std::vector<uint32_t> px; int cm[3]; int am; };
static std::vector<uint32_t> fb(1920 * 1080);
static Uint8 dr = 0, dg = 0, db = 0, da = 255;
static int g_frame = 0, g_btn[32], g_hat = 0, g_quit = 0; static std::string outdir = "/tmp", dumpName; static bool dumpNow = false;

int SDL_Init(uint32_t) { return 0; } void SDL_Quit(void) {}
int SDL_NumJoysticks(void) { return 1; } SDL_Joystick *SDL_JoystickOpen(int) { return (SDL_Joystick *)1; } void SDL_JoystickUpdate(void) {}
int SDL_JoystickNumButtons(SDL_Joystick *) { return 16; } int SDL_JoystickNumAxes(SDL_Joystick *) { return 4; } int SDL_JoystickNumHats(SDL_Joystick *) { return 1; }
Uint8 SDL_JoystickGetButton(SDL_Joystick *, int i) { return (Uint8)g_btn[i]; } Sint16 SDL_JoystickGetAxis(SDL_Joystick *, int) { return 0; } Uint8 SDL_JoystickGetHat(SDL_Joystick *, int) { return (Uint8)g_hat; }
SDL_Window *SDL_CreateWindow(const char *, int, int, int, int, uint32_t) { return (SDL_Window *)1; }
SDL_Renderer *SDL_CreateRenderer(SDL_Window *, int, uint32_t) { return (SDL_Renderer *)1; }
SDL_Texture *SDL_CreateTexture(SDL_Renderer *, uint32_t, int, int w, int h) { Tex *t = new Tex; t->w = w; t->h = h; t->px.assign((size_t)w * h, 0); t->cm[0] = t->cm[1] = t->cm[2] = 255; t->am = 255; return (SDL_Texture *)t; }
int SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *, const void *p, int pitch) { Tex *x = (Tex *)t; for (int y = 0; y < x->h; y++) memcpy(&x->px[(size_t)y * x->w], (const uint8_t *)p + (size_t)y * pitch, (size_t)x->w * 4); return 0; }
int SDL_SetTextureBlendMode(SDL_Texture *, int) { return 0; }
int SDL_SetTextureColorMod(SDL_Texture *t, Uint8 r, Uint8 g, Uint8 b) { Tex *x = (Tex *)t; x->cm[0] = r; x->cm[1] = g; x->cm[2] = b; return 0; }
int SDL_SetTextureAlphaMod(SDL_Texture *t, Uint8 a) { ((Tex *)t)->am = a; return 0; }
static inline void blend(int x, int y, int r, int g, int b, int a) {
  if (x < 0 || y < 0 || x >= 1920 || y >= 1080 || a <= 0) return;
  uint32_t &d = fb[(size_t)y * 1920 + x];
  int R = (d >> 16) & 255, G = (d >> 8) & 255, B = d & 255;
  R = (r * a + R * (255 - a)) / 255; G = (g * a + G * (255 - a)) / 255; B = (b * a + B * (255 - a)) / 255;
  d = 0xFF000000u | (R << 16) | (G << 8) | B;
}
int SDL_RenderCopy(SDL_Renderer *, SDL_Texture *t, const SDL_Rect *src, const SDL_Rect *dst) {
  Tex *x = (Tex *)t; SDL_Rect s = src ? *src : SDL_Rect{0, 0, x->w, x->h}; SDL_Rect d = *dst;
  for (int j = 0; j < d.h; j++) for (int i = 0; i < d.w; i++) {
    int sx = s.x + i * s.w / d.w, sy = s.y + j * s.h / d.h;
    uint32_t p = x->px[(size_t)sy * x->w + sx];
    int a = ((p >> 24) & 255) * x->am / 255;
    blend(d.x + i, d.y + j, (((p >> 16) & 255) * x->cm[0]) / 255, (((p >> 8) & 255) * x->cm[1]) / 255, ((p & 255) * x->cm[2]) / 255, a);
  }
  return 0;
}
int SDL_SetRenderDrawBlendMode(SDL_Renderer *, int) { return 0; }
int SDL_SetRenderDrawColor(SDL_Renderer *, Uint8 r, Uint8 g, Uint8 b, Uint8 a) { dr = r; dg = g; db = b; da = a; return 0; }
int SDL_RenderFillRect(SDL_Renderer *, const SDL_Rect *q) { for (int y = q->y; y < q->y + q->h; y++) for (int x = q->x; x < q->x + q->w; x++) blend(x, y, dr, dg, db, da); return 0; }
void SDL_RenderPresent(SDL_Renderer *) {
  if (!dumpNow) return; dumpNow = false;
  std::string path = outdir + "/" + dumpName + ".ppm"; FILE *f = fopen(path.c_str(), "wb"); fprintf(f, "P6\n1920 1080\n255\n");
  for (size_t i = 0; i < fb.size(); i++) { fputc((fb[i] >> 16) & 255, f); fputc((fb[i] >> 8) & 255, f); fputc(fb[i] & 255, f); } fclose(f);
}
Uint32 SDL_GetTicks(void) { return (Uint32)g_frame * 16; }
int SDL_PollEvent(SDL_Event *e) { if (g_quit) { e->type = SDL_QUIT; g_quit = 0; return 1; } return 0; }

struct Act { int frame; int kind; int arg; std::function<void()> fn; };
static std::vector<Act> acts; static int TT = 3, lastFrameOfScript = 0;
static void tap(int b) { acts.push_back({TT, 0, b, 0}); acts.push_back({TT + 1, 1, b, 0}); TT += 4; }
static void hold(int b, int frames) { acts.push_back({TT, 0, b, 0}); acts.push_back({TT + frames, 1, b, 0}); TT += frames + 3; }
static void dir(int d) { acts.push_back({TT, 2, d, 0}); acts.push_back({TT + 1, 2, 0, 0}); TT += 4; }
static void wait(int n) { TT += n; }
static void check(std::function<void()> f) { acts.push_back({TT, 3, 0, f}); TT += 2; }
static std::vector<std::string> shotNames;
static void shot(const char *name) { std::string n = name; acts.push_back({TT, 3, 0, [n] { dumpName = n; dumpNow = true; }}); TT += 3; }
#define UP SDL_HAT_UP
#define DOWN SDL_HAT_DOWN
#define LEFT SDL_HAT_LEFT
#define RIGHT SDL_HAT_RIGHT
void SDL_Delay(Uint32) {
  g_frame++;
  for (size_t i = 0; i < acts.size(); i++) if (acts[i].frame == g_frame) { Act &a = acts[i]; if (a.kind == 0) g_btn[a.arg] = 1; else if (a.kind == 1) g_btn[a.arg] = 0; else if (a.kind == 2) g_hat = a.arg; else a.fn(); }
  if (g_frame >= lastFrameOfScript) g_quit = 1;
}
#define main app_main
#include "../ps4/main.cpp"
#undef main

int main(int argc, char **argv) {
  if (argc > 1) outdir = argv[1];
  wait(3); shot("menu_robot");
  check([] { app.menuSel = 4; app.theme = 1; app.timeIdx = 3; }); wait(3); shot("menu_theme");
  check([] { app.menuSel = 0; app.mode = M_LAN; }); wait(25); shot("menu_lan");
  check([] { app.mode = M_ROBOT; app.level = 0; app.menuSel = 9; app.theme = 0; app.timeIdx = 3; }); wait(3);
  tap(0); wait(3);
  check([] { const char *mv[] = {"e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5a4","g8f6","e1g1","f8e7","f1e1","b7b5","a4b3","d7d6","c2c3","e8g8"};
    for (int i = 0; i < 16; i++) app.game.playUci(mv[i]); app.lastFrom = 60; app.lastTo = 62; app.humanSide = WHITE; app.aiState = 0; app.evalPly = -1; app.game.clk[0] = 512000; app.game.clk[1] = 487000; });
  wait(20);
  check([] { app.curCol = 3; app.curRow = 6; });
  tap(0); wait(8); shot("game_selected");
  tap(1); check([] { app.game.playUci("h2h3"); app.lastFrom = 15; app.lastTo = 23; }); wait(40);
  check([] { app.game.playUci("c8b7"); }); wait(10);
  hold(1, 80); wait(5); shot("pause");
  tap(1); wait(3);
  hold(1, 80); dir(DOWN); tap(0); wait(8); shot("game_hint");
  check([] { app.game.playUci("d2d4"); app.game.resign(BLACK); }); wait(40); shot("game_over");
  check([] { app.dlg = D_NONE; app.game.loadFen("r3k2r/pppq1ppp/2npbn2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w kq - 0 8"); app.prevOver = false; app.promoOpen = false; app.mode = M_LOCAL; app.humanSide = WHITE; app.game.drawOffer = -1; });
  wait(5); check([] { app.game.offerDraw(BLACK); app.dlg = D_CONFIRM; app.confirmKind = C_DRAW_RECV; app.dlgSel = 1; }); wait(5); shot("draw_dialog");
  lastFrameOfScript = TT + 5;
  app_main(0, 0);
  return 0;
}
