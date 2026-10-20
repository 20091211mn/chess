// Builds one RGBA glyph atlas from the embedded Poppins data (4-bit alpha, base64). SDL-free.
#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "fontdata.hpp"

namespace txt {

struct Rect { int x, y, w, h, bx, by, adv; };
struct Face { int px, asc, lh; Rect g[95]; };
struct Atlas { int w, h; std::vector<uint32_t> px; Face face[7]; };
static const int NFACES = 7;

inline int b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  return c == '+' ? 62 : c == '/' ? 63 : -1;
}

inline void build(Atlas &at) {
  const int AW = 1024;
  // decode every face to a byte stream, pack glyphs with a simple shelf packer
  std::vector<std::vector<uint8_t> > dec(NFACES);
  int x = 0, y = 0, rowH = 0;
  struct Place { int f, g, x, y; };
  std::vector<Place> places;
  for (int f = 0; f < NFACES; f++) {
    const fontdata::F &F = fontdata::FACES[f];
    std::string b64;
    for (int i = 0; F.parts[i]; i++) b64 += F.parts[i];
    std::vector<uint8_t> &d = dec[f];
    uint32_t acc = 0; int bits = 0;
    for (size_t i = 0; i < b64.size(); i++) {
      int v = b64val(b64[i]); if (v < 0) continue;
      acc = (acc << 6) | (uint32_t)v; bits += 6;
      if (bits >= 8) { bits -= 8; d.push_back((uint8_t)((acc >> bits) & 255)); }
    }
    at.face[f].px = F.px; at.face[f].asc = F.asc; at.face[f].lh = F.lh;
    for (int g = 0; g < 95; g++) {
      const fontdata::G &G = F.g[g];
      Rect &r = at.face[f].g[g];
      r.w = G.w; r.h = G.h; r.bx = G.bx; r.by = G.by; r.adv = G.adv; r.x = r.y = 0;
      if (!G.w || !G.h) continue;
      if (x + G.w + 1 > AW) { x = 0; y += rowH + 1; rowH = 0; }
      r.x = x; r.y = y; x += G.w + 1; if (G.h > rowH) rowH = G.h;
      Place p = {f, g, r.x, r.y}; places.push_back(p);
    }
  }
  at.w = AW; at.h = y + rowH + 1;
  at.px.assign((size_t)at.w * at.h, 0x00FFFFFFu);
  for (size_t i = 0; i < places.size(); i++) {
    const Place &p = places[i];
    const fontdata::G &G = fontdata::FACES[p.f].g[p.g];
    const std::vector<uint8_t> &d = dec[p.f];
    for (int j = 0; j < G.w * G.h; j++) {
      uint32_t n = G.off + (uint32_t)j;
      uint8_t byte = (n >> 1) < d.size() ? d[n >> 1] : 0;
      uint32_t nib = (n & 1) ? (byte & 15) : (byte >> 4);
      uint32_t a = nib * 17;
      at.px[(size_t)(p.y + j / G.w) * at.w + p.x + j % G.w] = (a << 24) | 0x00FFFFFFu;
    }
  }
}

inline int width(const Atlas &at, int face, const char *s) {
  int w = 0;
  for (; *s; s++) { int g = (unsigned char)*s - 32; if (g < 0 || g > 94) g = '?' - 32; w += at.face[face].g[g].adv; }
  return w;
}

} // namespace txt
