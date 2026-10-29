// Counts draw calls per loop iteration in a LAN game (idle, typical position). build: g++ -std=c++11 -O2 -w -Itests/stub -Icore -Ips4 tests/perf_app.cpp
#define ENABLE_LAN 1
#include <SDL2/SDL.h>
#include <stdio.h>
#include <functional>
#include <vector>
#include <string.h>
static long copies = 0, fills = 0, presents = 0, uploads = 0; static int g_frame = 0, g_btn[32], g_hat = 0, g_quit = 0;
int SDL_Init(uint32_t) { return 0; } void SDL_Quit(void) {} int SDL_NumJoysticks(void) { return 1; } SDL_Joystick *SDL_JoystickOpen(int) { return (SDL_Joystick *)1; } void SDL_JoystickUpdate(void) {}
int SDL_JoystickNumButtons(SDL_Joystick *) { return 16; } int SDL_JoystickNumAxes(SDL_Joystick *) { return 4; } int SDL_JoystickNumHats(SDL_Joystick *) { return 1; }
Uint8 SDL_JoystickGetButton(SDL_Joystick *, int i) { return (Uint8)g_btn[i]; } Sint16 SDL_JoystickGetAxis(SDL_Joystick *, int) { return 0; } Uint8 SDL_JoystickGetHat(SDL_Joystick *, int) { return (Uint8)g_hat; }
SDL_Window *SDL_CreateWindow(const char *, int, int, int, int, uint32_t) { return (SDL_Window *)1; } SDL_Renderer *SDL_CreateRenderer(SDL_Window *, int, uint32_t) { return (SDL_Renderer *)1; }
SDL_Texture *SDL_CreateTexture(SDL_Renderer *, uint32_t, int, int, int) { return (SDL_Texture *)1; } void SDL_DestroyTexture(SDL_Texture *) {}
int SDL_UpdateTexture(SDL_Texture *, const SDL_Rect *, const void *, int) { uploads++; return 0; } int SDL_SetTextureBlendMode(SDL_Texture *, int) { return 0; }
int SDL_SetTextureColorMod(SDL_Texture *, Uint8, Uint8, Uint8) { return 0; } int SDL_SetTextureAlphaMod(SDL_Texture *, Uint8) { return 0; }
int SDL_RenderCopy(SDL_Renderer *, SDL_Texture *, const SDL_Rect *, const SDL_Rect *) { copies++; return 0; } int SDL_SetRenderDrawBlendMode(SDL_Renderer *, int) { return 0; }
int SDL_SetRenderDrawColor(SDL_Renderer *, Uint8, Uint8, Uint8, Uint8) { return 0; } int SDL_RenderFillRect(SDL_Renderer *, const SDL_Rect *) { fills++; return 0; }
void SDL_RenderPresent(SDL_Renderer *) { presents++; } Uint32 SDL_GetTicks(void) { return (Uint32)g_frame * 8; } int SDL_PollEvent(SDL_Event *e) { if (g_quit) { e->type = SDL_QUIT; g_quit = 0; return 1; } return 0; }
struct Act { int frame; int kind; int arg; std::function<void()> fn; };
static std::vector<Act> acts; static int TT = 3, lastFrame = 0;
static void tap(int b) { acts.push_back({TT, 0, b, 0}); acts.push_back({TT + 1, 1, b, 0}); TT += 4; }
static void check(std::function<void()> f) { acts.push_back({TT, 3, 0, f}); TT += 2; }
static void wait(int n) { TT += n; }
static long c0, f0, p0, u0; static int f_at;
void SDL_Delay(Uint32) { g_frame++; for (size_t i = 0; i < acts.size(); i++) if (acts[i].frame == g_frame) { Act &a = acts[i]; if (a.kind == 0) g_btn[a.arg] = 1; else if (a.kind == 1) g_btn[a.arg] = 0; else a.fn(); } if (g_frame >= lastFrame) g_quit = 1; }
#define main app_main
#include "../ps4/main.cpp"
#undef main
int main() {
  check([] { app.menuSel = 0; app.mode = M_LAN; app.timeIdx = 3; app.menuSel = 9; }); wait(30); tap(0); wait(10);
  check([] { const char *mv[] = {"e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5a4","g8f6","e1g1","f8e7","f1e1","b7b5","a4b3","d7d6","c2c3","e8g8"}; for (int i = 0; i < 16; i++) app.game.playUci(mv[i]); app.evalPly = -1; });
  wait(60);
  check([] { c0 = copies; f0 = fills; p0 = presents; u0 = uploads; f_at = g_frame; });
  wait(400);
  check([] { int n = g_frame - f_at; printf("idle LAN game, %d loop iterations: draw calls/iteration = %.1f (copies %.1f + fills %.1f), presents = %ld, texture uploads = %ld\n", n, (double)(copies - c0 + fills - f0) / n, (double)(copies - c0) / n, (double)(fills - f0) / n, presents - p0, uploads - u0); });
  lastFrame = TT + 3; app_main(0, 0); return 0;
}
