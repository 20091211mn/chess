// PS4 simulator: the PS4 side of the LAN protocol (TCP lines + UDP discovery) on a PC or in Termux, with a robot as the
// PS4 player. Lets you try the Android app without a console, and is what the automated Android tests talk to.
// Build: clang++ -O2 -std=c++11 tools/ps4sim.cpp -o ps4sim      Run: ./ps4sim [port=7788] [robot level 0-4 = 2] [w|b = the phone's colour]
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../core/ai.hpp"
#include "../core/lan.hpp"
using namespace ce;
int main(int argc, char **argv) {
  int port = argc > 1 ? atoi(argv[1]) : 7788, level = argc > 2 ? atoi(argv[2]) : 2;
  int mine = (argc > 3 && argv[3][0] == 'b') ? BLACK : WHITE;
  if (level < 0) level = 0;
  if (level > 4) level = 4;
  Game g; LanRoom room; room.phoneSide = mine;
  std::string err;
  if (!room.start(port, &err)) { printf("cannot start: %s\n", err.c_str()); return 1; }
  std::string ip = net::localIp();
  printf("PS4 simulator on port %d (robot level %d: %s). Phone plays %s.%s%s\n", port, level, LEVELS[level].name, mine == WHITE ? "White" : "Black", ip.empty() ? "" : "  Address: ", ip.c_str());
  fflush(stdout);
  for (;;) {
    LanEvents ev;
    room.update(g, ev);
    if (ev.drawOffered) { int cp = evalWhiteCp(g.pos, 10000), robotCp = room.phoneSide == WHITE ? -cp : cp; g.answerDraw(robotCp <= 30); room.broadcastState(g); }
    if (ev.newGameReq) { mine ^= 1; room.phoneSide = mine; g.reset(); room.last.clear(); room.broadcastState(g); }
    if (room.connected() && !g.over() && g.pos.side != room.phoneSide) {
      usleep(350000);
      Move m = aiMove(g.pos, level);
      if (!m.isNull() && g.play(m)) { room.last = toUci(m); room.extra.evalCp = evalWhiteCp(g.pos, 10000); room.extra.hasEval = true; room.broadcastState(g); }
    }
    usleep(2000);
  }
}
