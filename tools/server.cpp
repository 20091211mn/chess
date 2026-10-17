// Run the chess web page on this machine (Termux/Linux): play against the robot from a browser.
// Build: clang++ -O2 -std=c++11 tools/server.cpp -o chessweb
// Run:   ./chessweb [port=8080] [level 0-4 =2] [w|b = your colour]
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../core/ai.hpp"
#include "../core/lan.hpp"
#include "../core/web_html.h"
using namespace ce;
int main(int argc, char **argv) {
  int port = argc > 1 ? atoi(argv[1]) : 8080, level = argc > 2 ? atoi(argv[2]) : 2;
  int mine = (argc > 3 && argv[3][0] == 'b') ? BLACK : WHITE;
  Game g; LanRoom room; room.phoneSide = mine;
  for (int i = 0; WEB_INDEX_PARTS[i]; i++) room.srv.html += WEB_INDEX_PARTS[i];
  std::string err;
  if (!room.start(port, &err)) { printf("cannot start server: %s\n", err.c_str()); return 1; }
  std::string ip = net::localIp();
  printf("Chess web server running (robot level %d: %s)\n  This phone:  http://localhost:%d\n", level, LEVELS[level < 0 ? 0 : level > 4 ? 4 : level].name, port);
  if (!ip.empty()) printf("  Other device on same Wi-Fi: http://%s:%d\nCtrl+C to stop\n", ip.c_str(), port);
  for (;;) {
    LanEvents ev;
    room.update(g, ev);
    if (ev.moved || ev.resigned || ev.connected || ev.disconnected) room.broadcastState(g);
    if (ev.newGameReq) { mine ^= 1; room.phoneSide = mine; g.reset(); room.last.clear(); room.broadcastState(g); }
    if (room.connected() && !g.over() && g.pos.side != room.phoneSide) {
      usleep(350000);
      Move m = aiMove(g.pos, level);
      if (!m.isNull() && g.play(m)) { room.last = toUci(m); room.broadcastState(g); }
    }
    usleep(4000);
  }
}
