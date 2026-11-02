// Tests of the PS4 LAN layer: plain TCP line protocol, UDP discovery, room logic (moves, draw offers, takeover).
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "../core/lan.hpp"
using namespace ce;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static LanRoom room; static Game game; static const int PORT = 18765;

static int dial() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons(PORT); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
  if (connect(fd, (sockaddr *)&a, sizeof(a)) != 0) return -1;
  net::makeNonBlocking(fd); return fd;
}
// pump the server and collect complete lines the client received
static std::vector<std::string> pump(int fd, std::string &buf, int rounds = 40) {
  std::vector<std::string> lines; LanEvents ev; int idle = 0;
  for (int i = 0; i < rounds * 5; i++) {
    room.update(game, ev);
    char b[8192]; ssize_t n = recv(fd, b, sizeof(b), 0);
    if (n > 0) { buf.append(b, (size_t)n); idle = 0; } else idle++;
    size_t nl; while ((nl = buf.find('\n')) != std::string::npos) { lines.push_back(buf.substr(0, nl)); buf.erase(0, nl + 1); }
    usleep(1000);
    if (!lines.empty() && idle >= 15) break;
  }
  return lines;
}
static void sendLine(int fd, const std::string &s) { std::string l = s + "\n"; send(fd, l.data(), l.size(), 0); }
static void spin(int n) { LanEvents ev; for (int i = 0; i < n; i++) { room.update(game, ev); usleep(1000); } }
static bool has(const std::vector<std::string> &v, const char *needle) { for (size_t i = 0; i < v.size(); i++) if (v[i].find(needle) != std::string::npos) return true; return false; }

int main() {
  room.phoneSide = BLACK;
  std::string err; CHECK(room.start(PORT, &err));

  // 1. connect: greeting first, then the state for our side
  std::string b1; int c1 = dial(); CHECK(c1 >= 0);
  std::vector<std::string> l = pump(c1, b1);
  CHECK(l.size() >= 2 && l[0].find("\"app\":\"PS4CHESS\"") != std::string::npos && l[0].find("\"t\":\"hello\"") != std::string::npos);
  CHECK(has(l, "\"you\":\"b\"") && has(l, "\"turn\":\"w\"") && room.connected());
  sendLine(c1, "ping"); l = pump(c1, b1); CHECK(has(l, "\"t\":\"pong\""));
  sendLine(c1, "hello"); l = pump(c1, b1); CHECK(has(l, "\"t\":\"state\""));

  // 2. turn order and legality
  sendLine(c1, "move e7e5"); l = pump(c1, b1); CHECK(has(l, "not your turn") && game.plies() == 0);
  CHECK(game.playUci("e2e4")); room.last = "e2e4"; room.broadcastState(game);
  l = pump(c1, b1); CHECK(has(l, "\"last\":\"e2e4\"") && has(l, "\"turn\":\"b\""));
  sendLine(c1, "move e7e3"); l = pump(c1, b1); CHECK(has(l, "illegal move") && game.plies() == 1);
  { sendLine(c1, "move e7e5"); LanEvents ev; for (int i = 0; i < 120; i++) { room.update(game, ev); usleep(1000); if (ev.moved) break; } CHECK(ev.moved && game.plies() == 2 && game.san[1] == "e5"); }
  pump(c1, b1);

  // 3. a line arriving in two TCP segments, and several lines in one segment
  { std::string m = "move g1f3\n"; send(c1, m.data(), 4, 0); spin(20); send(c1, m.data() + 4, m.size() - 4, 0); spin(40); pump(c1, b1);
    CHECK(game.plies() == 2); /* white's turn, phone is black -> refused, no crash */
    std::string two = "ping\nping\n"; send(c1, two.data(), two.size(), 0); l = pump(c1, b1); int pongs = 0; for (size_t i = 0; i < l.size(); i++) if (l[i].find("pong") != std::string::npos) pongs++; CHECK(pongs == 2);
    std::string crlf = "ping\r\n"; send(c1, crlf.data(), crlf.size(), 0); l = pump(c1, b1); CHECK(has(l, "pong")); }

  // 4. spectator
  { std::string b2; int c2 = dial(); l = pump(c2, b2); CHECK(has(l, "\"you\":\"s\""));
    CHECK(game.playUci("g1f3")); room.broadcastState(game);
    sendLine(c2, "move b8c6"); l = pump(c2, b2); CHECK(game.plies() == 3 && has(l, "spectator")); close(c2); spin(30); }

  // 5. resign, rematch request only after the game is over
  { sendLine(c1, "new"); LanEvents ev; for (int i = 0; i < 60; i++) { room.update(game, ev); usleep(1000); } CHECK(!ev.newGameReq);
    sendLine(c1, "resign"); LanEvents ev2; for (int i = 0; i < 100; i++) { room.update(game, ev2); usleep(1000); }
    CHECK(ev2.resigned && game.over() && game.result == R_WHITE && game.reason == "resignation");
    sendLine(c1, "new"); LanEvents ev3; for (int i = 0; i < 100; i++) { room.update(game, ev3); usleep(1000); } CHECK(ev3.newGameReq); pump(c1, b1); }

  // 6. history, draw offers
  { game.reset(); room.last.clear(); CHECK(game.playUci("e2e4")); CHECK(game.playUci("e7e5"));
    sendLine(c1, "hist"); l = pump(c1, b1); CHECK(has(l, "\"t\":\"hist\"") && has(l, "\"n\":2") && has(l, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    sendLine(c1, "draw"); LanEvents ev; for (int i = 0; i < 100; i++) { room.update(game, ev); usleep(1000); } CHECK(ev.drawOffered && game.drawOffer == BLACK);
    l = pump(c1, b1); CHECK(has(l, "\"draw\":1")); CHECK(game.playUci("g1f3")); CHECK(game.drawOffer == -1);
    CHECK(game.offerDraw(WHITE)); room.broadcastState(game); pump(c1, b1);
    sendLine(c1, "draw_yes"); LanEvents ev2; for (int i = 0; i < 100; i++) { room.update(game, ev2); usleep(1000); }
    CHECK(ev2.drawAnswered && game.over() && game.result == R_DRAW && game.reason == "agreement");
    game.reset(); CHECK(game.playUci("e2e4")); sendLine(c1, "draw"); spin(80); sendLine(c1, "draw_yes"); spin(80); CHECK(!game.over()); }

  // 7. takeover: a phone that stopped answering is replaced by a new connection
  { game.reset(); int oldId = room.phoneId; uint32_t fake = net::nowMs() + 20000; std::string b3; int c3 = dial();
    LanEvents ev; for (int i = 0; i < 40; i++) { room.update(game, ev, fake); usleep(1000); }
    CHECK(room.connected() && room.phoneId != oldId); close(c3); close(c1); spin(60);
    LanEvents ev4; for (int i = 0; i < 40; i++) { room.update(game, ev4, fake + 40000); usleep(1000); } }

  // 8. UDP discovery
  { int u = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons(PORT); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    const char *probe = "PS4CHESS?"; sendto(u, probe, strlen(probe), 0, (sockaddr *)&a, sizeof(a)); net::makeNonBlocking(u);
    std::string reply; for (int i = 0; i < 300 && reply.empty(); i++) { LanEvents ev; room.update(game, ev); char b[128]; ssize_t n = recv(u, b, sizeof(b), 0); if (n > 0) reply.assign(b, (size_t)n); usleep(1000); }
    CHECK(reply == "PS4CHESS|18765|PS4 Chess"); close(u); }

  // 9. garbage must not crash or stall the server
  { int fd = dial(); std::string junk(9000, '\xff'); send(fd, junk.data(), junk.size(), 0); spin(60); close(fd);
    fd = dial(); std::string nonl(5000, 'x'); send(fd, nonl.data(), nonl.size(), 0); spin(60); close(fd);
    int ok = dial(); CHECK(ok >= 0); close(ok); spin(20); }

  // 10. the server never blocks: a poll with silent clients takes (well) under a few milliseconds
  { std::string b4; int c = dial(), d = dial(); pump(c, b4); room.srv.takeMaxPollUs(); uint64_t t0 = net::nowUs(); for (int i = 0; i < 200; i++) { LanEvents ev; room.update(game, ev); }
    double avgUs = (double)(net::nowUs() - t0) / 200; printf("average LAN update with 2 silent clients: %.0f us (max single poll %llu us)\n", avgUs, (unsigned long long)room.srv.takeMaxPollUs());
    CHECK(avgUs < 2000); close(c); close(d); spin(20); }

  room.stop();
  std::string ip = net::localIp(); printf("localIp: '%s'\n", ip.c_str());
  printf(fails ? "\n%d FAILURES\n" : "\nALL NET TESTS PASSED\n", fails);
  return fails ? 1 : 0;
}
