// Software-rasterised anti-aliased chess piece sprites (no image files needed).
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <vector>
#include "piecedata.hpp"

namespace gfx {

struct Poly { std::vector<float> p; float x0, y0, x1, y1; };

inline void finish(Poly &q) {
  q.x0 = q.y0 = 1e9f; q.x1 = q.y1 = -1e9f;
  for (size_t i = 0; i + 1 < q.p.size(); i += 2) {
    if (q.p[i] < q.x0) q.x0 = q.p[i]; if (q.p[i] > q.x1) q.x1 = q.p[i];
    if (q.p[i+1] < q.y0) q.y0 = q.p[i+1]; if (q.p[i+1] > q.y1) q.y1 = q.p[i+1];
  }
}
inline void poly(std::vector<Poly> &v, const float *pts, int n) {
  Poly q; q.p.assign(pts, pts + n * 2); finish(q); v.push_back(q);
}
inline void ellipse(std::vector<Poly> &v, float cx, float cy, float rx, float ry) {
  Poly q;
  for (int i = 0; i < 32; i++) { float a = (float)i * 6.2831853f / 32.f; q.p.push_back(cx + rx * cosf(a)); q.p.push_back(cy + ry * sinf(a)); }
  finish(q); v.push_back(q);
}
// piece types 1..6 = pawn knight bishop rook queen king; shapes come from piecedata.hpp (tools/piece_shapes.py)
inline void shapes(int type, std::vector<Poly> &fill, std::vector<Poly> &detail) {
  const PS &ps = PIECE_SHAPES[type];
  for (int i = 0; i < ps.n; i++) poly(ps.p[i].detail ? detail : fill, ps.p[i].pts, ps.p[i].n);
}

inline bool inside(const std::vector<Poly> &v, float x, float y) {
  for (size_t k = 0; k < v.size(); k++) {
    const Poly &q = v[k];
    if (x < q.x0 || x > q.x1 || y < q.y0 || y > q.y1) continue;
    bool in = false;
    size_t n = q.p.size() / 2;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
      float xi = q.p[i*2], yi = q.p[i*2+1], xj = q.p[j*2], yj = q.p[j*2+1];
      if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) in = !in;
    }
    if (in) return true;
  }
  return false;
}

// out: N*N ARGB8888 pixels. color 0 = white piece, 1 = black piece
inline void renderPiece(std::vector<uint32_t> &out, int N, int type, int color) {
  const int SS = 3, M = N * SS, R = SS; // outline radius = 1 output pixel
  std::vector<Poly> fill, detail;
  shapes(type, fill, detail);
  std::vector<uint8_t> cov((size_t)M * M, 0), det((size_t)M * M, 0), tmp((size_t)M * M, 0), dil((size_t)M * M, 0);
  for (int y = 0; y < M; y++) for (int x = 0; x < M; x++) {
    float fx = ((float)x + 0.5f) * 100.f / (float)M, fy = ((float)y + 0.5f) * 100.f / (float)M;
    if (inside(fill, fx, fy)) { cov[(size_t)y * M + x] = 1; if (!detail.empty() && inside(detail, fx, fy)) det[(size_t)y * M + x] = 1; }
  }
  // separable square dilation
  for (int y = 0; y < M; y++) for (int x = 0; x < M; x++) {
    uint8_t v = 0;
    for (int d = -R; d <= R && !v; d++) { int xx = x + d; if (xx >= 0 && xx < M && cov[(size_t)y * M + xx]) v = 1; }
    tmp[(size_t)y * M + x] = v;
  }
  for (int y = 0; y < M; y++) for (int x = 0; x < M; x++) {
    uint8_t v = 0;
    for (int d = -R; d <= R && !v; d++) { int yy = y + d; if (yy >= 0 && yy < M && tmp[(size_t)yy * M + x]) v = 1; }
    dil[(size_t)y * M + x] = v;
  }
  const int orr = color == 0 ? 28 : 14, og = color == 0 ? 28 : 14, ob = color == 0 ? 30 : 16;
  out.assign((size_t)N * N, 0);
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int a = 0, r = 0, g = 0, b = 0;
    for (int sy = 0; sy < SS; sy++) for (int sx = 0; sx < SS; sx++) {
      size_t i = (size_t)(y * SS + sy) * M + (x * SS + sx);
      if (cov[i]) {
        a++;
        if (det[i]) { r += orr; g += og; b += ob; }
        else {
          float fy = ((float)(y * SS + sy) + 0.5f) * 100.f / (float)M, fx = ((float)(x * SS + sx) + 0.5f) * 100.f / (float)M;
          float v = color == 0 ? 252.f - 0.30f * fy + (50.f - fx) * 0.18f : 100.f - 0.55f * fy + (50.f - fx) * 0.32f;
          float lo = color == 0 ? 190.f : 26.f, hi = color == 0 ? 255.f : 118.f;
          if (v < lo) v = lo; if (v > hi) v = hi;
          int iv = (int)v; r += iv; g += iv; b += iv + (color ? 6 : 2);
        }
      }
      else if (dil[i]) { a++; r += orr; g += og; b += ob; }
    }
    if (!a) continue;
    uint32_t alpha = (uint32_t)(a * 255 / (SS * SS));
    out[(size_t)y * N + x] = (alpha << 24) | ((uint32_t)(r / a) << 16) | ((uint32_t)(g / a) << 8) | (uint32_t)(b / a);
  }
}

} // namespace gfx
