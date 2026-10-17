// LAN room: one controlling client (the phone) plus spectators. Server-authoritative:
// the phone sends moves, the PS4 validates them with the same rules engine.
#pragma once
#include "engine.hpp"
#include "net.hpp"

namespace ce {

inline std::string jsonState(const Game &g, int you, const std::string &lastUci, const std::string &err = "") {
  std::string s = "{\"t\":\"state\",\"fen\":\"" + toFen(g.pos) + "\",\"turn\":\"" + (g.pos.side == WHITE ? "w" : "b") + "\",\"legal\":[";
  Move L[256];
  int n = g.over() ? 0 : g.legal(L);
  for (int i = 0; i < n; i++) { if (i) s += ','; s += "\"" + toUci(L[i]) + "\""; }
  s += "],\"last\":\"" + lastUci + "\",\"check\":" + (g.check() ? "true" : "false") + ",\"san\":[";
  for (size_t i = 0; i < g.san.size(); i++) { if (i) s += ','; s += "\"" + g.san[i] + "\""; }
  s += "],\"res\":" + std::to_string(g.result) + ",\"reason\":\"" + g.reason + "\",\"you\":\"" +
       (you == WHITE ? "w" : you == BLACK ? "b" : "s") + "\",\"wt\":" + std::to_string(g.initMs > 0 ? (long long)g.clk[0] : -1LL) +
       ",\"bt\":" + std::to_string(g.initMs > 0 ? (long long)g.clk[1] : -1LL) + ",\"plies\":" + std::to_string(g.plies());
  if (!err.empty()) s += ",\"err\":\"" + err + "\"";
  return s + "}";
}

struct LanEvents { bool moved, newGameReq, resigned, connected, disconnected; LanEvents() : moved(false), newGameReq(false), resigned(false), connected(false), disconnected(false) {} };

struct LanRoom {
  net::Server srv;
  int phoneSide;   // side controlled by the phone
  int phoneId;     // controlling client id (0 = none)
  std::string last;
  std::string lockMsg; // non-empty: game not started yet, moves are refused with this message

  LanRoom() : phoneSide(BLACK), phoneId(0) {}
  bool start(int port, std::string *err = 0) { return srv.start(port, err); }
  void stop() { srv.stop(); phoneId = 0; }
  bool connected() const { return phoneId != 0; }

  void sendState(const Game &g, int id, const std::string &err = "") {
    srv.send(id, jsonState(g, id == phoneId ? phoneSide : 2, last, err));
  }
  void broadcastState(const Game &g) {
    // each client gets its own "you"
    net::Event dummy; (void)dummy;
    for (int pass = 0; pass < 2; pass++) {
      // controller first, then spectators (spectator view is identical for everyone)
      if (pass == 0 && phoneId) srv.send(phoneId, jsonState(g, phoneSide, last));
      if (pass == 1) {
        std::string sp = jsonState(g, 2, last);
        for (size_t i = 0; i < spectators.size(); i++) srv.send(spectators[i], sp);
      }
    }
  }

  // process network; mutates the game when the phone moves
  void update(Game &g, LanEvents &out) {
    srv.poll();
    net::Event e;
    while (srv.pop(e)) {
      if (e.type == 1) {
        if (!phoneId) phoneId = e.id; else spectators.push_back(e.id);
        out.connected = true;
        sendState(g, e.id, lockMsg);
      } else if (e.type == 2) {
        if (e.id == phoneId) { phoneId = 0; out.disconnected = true; if (!spectators.empty()) { phoneId = spectators.front(); spectators.erase(spectators.begin()); } }
        else for (size_t i = 0; i < spectators.size(); i++) if (spectators[i] == e.id) { spectators.erase(spectators.begin() + (long)i); break; }
      } else {
        const std::string &m = e.msg;
        if (!lockMsg.empty()) { sendState(g, e.id, lockMsg); continue; }
        if (m.compare(0, 5, "move ") == 0) {
          std::string u = m.substr(5);
          if (e.id != phoneId) sendState(g, e.id, "spectator");
          else if (g.over()) sendState(g, e.id, "game over");
          else if (g.pos.side != phoneSide) sendState(g, e.id, "not your turn");
          else {
            Move mv;
            if (parseUci(g.pos, u, mv) && g.play(mv)) { last = toUci(mv); out.moved = true; }
            else sendState(g, e.id, "illegal move");
          }
        } else if (m == "resign") {
          if (e.id == phoneId && !g.over()) { g.resign(phoneSide); out.resigned = true; }
        } else if (m == "new") {
          if (e.id == phoneId && g.over()) out.newGameReq = true;
        } else sendState(g, e.id);
      }
    }
  }
  std::vector<int> spectators;
};

} // namespace ce
