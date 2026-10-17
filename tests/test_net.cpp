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
  std::string got; LanEvents ev;
  for (int i = 0; i < rounds; i++) {
    room.update(game, ev);
    char buf[8192]; ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n > 0) got.append(buf, (size_t)n);
    usleep(1000);
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
    for (int i = 0; i < 40; i++) { room.update(game, ev); usleep(1000); } CHECK(ev.moved); CHECK(game.plies() == 2); CHECK(game.san[1] == "e5"); }

  // message split across two TCP writes + 126-byte extended length payload
  { std::string big = "move g1f3"; big += std::string(200, ' '); std::string m = maskedFrame(big);
    send(c1, m.data(), 3, 0); pump(c1, 5); send(c1, m.data() + 3, m.size() - 3, 0); LanEvents ev;
    for (int i = 0; i < 30; i++) { room.update(game, ev); usleep(1000); }
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
  { std::string m = maskedFrame("new"); send(c1, m.data(), m.size(), 0); LanEvents ev; for (int i = 0; i < 20; i++) { room.update(game, ev); usleep(1000); } CHECK(!ev.newGameReq);
    m = maskedFrame("resign"); send(c1, m.data(), m.size(), 0); LanEvents ev2; for (int i = 0; i < 20; i++) { room.update(game, ev2); usleep(1000); }
    CHECK(ev2.resigned); CHECK(game.over() && game.result == R_WHITE && game.reason == "resignation");
    m = maskedFrame("new"); send(c1, m.data(), m.size(), 0); LanEvents ev3; for (int i = 0; i < 20; i++) { room.update(game, ev3); usleep(1000); } CHECK(ev3.newGameReq); }

  // disconnect frees the controller slot, a new client takes over
  { close(c1); LanEvents ev; for (int i = 0; i < 30; i++) { room.update(game, ev); usleep(1000); } CHECK(!room.connected());
    std::string r3; int c3 = wsConnect(r3); CHECK(room.connected()); close(c3); }

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
