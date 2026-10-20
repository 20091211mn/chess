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
  net::setNonBlocking(fd); return fd;
}
// pump server and read whatever the client receives
static std::string pump(int fd, int rounds = 40) {
  std::string got; LanEvents ev; int idle = 0;
  for (int i = 0; i < rounds * 5; i++) {
    room.update(game, ev);
    char buf[8192]; ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n > 0) { got.append(buf, (size_t)n); idle = 0; } else idle++;
    usleep(1000);
    if (!got.empty() && idle >= 15) break;   // got something and it went quiet
  }
  return got;
}
static std::string maskedFrame(const std::string &pl, int op = 1, bool fin = true) {
  std::string f; f += (char)((fin ? 0x80 : 0) | op);
  size_t n = pl.size();
  if (n < 126) f += (char)(0x80 | n); else { f += (char)(0x80 | 126); f += (char)(n >> 8); f += (char)(n & 255); }
  const uint8_t k[4] = {0x12, 0x34, 0x56, 0x78}; for (int i = 0; i < 4; i++) f += (char)k[i];
  for (size_t i = 0; i < n; i++) f += (char)(pl[i] ^ k[i & 3]);
  return f;
}
static int wsConnect(std::string &resp) {
  int fd = dial(); if (fd < 0) return -1;
  std::string req = "GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nsec-websocket-key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
  send(fd, req.data(), req.size(), 0);
  resp = pump(fd);
  return fd;
}
// extract text payloads from unmasked server frames
static std::vector<std::string> frames(const std::string &s, size_t start = 0) {
  std::vector<std::string> out; size_t p = start;
  while (p + 2 <= s.size()) {
    int op = (uint8_t)s[p] & 15; size_t len = (uint8_t)s[p + 1] & 127, h = 2;
    if (len == 126) { len = ((uint8_t)s[p+2] << 8) | (uint8_t)s[p+3]; h = 4; }
    if (p + h + len > s.size()) break;
    if (op == 1) out.push_back(s.substr(p + h, len)); else if (op == 10) out.push_back("<pong>"); else if (op == 8) out.push_back("<close>");
    p += h + len;
  }
  return out;
}

int main() {
  // crypto vectors
  CHECK(net::wsAccept("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
  CHECK(net::base64("") == "" && net::base64("f") == "Zg==" && net::base64("fo") == "Zm8=" && net::base64("foo") == "Zm9v" && net::base64("foobar") == "Zm9vYmFy");
  { std::string h = net::sha1("abc"); char hex[64] = {0}; for (int i = 0; i < 20; i++) sprintf(hex + i * 2, "%02x", (uint8_t)h[i]); CHECK(std::string(hex) == "a9993e364706816aba3e25717850c26c9cd0d89d"); }

  room.srv.html = "<html>hello chess</html>";
  room.phoneSide = BLACK;
  std::string err; CHECK(room.start(PORT, &err));

  // plain HTTP page
  { int fd = dial(); CHECK(fd >= 0); std::string r = "GET / HTTP/1.1\r\nHost: x\r\n\r\n"; send(fd, r.data(), r.size(), 0);
    std::string got = pump(fd); CHECK(got.find("200 OK") != std::string::npos); CHECK(got.find("<html>hello chess</html>") != std::string::npos);
    CHECK(got.find("Content-Length: 24") != std::string::npos); close(fd); }
  { int fd = dial(); std::string r = "GET /nothing HTTP/1.1\r\n\r\n"; send(fd, r.data(), r.size(), 0); std::string got = pump(fd); CHECK(got.find("404") != std::string::npos); close(fd); }

  // websocket handshake; first client controls black
  std::string resp; int c1 = wsConnect(resp);
  CHECK(resp.find("101 Switching Protocols") != std::string::npos);
  CHECK(resp.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
  size_t hdrEnd = resp.find("\r\n\r\n") + 4;
  std::vector<std::string> fr = frames(resp, hdrEnd);
  CHECK(fr.size() == 1 && fr[0].find("\"you\":\"b\"") != std::string::npos);
  CHECK(fr.size() == 1 && fr[0].find("\"turn\":\"w\"") != std::string::npos);
  CHECK(room.connected());

  // phone tries to move while it is white's turn -> rejected
  { std::string m = maskedFrame("move e7e5"); send(c1, m.data(), m.size(), 0);
    std::vector<std::string> f = frames(pump(c1)); CHECK(!f.empty() && f.back().find("not your turn") != std::string::npos); CHECK(game.plies() == 0); }

  // PS4 (white) plays e2e4 -> phone gets state
  { CHECK(game.playUci("e2e4")); room.last = "e2e4"; room.broadcastState(game);
    std::vector<std::string> f = frames(pump(c1)); CHECK(!f.empty() && f.back().find("\"turn\":\"b\"") != std::string::npos);
    CHECK(f.back().find("\"last\":\"e2e4\"") != std::string::npos); CHECK(f.back().find("\"e7e5\"") != std::string::npos); }

  // illegal move rejected; legal move accepted
  { std::string m = maskedFrame("move e7e3"); send(c1, m.data(), m.size(), 0);
    std::vector<std::string> f = frames(pump(c1)); CHECK(!f.empty() && f.back().find("illegal move") != std::string::npos); CHECK(game.plies() == 1); }
  { std::string m = maskedFrame("move e7e5"); send(c1, m.data(), m.size(), 0); LanEvents ev; 
    for (int i = 0; i < 160; i++) { room.update(game, ev); usleep(1000); } CHECK(ev.moved); CHECK(game.plies() == 2); CHECK(game.san[1] == "e5"); }

  // message split across two TCP writes + 126-byte extended length payload
  { std::string big = "move g1f3"; big += std::string(200, ' '); std::string m = maskedFrame(big);
    send(c1, m.data(), 3, 0); pump(c1, 5); send(c1, m.data() + 3, m.size() - 3, 0); LanEvents ev;
    for (int i = 0; i < 120; i++) { room.update(game, ev); usleep(1000); }
    CHECK(game.plies() == 2); /* white's turn, phone is black: must be rejected, no crash */ }

  // spectator cannot move and sees you:"s"
  { std::string r2; int c2 = wsConnect(r2); std::vector<std::string> f = frames(r2, r2.find("\r\n\r\n") + 4);
    CHECK(!f.empty() && f[0].find("\"you\":\"s\"") != std::string::npos);
    CHECK(game.playUci("g1f3")); room.broadcastState(game);
    std::string m = maskedFrame("move b8c6"); send(c2, m.data(), m.size(), 0); std::vector<std::string> g2 = frames(pump(c2));
    CHECK(game.plies() == 3); bool sawSpec = false; for (size_t i = 0; i < g2.size(); i++) if (g2[i].find("spectator") != std::string::npos) sawSpec = true; CHECK(sawSpec);
    // ping/pong
    std::string pg = maskedFrame("hi", 9); send(c2, pg.data(), pg.size(), 0); std::vector<std::string> p2 = frames(pump(c2)); bool pong = false; for (size_t i = 0; i < p2.size(); i++) if (p2[i] == "<pong>") pong = true; CHECK(pong);
    close(c2); pump(c1, 10); }

  // phone resigns -> game over, white wins; "new" only after game over
  { std::string m = maskedFrame("new"); send(c1, m.data(), m.size(), 0); LanEvents ev; for (int i = 0; i < 80; i++) { room.update(game, ev); usleep(1000); } CHECK(!ev.newGameReq);
    m = maskedFrame("resign"); send(c1, m.data(), m.size(), 0); LanEvents ev2; for (int i = 0; i < 80; i++) { room.update(game, ev2); usleep(1000); }
    CHECK(ev2.resigned); CHECK(game.over() && game.result == R_WHITE && game.reason == "resignation");
    m = maskedFrame("new"); send(c1, m.data(), m.size(), 0); LanEvents ev3; for (int i = 0; i < 80; i++) { room.update(game, ev3); usleep(1000); } CHECK(ev3.newGameReq); }

  // disconnect frees the controller slot, a new client takes over
  { close(c1); LanEvents ev; for (int i = 0; i < 120; i++) { room.update(game, ev); usleep(1000); } CHECK(!room.connected());
    std::string r3; int c3 = wsConnect(r3); CHECK(room.connected()); close(c3); }

  // ping/pong, history, draw offers (fresh game)
  { game.reset(); room.last.clear(); std::string r4; int c4 = wsConnect(r4); CHECK(room.connected());
    std::string m = maskedFrame("ping"); send(c4, m.data(), m.size(), 0); std::vector<std::string> f = frames(pump(c4));
    bool pong = false; for (size_t i = 0; i < f.size(); i++) if (f[i].find("\"t\":\"pong\"") != std::string::npos) pong = true; CHECK(pong);
    CHECK(game.playUci("e2e4")); CHECK(game.playUci("e7e5"));
    m = maskedFrame("hist"); send(c4, m.data(), m.size(), 0); f = frames(pump(c4)); bool hist = false;
    for (size_t i = 0; i < f.size(); i++) if (f[i].find("\"t\":\"hist\"") != std::string::npos && f[i].find("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") != std::string::npos && f[i].find("\"n\":2") != std::string::npos) hist = true; CHECK(hist);
    // phone (black) offers a draw while it is white's turn: allowed, game.drawOffer set; white's move clears it
    m = maskedFrame("draw"); send(c4, m.data(), m.size(), 0); LanEvents ev; for (int i = 0; i < 120; i++) { room.update(game, ev); usleep(1000); }
    CHECK(ev.drawOffered && game.drawOffer == BLACK); f = frames(pump(c4)); bool sawDraw = false; for (size_t i = 0; i < f.size(); i++) if (f[i].find("\"draw\":1") != std::string::npos) sawDraw = true; CHECK(sawDraw);
    CHECK(game.playUci("g1f3")); CHECK(game.drawOffer == -1);
    // PS4 (white) offers, phone accepts
    CHECK(game.offerDraw(WHITE)); room.broadcastState(game); pump(c4, 5);
    m = maskedFrame("draw_yes"); send(c4, m.data(), m.size(), 0); LanEvents ev2; for (int i = 0; i < 120; i++) { room.update(game, ev2); usleep(1000); }
    CHECK(ev2.drawAnswered && game.over() && game.result == R_DRAW && game.reason == "agreement");
    // an offer can only be answered by the other side: phone cannot accept its own offer
    game.reset(); CHECK(game.playUci("e2e4")); m = maskedFrame("draw"); send(c4, m.data(), m.size(), 0); for (int i = 0; i < 80; i++) { room.update(game, ev2); usleep(1000); }
    m = maskedFrame("draw_yes"); send(c4, m.data(), m.size(), 0); for (int i = 0; i < 80; i++) { room.update(game, ev2); usleep(1000); } CHECK(!game.over());
    close(c4); { LanEvents ev3; for (int i = 0; i < 80; i++) { room.update(game, ev3); usleep(1000); } } }

  // takeover: a phone that stopped answering is replaced by a new connection
  { game.reset(); std::string ra; int ca = wsConnect(ra); CHECK(room.connected()); int oldId = room.phoneId;
    LanEvents ev; uint32_t fake = net::nowMs() + 20000; // pretend 20 s passed without a ping from the old phone
    std::string rb; int cb = dial(); std::string req = "GET /ws HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"; send(cb, req.data(), req.size(), 0);
    for (int i = 0; i < 80; i++) { room.update(game, ev, fake); usleep(1000); }
    CHECK(room.connected() && room.phoneId != oldId); close(ca); close(cb);
    LanEvents ev4; for (int i = 0; i < 120; i++) { room.update(game, ev4, fake + 40000); usleep(1000); } }

  // discovery: UDP broadcast probe and /ping
  { int u = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons(PORT + 1); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    const char *probe = "PS4CHESS?"; sendto(u, probe, strlen(probe), 0, (sockaddr *)&a, sizeof(a)); net::setNonBlocking(u);
    std::string reply; for (int i = 0; i < 200 && reply.empty(); i++) { LanEvents ev; room.update(game, ev); char b[128]; ssize_t n = recv(u, b, sizeof(b), 0); if (n > 0) reply.assign(b, (size_t)n); usleep(1000); }
    CHECK(reply == "PS4CHESS|18765|PS4 Chess"); const char *junk = "hello"; sendto(u, junk, strlen(junk), 0, (sockaddr *)&a, sizeof(a)); { LanEvents ev; for (int i = 0; i < 20; i++) { room.update(game, ev); usleep(1000); } } close(u);
    int fd = dial(); std::string r = "GET /ping HTTP/1.0\r\n\r\n"; send(fd, r.data(), r.size(), 0); std::string got = pump(fd); CHECK(got.find("PS4CHESS") != std::string::npos && got.find("200 OK") != std::string::npos); close(fd); }

  // garbage input must not crash the server
  { int fd = dial(); std::string junk(5000, '\xff'); send(fd, junk.data(), junk.size(), 0); pump(fd, 10); close(fd);
    fd = dial(); std::string bad = "GET /ws HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: abc\r\n\r\n"; send(fd, bad.data(), bad.size(), 0); std::string q = pump(fd, 10);
    std::string huge; huge += (char)0x81; huge += (char)(0x80 | 127); for (int i = 0; i < 8; i++) huge += (char)0x7f; huge += std::string(4, 'x'); send(fd, huge.data(), huge.size(), 0); pump(fd, 10); close(fd);
    int ok = dial(); CHECK(ok >= 0); close(ok); }

  room.stop();
  std::string ip = net::localIp(); printf("localIp: '%s'\n", ip.c_str());
  printf(fails ? "\n%d FAILURES\n" : "\nALL NET TESTS PASSED\n", fails);
  return fails ? 1 : 0;
}
