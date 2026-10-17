// Headless functional test of the PS4 UI: stub SDL, scripted controller, virtual clock.
#define ENABLE_LAN 1
#include <SDL2/SDL.h>
#include <stdio.h>
#include <functional>
#include <vector>
#include <string>
#include <unistd.h>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

// ---------------------------------------------------------------- stub SDL
static int g_frame = 0, g_copies = 0, g_fills = 0, g_btn[32], g_hat = 0, g_quit = 0;
int SDL_Init(uint32_t) { return 0; } void SDL_Quit(void) {}
int SDL_NumJoysticks(void) { return 1; } SDL_Joystick *SDL_JoystickOpen(int) { return (SDL_Joystick *)1; } void SDL_JoystickUpdate(void) {}
int SDL_JoystickNumButtons(SDL_Joystick *) { return 16; } int SDL_JoystickNumAxes(SDL_Joystick *) { return 4; } int SDL_JoystickNumHats(SDL_Joystick *) { return 1; }
Uint8 SDL_JoystickGetButton(SDL_Joystick *, int i) { return (Uint8)g_btn[i]; } Sint16 SDL_JoystickGetAxis(SDL_Joystick *, int) { return 0; } Uint8 SDL_JoystickGetHat(SDL_Joystick *, int) { return (Uint8)g_hat; }
SDL_Window *SDL_CreateWindow(const char *, int, int, int, int, uint32_t) { return (SDL_Window *)1; }
SDL_Renderer *SDL_CreateRenderer(SDL_Window *, int, uint32_t) { return (SDL_Renderer *)1; }
SDL_Texture *SDL_CreateTexture(SDL_Renderer *, uint32_t, int, int, int) { return (SDL_Texture *)1; }
int SDL_UpdateTexture(SDL_Texture *, const SDL_Rect *, const void *, int) { return 0; }
int SDL_SetTextureBlendMode(SDL_Texture *, int) { return 0; } int SDL_SetTextureColorMod(SDL_Texture *, Uint8, Uint8, Uint8) { return 0; } int SDL_SetTextureAlphaMod(SDL_Texture *, Uint8) { return 0; }
int SDL_RenderCopy(SDL_Renderer *, SDL_Texture *, const SDL_Rect *, const SDL_Rect *) { g_copies++; return 0; }
int SDL_SetRenderDrawBlendMode(SDL_Renderer *, int) { return 0; } int SDL_SetRenderDrawColor(SDL_Renderer *, Uint8, Uint8, Uint8, Uint8) { return 0; }
int SDL_RenderFillRect(SDL_Renderer *, const SDL_Rect *) { g_fills++; return 0; } void SDL_RenderPresent(SDL_Renderer *) {}
Uint32 SDL_GetTicks(void) { return (Uint32)g_frame * 16; }
int SDL_PollEvent(SDL_Event *e) { if (g_quit) { e->type = SDL_QUIT; g_quit = 0; return 1; } return 0; }

// ---------------------------------------------------------------- script
struct Act { int frame; int kind; int arg; std::function<void()> fn; };
static std::vector<Act> acts; static int TT = 3, lastFrameOfScript = 0;
static void tap(int b) { acts.push_back({TT, 0, b, 0}); acts.push_back({TT + 1, 1, b, 0}); TT += 4; }
static void hold(int b, int frames) { acts.push_back({TT, 0, b, 0}); acts.push_back({TT + frames, 1, b, 0}); TT += frames + 3; }
static void dir(int d) { acts.push_back({TT, 2, d, 0}); acts.push_back({TT + 1, 2, 0, 0}); TT += 4; }
static void wait(int n) { TT += n; }
static void check(std::function<void()> f) { acts.push_back({TT, 3, 0, f}); TT += 2; }
#define UP SDL_HAT_UP
#define DOWN SDL_HAT_DOWN
#define LEFT SDL_HAT_LEFT
#define RIGHT SDL_HAT_RIGHT
static void runActs() {
  for (size_t i = 0; i < acts.size(); i++) if (acts[i].frame == g_frame) {
    Act &a = acts[i];
    if (a.kind == 0) g_btn[a.arg] = 1; else if (a.kind == 1) g_btn[a.arg] = 0; else if (a.kind == 2) g_hat = a.arg; else a.fn();
  }
  if (g_frame >= lastFrameOfScript) g_quit = 1;
}
void SDL_Delay(Uint32) { g_frame++; runActs(); }

#define main app_main
#include "../ps4/main.cpp"
#undef main

// ---------------------------------------------------------------- fake phone
static int phone = -1; static std::string phoneBuf;
static void phoneRead() { char b[8192]; ssize_t n; while (phone >= 0 && (n = recv(phone, b, sizeof(b), 0)) > 0) phoneBuf.append(b, (size_t)n); }
static void phoneSend(const std::string &pl) {
  std::string f; f += (char)0x81; f += (char)(0x80 | pl.size()); const uint8_t k[4] = {1, 2, 3, 4}; for (int i = 0; i < 4; i++) f += (char)k[i];
  for (size_t i = 0; i < pl.size(); i++) f += (char)(pl[i] ^ k[i & 3]);
  send(phone, f.data(), f.size(), 0);
}

int main() {
  // 1. menu -> robot game (Beginner level, white)
  for (int i = 0; i < 5; i++) dir(UP);
  dir(LEFT); dir(LEFT);
  check([] { CHECK(app.menuSel == 2 && app.level == 0 && app.screen == S_MENU); });
  for (int i = 0; i < 5; i++) dir(DOWN);
  tap(0);
  check([] { CHECK(app.screen == S_GAME && app.mode == M_ROBOT && app.humanSide == WHITE && app.game.plies() == 0); });
  // select e2, move to e4 (cursor starts on e2)
  tap(0); check([] { CHECK(app.sel == 12); });
  dir(UP); dir(UP); tap(0);
  check([] { CHECK(app.game.plies() == 1 && app.game.san[0] == "e4"); });
  wait(120);
  check([] { CHECK(app.game.plies() == 2 && !app.game.over()); });
  // cannot move the robot's pieces / out of turn
  tap(3); // triangle: undo (robot mode undoes both plies)
  check([] { CHECK(app.game.plies() == 0); });

  // 2. promotion dialog
  check([] { app.game.loadFen("8/P6k/8/8/8/8/8/K7 w - - 0 1"); app.curCol = 0; app.curRow = 1; app.aiState = 0; });
  tap(0); dir(UP); tap(0);
  check([] { CHECK(app.promoOpen && app.promoMoves.size() == 4); });
  dir(RIGHT); tap(0);
  check([] { CHECK(!app.promoOpen && app.game.pos.b[56] == mkp(ROOK, WHITE)); });
  wait(80);

  // 3. long-press O -> pause menu -> resign -> rematch
  hold(1, 80);
  check([] { CHECK(app.paused); });
  dir(DOWN); dir(DOWN); tap(0);
  check([] { CHECK(!app.paused && app.game.over() && app.game.result == R_BLACK && app.game.reason == "resignation"); });
  wait(20); tap(0);
  check([] { CHECK(app.screen == S_GAME && app.game.plies() == 0 && !app.game.over()); });

  // 4. back to menu through pause, switch to LAN
  hold(1, 80); for (int i = 0; i < 4; i++) dir(DOWN); tap(0);
  check([] { CHECK(app.screen == S_MENU); });
  for (int i = 0; i < 7; i++) dir(UP);
  dir(RIGHT);
  check([] { CHECK(app.menuSel == 0 && app.mode == M_LAN); });
  // phone opens the page while the PS4 is still on the menu
  wait(5);
  check([] { CHECK(app.screen == S_MENU && app.room.srv.running());
    phone = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons(8080); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    CHECK(connect(phone, (sockaddr *)&a, sizeof(a)) == 0); net::setNonBlocking(phone);
    std::string req = "GET /ws HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
    send(phone, req.data(), req.size(), 0); });
  wait(10);
  check([] { phoneRead(); CHECK(phoneBuf.find("101 Switching") != std::string::npos); CHECK(phoneBuf.find("Waiting for the game to start") != std::string::npos);
    phoneBuf.clear(); phoneSend("move e7e5"); });
  wait(10);
  check([] { phoneRead(); CHECK(app.game.plies() == 0); CHECK(phoneBuf.find("Waiting for the game to start") != std::string::npos); phoneBuf.clear(); });
  for (int i = 0; i < 7; i++) dir(DOWN);
  tap(0);
  check([] { CHECK(app.screen == S_GAME && app.room.srv.running() && app.humanSide == WHITE); });
  wait(10);
  check([] { phoneRead(); CHECK(phoneBuf.find("\"you\":\"b\"") != std::string::npos); CHECK(phoneBuf.find("Waiting for the game") == std::string::npos || phoneBuf.rfind("\"you\":\"b\"") > phoneBuf.rfind("Waiting for the game")); CHECK(app.room.connected()); phoneBuf.clear(); });
  // PS4 (white) plays e2e4, phone must receive it
  tap(0); dir(UP); dir(UP); tap(0); wait(10);
  check([] { phoneRead(); CHECK(app.game.plies() == 1); CHECK(phoneBuf.find("\"last\":\"e2e4\"") != std::string::npos); phoneBuf.clear(); phoneSend("move e7e5"); });
  wait(10);
  check([] { phoneRead(); CHECK(app.game.plies() == 2 && app.game.san[1] == "e5"); CHECK(phoneBuf.find("\"plies\":2") != std::string::npos); CHECK(app.anim[0].active || app.lastTo == 36); });
  // phone resigns, PS4 sees game over, phone asks for a rematch, sides swap
  check([] { phoneSend("resign"); }); wait(10);
  check([] { CHECK(app.game.over() && app.game.result == R_WHITE); phoneSend("new"); }); wait(10);
  check([] { CHECK(!app.game.over() && app.game.plies() == 0 && app.humanSide == BLACK && app.room.phoneSide == WHITE); });
  check([] { close(phone); phone = -1; });
  wait(10);
  check([] { printf("frames=%d textureCopies=%d fillRects=%d\n", g_frame, g_copies, g_fills); CHECK(g_copies > 1000); });
  lastFrameOfScript = TT + 5;
  app_main(0, 0);
  printf(fails ? "\n%d FAILURES\n" : "\nALL APP TESTS PASSED\n", fails);
  return fails ? 1 : 0;
}
