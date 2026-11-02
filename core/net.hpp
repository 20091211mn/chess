// Plain TCP "line" server for the Android app (no HTTP, no WebSocket) + UDP discovery responder.
//
//  * TCP: every message is one line of text ending in '\n'. The server greets each new connection with
//    {"t":"hello","app":"PS4CHESS","v":1,...} so the phone can recognise the console immediately.
//  * UDP: a datagram "PS4CHESS?" is answered with "PS4CHESS|<port>|PS4 Chess" (the app finds the PS4 by itself).
//
// Nothing here can block the game loop: sockets are non-blocking, every call uses MSG_DONTWAIT, and as a second
// safety net every socket gets 1 ms send/receive timeouts. select()/poll() are not used at all.
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
#include <sys/ioctl.h>

namespace net {

inline uint32_t nowMs() { timeval tv; gettimeofday(&tv, 0); return (uint32_t)(tv.tv_sec * 1000u + tv.tv_usec / 1000u); }
inline uint64_t nowUs() { timeval tv; gettimeofday(&tv, 0); return (uint64_t)tv.tv_sec * 1000000ull + (uint64_t)tv.tv_usec; }

// "nothing to read/write right now" - not an error
inline bool wouldBlock(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINTR || e == ETIMEDOUT; }

inline void makeNonBlocking(int fd) {
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
#ifdef FIONBIO
  int one = 1;
  ioctl(fd, FIONBIO, &one);                 // some platforms only honour this one
#endif
  timeval tv; tv.tv_sec = 0; tv.tv_usec = 1000;   // belt and braces: even a blocking call returns after ~1 ms
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
}

inline int dontWait() {
#ifdef MSG_DONTWAIT
  int f = MSG_DONTWAIT;
#else
  int f = 0;
#endif
#ifdef MSG_NOSIGNAL
  f |= MSG_NOSIGNAL;
#endif
  return f;
}

struct Client { int fd, id; bool closing; uint32_t lastRx; std::string in, out; };
struct Event { int id; int type; std::string msg; };   // type: 0 line received, 1 connected, 2 disconnected

class Server {
 public:
  Server() : lfd(-1), ufd(-1), nextId(1), stage_(0), port_(0), maxPollUs(0) {}
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
      case 4: makeNonBlocking(lfd); port_ = port; break;
      case 5: { // optional: answer "PS4CHESS?" broadcasts so the Android app can find this console by itself
        ufd = socket(AF_INET, SOCK_DGRAM, 0);
        if (ufd >= 0) {
          int one = 1;
          setsockopt(ufd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
          setsockopt(ufd, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof(one));
          sockaddr_in a; memset(&a, 0, sizeof(a));
          a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); a.sin_addr.s_addr = htonl(INADDR_ANY);
          if (bind(ufd, (sockaddr *)&a, sizeof(a)) < 0) { close(ufd); ufd = -1; } else makeNonBlocking(ufd);
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
    for (;;) { int r = startStep(port, err); if (r == 0) return false; if (r == 2) return true; }
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
    uint64_t t0 = nowUs();
    if (!now) now = nowMs();
    // discovery probes
    for (int guard = 0; ufd >= 0 && guard < 8; guard++) {
      char pb[64]; sockaddr_in from; socklen_t fl = sizeof(from);
      ssize_t n = recvfrom(ufd, pb, sizeof(pb), dontWait(), (sockaddr *)&from, &fl);
      if (n < 0) break;
      if (n >= 9 && memcmp(pb, "PS4CHESS?", 9) == 0) {
        std::string reply = "PS4CHESS|" + std::to_string(port_) + "|PS4 Chess";
        sendto(ufd, reply.data(), reply.size(), dontWait(), (sockaddr *)&from, fl);
      }
    }
    // new connections
    for (int guard = 0; guard < 4; guard++) {
      int fd = accept(lfd, 0, 0);
      if (fd < 0) break;
      if (cl.size() >= 6) { close(fd); continue; }
      makeNonBlocking(fd);
      int one = 1;
      setsockopt(fd, 6 /* IPPROTO_TCP */, 1 /* TCP_NODELAY */, (const char *)&one, sizeof(one));
      Client c; c.fd = fd; c.id = nextId++; c.closing = false; c.lastRx = now;
      c.out = "{\"t\":\"hello\",\"app\":\"PS4CHESS\",\"v\":1,\"name\":\"PS4 Chess\"}\n";
      cl.push_back(c);
      ev.push_back({cl.back().id, 1, ""});
    }
    for (size_t i = 0; i < cl.size();) {
      Client &c = cl[i];
      bool dead = false;
      char buf[2048];
      for (int guard = 0; guard < 8; guard++) {
        ssize_t n = recv(c.fd, buf, sizeof(buf), dontWait());
        if (n > 0) {
          c.lastRx = now; c.in.append(buf, (size_t)n);
          size_t nl;
          while ((nl = c.in.find('\n')) != std::string::npos) {
            std::string line = c.in.substr(0, nl);
            c.in.erase(0, nl + 1);
            if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
            if (!line.empty()) ev.push_back({c.id, 0, line});
          }
          if (c.in.size() > 4096) { dead = true; break; }     // a line this long is not ours
        } else if (n == 0) { dead = true; break; }
        else { if (!wouldBlock(errno)) dead = true; break; }
      }
      if (!dead && !c.out.empty()) {
        size_t chunk = c.out.size() < 16384 ? c.out.size() : 16384;
        ssize_t n = ::send(c.fd, c.out.data(), chunk, dontWait());
        if (n > 0) c.out.erase(0, (size_t)n);
        else if (n < 0 && !wouldBlock(errno)) dead = true;
      }
      if (c.out.size() > 262144) dead = true;              // a client that never reads must not hold memory forever
      if (c.closing && c.out.empty()) dead = true;
      if (now - c.lastRx > 25000) dead = true;             // the app pings every 4 s
      if (dead) { ev.push_back({c.id, 2, ""}); close(c.fd); cl.erase(cl.begin() + (long)i); }
      else i++;
    }
    uint64_t dt = nowUs() - t0;
    if (dt > maxPollUs) maxPollUs = dt;
  }

  bool pop(Event &e) { if (ev.empty()) return false; e = ev.front(); ev.pop_front(); return true; }
  void send(int id, const std::string &line) { for (size_t i = 0; i < cl.size(); i++) if (cl[i].id == id && !cl[i].closing) cl[i].out += line + "\n"; }
  void broadcast(const std::string &line) { for (size_t i = 0; i < cl.size(); i++) if (!cl[i].closing) cl[i].out += line + "\n"; }
  int clientCount() const { return (int)cl.size(); }
  // milliseconds since this client last sent anything (large for unknown ids)
  uint32_t idleMs(int id, uint32_t now = 0) const {
    if (!now) now = nowMs();
    for (size_t i = 0; i < cl.size(); i++) if (cl[i].id == id) return now - cl[i].lastRx;
    return 0xFFFFFFFFu;
  }
  void kick(int id) { for (size_t i = 0; i < cl.size(); i++) if (cl[i].id == id) { cl[i].closing = true; cl[i].out.clear(); } }
  // slowest poll() since the last call, in microseconds (shown on screen so a stall is visible)
  uint64_t takeMaxPollUs() { uint64_t v = maxPollUs; maxPollUs = 0; return v; }

 private:
  int lfd, ufd, nextId, stage_, port_;
  uint64_t maxPollUs;
  std::vector<Client> cl;
  std::deque<Event> ev;
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
