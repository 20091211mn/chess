// LAN room: one controlling client (the phone) plus spectators. Server-authoritative:
// the phone sends moves, the PS4 validates them with the same rules engine.
//
// client -> server (text):  hello | ping | hist | move <uci> | resign | new | draw | draw_yes | draw_no
// server -> client (JSON):  {"t":"state",...}  {"t":"pong"}  {"t":"hist","fens":[...]}
#pragma once
#include "engine.hpp"
#include "net.hpp"

namespace ce {

struct StateExtra { int evalCp; bool hasEval; int clients; StateExtra() : evalCp(0), hasEval(false), clients(0) {} };

inline std::string jsonState(const Game &g, int you, const std::string &lastUci, const std::string &err = "", const StateExtra &x = StateExtra()) {
  std::string s = "{\"t\":\"state\",\"fen\":\"" + toFen(g.pos) + "\",\"turn\":\"" + (g.pos.side == WHITE ? "w" : "b") + "\",\"legal\":[";
  Move L[256];
  int n = g.over() ? 0 : g.legal(L);
  for (int i = 0; i < n; i++) { if (i) s += ','; s += "\"" + toUci(L[i]) + "\""; }
  s += "],\"last\":\"" + lastUci + "\",\"check\":" + (g.check() ? "true" : "false") + ",\"san\":[";
  for (size_t i = 0; i < g.san.size(); i++) { if (i) s += ','; s += "\"" + g.san[i] + "\""; }
  s += "],\"res\":" + std::to_string(g.result) + ",\"reason\":\"" + g.reason + "\",\"you\":\"" +
       (you == WHITE ? "w" : you == BLACK ? "b" : "s") + "\",\"wt\":" + std::to_string(g.initMs > 0 ? (long long)g.clk[0] : -1LL) +
       ",\"bt\":" + std::to_string(g.initMs > 0 ? (long long)g.clk[1] : -1LL) + ",\"plies\":" + std::to_string(g.plies()) +
       ",\"draw\":" + std::to_string(g.drawOffer) + ",\"clients\":" + std::to_string(x.clients);
  if (x.hasEval) s += ",\"ev\":" + std::to_string(x.evalCp);
  if (!err.empty()) s += ",\"err\":\"" + err + "\"";
  return s + "}";
}

inline std::string jsonHist(const Game &g) {
  std::string s = "{\"t\":\"hist\",\"n\":" + std::to_string(g.plies()) + ",\"fens\":[";
  for (size_t i = 0; i < g.hist.size(); i++) { s += "\"" + toFen(g.hist[i]) + "\","; }
  s += "\"" + toFen(g.pos) + "\"]}";
  return s;
}

struct LanEvents {
  bool moved, newGameReq, resigned, connected, disconnected, drawOffered, drawAnswered;
  LanEvents() : moved(false), newGameReq(false), resigned(false), connected(false), disconnected(false), drawOffered(false), drawAnswered(false) {}
};

struct LanRoom {
  net::Server srv;
  int phoneSide;   // side controlled by the phone
  int phoneId;     // controlling client id (0 = none)
  std::string last;
  std::string lockMsg; // non-empty: game not started yet, actions are refused with this message
  std::vector<int> spectators;
  StateExtra extra;

  LanRoom() : phoneSide(BLACK), phoneId(0) {}
  bool start(int port, std::string *err = 0) { return srv.start(port, err); }
  void stop() { srv.stop(); phoneId = 0; spectators.clear(); }
  bool connected() const { return phoneId != 0; }
  int clients() const { return (phoneId ? 1 : 0) + (int)spectators.size(); }

  void sendState(const Game &g, int id, const std::string &err = "") {
    extra.clients = clients();
    srv.send(id, jsonState(g, id == phoneId ? phoneSide : 2, last, err, extra));
  }
  void broadcastState(const Game &g) {
    extra.clients = clients();
    if (phoneId) srv.send(phoneId, jsonState(g, phoneSide, last, lockMsg, extra));
    if (!spectators.empty()) {
      std::string sp = jsonState(g, 2, last, lockMsg, extra);
      for (size_t i = 0; i < spectators.size(); i++) srv.send(spectators[i], sp);
    }
  }

  // process network; mutates the game when the phone moves / resigns / answers a draw offer
  void update(Game &g, LanEvents &out, uint32_t now = 0) {
    if (!now) now = net::nowMs();
    srv.poll(now);
    net::Event e;
    bool dirty = false;
    while (srv.pop(e)) {
      if (e.type == 1) {
        // a phone that went to sleep leaves a half-open connection behind: let the new one take over
        if (phoneId && srv.idleMs(phoneId, now) > 8000) { srv.kick(phoneId); phoneId = 0; }
        if (!phoneId) phoneId = e.id; else spectators.push_back(e.id);
        out.connected = true; dirty = true; // the broadcast below also greets the new client
      } else if (e.type == 2) {
        if (e.id == phoneId) { phoneId = 0; out.disconnected = true; if (!spectators.empty()) { phoneId = spectators.front(); spectators.erase(spectators.begin()); } }
        else for (size_t i = 0; i < spectators.size(); i++) if (spectators[i] == e.id) { spectators.erase(spectators.begin() + (long)i); break; }
        dirty = true;
      } else {
        const std::string &m = e.msg;
        if (m == "ping") { srv.send(e.id, "{\"t\":\"pong\"}"); continue; }
        if (!lockMsg.empty()) { sendState(g, e.id, lockMsg); continue; }
        bool isPhone = e.id == phoneId;
        if (m.compare(0, 5, "move ") == 0) {
          std::string u = m.substr(5);
          if (!isPhone) sendState(g, e.id, "spectator");
          else if (g.over()) sendState(g, e.id, "game over");
          else if (g.pos.side != phoneSide) sendState(g, e.id, "not your turn");
          else {
            Move mv;
            if (parseUci(g.pos, u, mv) && g.play(mv)) { last = toUci(mv); out.moved = true; dirty = true; }
            else sendState(g, e.id, "illegal move");
          }
        } else if (m == "resign") {
          if (isPhone && !g.over()) { g.resign(phoneSide); out.resigned = true; dirty = true; }
        } else if (m == "new") {
          if (isPhone && g.over()) out.newGameReq = true;
        } else if (m == "draw") {
          if (isPhone && !g.over() && g.offerDraw(phoneSide)) { out.drawOffered = true; dirty = true; }
        } else if (m == "draw_yes" || m == "draw_no") {
          if (isPhone && g.drawOffer == (phoneSide ^ 1)) { g.answerDraw(m == "draw_yes"); out.drawAnswered = true; dirty = true; }
        } else if (m == "hist") {
          srv.send(e.id, jsonHist(g));
        } else sendState(g, e.id);
      }
    }
    if (dirty) broadcastState(g);
  }
};

} // namespace ce
