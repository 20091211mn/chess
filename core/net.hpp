// Tiny non-blocking HTTP + WebSocket server (no threads, poll from the game loop).
// Serves one HTML page at "/" and a WebSocket at "/ws". POSIX sockets.
#pragma once
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <deque>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>

namespace net {

// ------------------------------------------------------------- SHA-1 / Base64
inline std::string sha1(const std::string &msg) {
  uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
  std::string m = msg;
  uint64_t bits = (uint64_t)msg.size() * 8;
  m += (char)0x80;
  while (m.size() % 64 != 56) m += (char)0;
  for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xFF);
  for (size_t off = 0; off < m.size(); off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
      w[i] = ((uint32_t)(uint8_t)m[off + i * 4] << 24) | ((uint32_t)(uint8_t)m[off + i * 4 + 1] << 16) |
             ((uint32_t)(uint8_t)m[off + i * 4 + 2] << 8) | (uint32_t)(uint8_t)m[off + i * 4 + 3];
    for (int i = 16; i < 80; i++) { uint32_t x = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16]; w[i] = (x << 1) | (x >> 31); }
    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
    for (int i = 0; i < 80; i++) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6; }
      uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
      e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = t;
    }
    h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
  }
  std::string out;
  uint32_t hs[5] = {h0, h1, h2, h3, h4};
  for (int i = 0; i < 5; i++) for (int j = 3; j >= 0; j--) out += (char)((hs[i] >> (j * 8)) & 0xFF);
  return out;
}

inline std::string base64(const std::string &in) {
  static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  size_t i = 0;
  for (; i + 2 < in.size(); i += 3) {
    uint32_t v = ((uint8_t)in[i] << 16) | ((uint8_t)in[i+1] << 8) | (uint8_t)in[i+2];
    out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += T[v & 63];
  }
  if (i + 1 == in.size()) {
    uint32_t v = (uint8_t)in[i] << 16;
    out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63]; out += "==";
  } else if (i + 2 == in.size()) {
    uint32_t v = ((uint8_t)in[i] << 16) | ((uint8_t)in[i+1] << 8);
    out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += '=';
  }
  return out;
}

inline std::string wsAccept(const std::string &key) {
  return base64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
}

inline std::string wsFrame(const std::string &payload, int opcode = 1) {
  std::string f;
  f += (char)(0x80 | opcode);
  size_t n = payload.size();
  if (n < 126) f += (char)n;
  else if (n < 65536) { f += (char)126; f += (char)(n >> 8); f += (char)(n & 255); }
  else { f += (char)127; for (int i = 7; i >= 0; i--) f += (char)(((uint64_t)n >> (i * 8)) & 255); }
  return f + payload;
}

inline std::string lower(std::string s) { for (size_t i = 0; i < s.size(); i++) if (s[i] >= 'A' && s[i] <= 'Z') s[i] += 32; return s; }

// header value lookup (case-insensitive name)
inline std::string headerValue(const std::string &req, const std::string &name) {
  std::string low = lower(req), key = "\r\n" + lower(name) + ":";
  size_t p = low.find(key);
  if (p == std::string::npos) return "";
  p += key.size();
  size_t e = req.find("\r\n", p);
  std::string v = req.substr(p, e == std::string::npos ? std::string::npos : e - p);
  size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t");
  return a == std::string::npos ? "" : v.substr(a, b - a + 1);
}

// ------------------------------------------------------------- server
struct Client {
  int fd, id;
  bool ws, closing;
  uint32_t lastRx, born;
  std::string in, out, frag;
};
struct Route { std::string path, mime, body; };

inline uint32_t nowMs() { timeval tv; gettimeofday(&tv, 0); return (uint32_t)(tv.tv_sec * 1000u + tv.tv_usec / 1000u); }
struct Event { int id; int type; std::string msg; }; // type: 0 message, 1 connected, 2 disconnected

inline void setNonBlocking(int fd) {
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
#ifdef FIONBIO
  int one = 1;
  ioctl(fd, FIONBIO, &one); // second attempt: some platforms only honour this one
#endif
}

// Never block, even if the socket could not be switched to non-blocking mode:
// ask select() first. If select() itself is unavailable (<0) assume ready.
inline bool fdReady(int fd, bool wr) {
  if (fd < 0 || fd >= FD_SETSIZE) return true;
  fd_set s; FD_ZERO(&s); FD_SET(fd, &s);
  timeval tv; tv.tv_sec = 0; tv.tv_usec = 0;
  int r = wr ? select(fd + 1, 0, &s, 0, &tv) : select(fd + 1, &s, 0, 0, &tv);
  return r != 0; // >0 ready, <0 unknown -> try anyway
}

class Server {
 public:
  std::string html;
  Server() : lfd(-1), ufd(-1), nextId(1), stage_(0), port_(0) {}
  ~Server() { stop(); }

  // One startup step per call so a UI can show progress (and where a platform call hangs).
  // returns 0 = failed (err set), 1 = step done, more to do, 2 = server is listening
  int startStep(int port, std::string *err = 0) {
    char buf[64];
    switch (stage_) {
      case 0:
#ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
#endif
        lfd = socket(AF_INET, SOCK_STREAM, 0);
        if (lfd < 0) { snprintf(buf, sizeof(buf), "socket() failed (errno %d)", errno); if (err) *err = buf; lfd = -1; return 0; }
        break;
      case 1: { int one = 1; setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one)); break; }
      case 2: {
        sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(lfd, (sockaddr *)&a, sizeof(a)) < 0) { snprintf(buf, sizeof(buf), "bind() failed (errno %d)", errno); if (err) *err = buf; close(lfd); lfd = -1; stage_ = 0; return 0; }
        break; }
      case 3:
        if (listen(lfd, 8) < 0) { snprintf(buf, sizeof(buf), "listen() failed (errno %d)", errno); if (err) *err = buf; close(lfd); lfd = -1; stage_ = 0; return 0; }
        break;
      case 4: setNonBlocking(lfd); port_ = port; break;
      case 5: { // optional: answer "PS4CHESS?" broadcasts so the Android app can find this console by itself
        ufd = socket(AF_INET, SOCK_DGRAM, 0);
        if (ufd >= 0) {
          int one = 1;
          setsockopt(ufd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
          setsockopt(ufd, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof(one));
          sockaddr_in a; memset(&a, 0, sizeof(a));
          a.sin_family = AF_INET; a.sin_port = htons((uint16_t)(port + 1)); a.sin_addr.s_addr = htonl(INADDR_ANY);
          if (bind(ufd, (sockaddr *)&a, sizeof(a)) < 0) { close(ufd); ufd = -1; } else setNonBlocking(ufd);
        }
        break; }
      default: return 2;
    }
    stage_++;
    return stage_ >= 6 ? 2 : 1;
  }
  int stage() const { return stage_; }
  static const char *stageName(int s) {
    static const char *n[] = {"create socket", "set options", "bind port", "listen", "non-blocking", "discovery", "ready"};
    return n[s < 0 ? 0 : s > 6 ? 6 : s];
  }
  bool start(int port, std::string *err = 0) {
    stop();
    for (;;) {
      int r = startStep(port, err);
      if (r == 0) return false;
      if (r == 2) return true;
    }
  }
  void stop() {
    for (size_t i = 0; i < cl.size(); i++) close(cl[i].fd);
    cl.clear();
    if (lfd >= 0) { close(lfd); lfd = -1; }
    if (ufd >= 0) { close(ufd); ufd = -1; }
    stage_ = 0;
  }
  bool running() const { return lfd >= 0 && stage_ >= 6; }

  void poll(uint32_t now = 0) {
    if (!running()) return;
    if (!now) now = nowMs();
    for (int guard = 0; ufd >= 0 && guard < 8 && fdReady(ufd, false); guard++) {
      char pb[64]; sockaddr_in from; socklen_t fl = sizeof(from);
      ssize_t n = recvfrom(ufd, pb, sizeof(pb), 0, (sockaddr *)&from, &fl);
      if (n < 0) break;
      if (n >= 9 && memcmp(pb, "PS4CHESS?", 9) == 0) {
        std::string reply = "PS4CHESS|" + std::to_string(port_) + "|PS4 Chess";
        sendto(ufd, reply.data(), reply.size(), 0, (sockaddr *)&from, fl);
      }
    }
    for (int guard = 0; guard < 8 && fdReady(lfd, false); guard++) {
      int fd = accept(lfd, 0, 0);
      if (fd < 0) break;
      if (cl.size() >= 8) { close(fd); continue; }
      setNonBlocking(fd);
      int one = 1;
      setsockopt(fd, IPPROTO_TCP_NODELAY_LEVEL(), TCP_NODELAY_OPT(), (const char *)&one, sizeof(one));
      Client c; c.fd = fd; c.id = nextId++; c.ws = false; c.closing = false; c.lastRx = now; c.born = now;
      cl.push_back(c);
    }
    for (size_t i = 0; i < cl.size();) {
      Client &c = cl[i];
      bool dead = false;
      char buf[4096];
      for (int guard = 0; guard < 16 && fdReady(c.fd, false); guard++) {
        ssize_t n = recv(c.fd, buf, sizeof(buf), RECV_FLAGS());
        if (n > 0) { c.lastRx = now; c.in.append(buf, (size_t)n); if (c.in.size() > 70000) { dead = true; break; } }
        else if (n == 0) { dead = true; break; }
        else { if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) dead = true; break; }
      }
      if (!dead) process(c);
      if (!c.out.empty() && fdReady(c.fd, true)) {
        ssize_t n = ::send(c.fd, c.out.data(), c.out.size(), SEND_FLAGS());
        if (n > 0) c.out.erase(0, (size_t)n);
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) dead = true;
      }
      if (c.closing && c.out.empty()) dead = true;
      if (c.ws && now - c.lastRx > 25000) dead = true;      // phone stopped answering pings
      if (!c.ws && now - c.born > 15000) dead = true;       // stalled HTTP request
      if (dead) {
        if (c.ws) ev.push_back({c.id, 2, ""});
        close(c.fd);
        cl.erase(cl.begin() + (long)i);
      } else i++;
    }
  }

  bool pop(Event &e) { if (ev.empty()) return false; e = ev.front(); ev.pop_front(); return true; }
  void send(int id, const std::string &text) {
    for (size_t i = 0; i < cl.size(); i++) if (cl[i].id == id && cl[i].ws && !cl[i].closing) cl[i].out += wsFrame(text);
  }
  void broadcast(const std::string &text) {
    for (size_t i = 0; i < cl.size(); i++) if (cl[i].ws && !cl[i].closing) cl[i].out += wsFrame(text);
  }
  // milliseconds since this client last sent anything (large for unknown ids)
  uint32_t idleMs(int id, uint32_t now = 0) const {
    if (!now) now = nowMs();
    for (size_t i = 0; i < cl.size(); i++) if (cl[i].id == id) return now - cl[i].lastRx;
    return 0xFFFFFFFFu;
  }
  void kick(int id) { for (size_t i = 0; i < cl.size(); i++) if (cl[i].id == id) { cl[i].closing = true; cl[i].out.clear(); cl[i].ws = false; } }
  void addRoute(const std::string &path, const std::string &mime, const std::string &body) { Route r; r.path = path; r.mime = mime; r.body = body; routes.push_back(r); }
  int wsCount() const { int n = 0; for (size_t i = 0; i < cl.size(); i++) if (cl[i].ws) n++; return n; }

 private:
  int lfd, ufd, nextId, stage_, port_;
  std::vector<Client> cl;
  std::deque<Event> ev;
  std::vector<Route> routes;

  static int IPPROTO_TCP_NODELAY_LEVEL() { return 6; } // IPPROTO_TCP
  static int TCP_NODELAY_OPT() { return 1; }           // TCP_NODELAY
  static int SEND_FLAGS() {
    int f = 0;
#ifdef MSG_NOSIGNAL
    f |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
    f |= MSG_DONTWAIT;
#endif
    return f;
  }
  static int RECV_FLAGS() {
#ifdef MSG_DONTWAIT
    return MSG_DONTWAIT;
#else
    return 0;
#endif
  }

  void process(Client &c) {
    if (!c.ws && !c.closing) {
      size_t e = c.in.find("\r\n\r\n");
      if (e == std::string::npos) return;
      std::string req = c.in.substr(0, e + 2);
      c.in.erase(0, e + 4);
      std::string line = req.substr(0, req.find("\r\n"));
      std::string path = "/";
      size_t s1 = line.find(' '), s2 = line.find(' ', s1 + 1);
      if (s1 != std::string::npos && s2 != std::string::npos) path = line.substr(s1 + 1, s2 - s1 - 1);
      std::string upgrade = lower(headerValue(req, "Upgrade")), key = headerValue(req, "Sec-WebSocket-Key");
      if (upgrade == "websocket" && !key.empty()) {
        c.out += "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
                 wsAccept(key) + "\r\n\r\n";
        c.ws = true;
        ev.push_back({c.id, 1, ""});
      } else if (path == "/" || path.compare(0, 2, "/?") == 0 || path == "/index.html") {
        c.out += "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-cache\r\nConnection: close\r\nContent-Length: " +
                 std::to_string(html.size()) + "\r\n\r\n" + html;
        c.closing = true;
      } else if (path == "/test") {
        std::string body = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>PS4 Chess test</title></head>"
          "<body style=\"background:#fff;color:#000;font:20px sans-serif;padding:20px\"><h2 style=\"color:#2e7d32\">PS4 Chess server is reachable</h2>"
          "<p>If you can read this, the network between this device and the PS4 works.</p><p><a href=\"/\">Open the chess board</a></p></body></html>";
        c.out += "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-cache\r\nConnection: close\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body; c.closing = true;
      } else if (path == "/ping") {
        c.out += "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-cache\r\nConnection: close\r\nContent-Length: 8\r\n\r\nPS4CHESS"; c.closing = true;
      } else if (serveRoute(c, path)) {
      } else if (path == "/favicon.ico") {
        c.out += "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"; c.closing = true;
      } else {
        c.out += "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"; c.closing = true;
      }
    }
    if (c.ws) parseFrames(c);
  }

  bool serveRoute(Client &c, const std::string &path) {
    for (size_t i = 0; i < routes.size(); i++) if (routes[i].path == path) {
      c.out += "HTTP/1.1 200 OK\r\nContent-Type: " + routes[i].mime + "\r\nCache-Control: no-cache\r\nConnection: close\r\nContent-Length: " +
               std::to_string(routes[i].body.size()) + "\r\n\r\n" + routes[i].body;
      c.closing = true; return true;
    }
    return false;
  }

  void parseFrames(Client &c) {
    for (;;) {
      const std::string &b = c.in;
      if (b.size() < 2) return;
      uint8_t b0 = (uint8_t)b[0], b1 = (uint8_t)b[1];
      bool fin = (b0 & 0x80) != 0; int op = b0 & 15; bool masked = (b1 & 0x80) != 0;
      uint64_t len = b1 & 127; size_t pos = 2;
      if (len == 126) { if (b.size() < 4) return; len = ((uint8_t)b[2] << 8) | (uint8_t)b[3]; pos = 4; }
      else if (len == 127) { if (b.size() < 10) return; len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | (uint8_t)b[2 + i]; pos = 10; }
      if (len > 60000) { c.closing = true; c.in.clear(); return; }
      uint8_t mk[4] = {0, 0, 0, 0};
      if (masked) { if (b.size() < pos + 4) return; for (int i = 0; i < 4; i++) mk[i] = (uint8_t)b[pos + i]; pos += 4; }
      if (b.size() < pos + len) return;
      std::string pl = b.substr(pos, (size_t)len);
      if (masked) for (size_t i = 0; i < pl.size(); i++) pl[i] ^= (char)mk[i & 3];
      c.in.erase(0, pos + (size_t)len);
      if (op == 8) { c.out += wsFrame("", 8); c.closing = true; c.in.clear(); return; }
      else if (op == 9) c.out += wsFrame(pl, 10);
      else if (op == 1 || op == 0) {
        c.frag += pl;
        if (fin) { ev.push_back({c.id, 0, c.frag}); c.frag.clear(); }
        if (c.frag.size() > 60000) { c.closing = true; return; }
      }
    }
  }
};

inline std::string localIp() {
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) return "";
  sockaddr_in a; memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET; a.sin_port = htons(80); inet_pton(AF_INET, "8.8.8.8", &a.sin_addr);
  std::string ip;
  if (connect(s, (sockaddr *)&a, sizeof(a)) == 0) {
    sockaddr_in me; socklen_t l = sizeof(me);
    if (getsockname(s, (sockaddr *)&me, &l) == 0) { char buf[32]; if (inet_ntop(AF_INET, &me.sin_addr, buf, sizeof(buf))) ip = buf; }
  }
  close(s);
  return ip;
}

} // namespace net
