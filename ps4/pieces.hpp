// Software-rasterised anti-aliased chess piece sprites (no image files needed).
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <vector>

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
#define POLY(v, ...) do { static const float pts_[] = {__VA_ARGS__}; poly(v, pts_, (int)(sizeof(pts_) / sizeof(float) / 2)); } while (0)

// piece types 1..6 = pawn knight bishop rook queen king; coordinates in a 100x100 box
inline void shapes(int type, std::vector<Poly> &fill, std::vector<Poly> &detail) {
  const bool needBase = true;
  if (type == 1) { // pawn
    ellipse(fill, 50, 29, 12, 12);
    POLY(fill, 38,42, 62,42, 60,48, 40,48);
    POLY(fill, 41,47, 59,47, 66,72, 34,72);
    POLY(fill, 28,72, 72,72, 76,80, 24,80);
    POLY(fill, 22,80, 78,80, 82,90, 18,90);
  } else if (type == 2) { // knight (faces left)
    POLY(fill, 24,80, 30,60, 40,48, 46,42, 40,38, 30,44, 22,44, 20,38, 28,30, 36,20, 38,10, 46,16, 50,12, 52,18, 64,22, 74,38, 78,60, 76,80);
    POLY(fill, 20,80, 80,80, 83,90, 17,90);
    POLY(detail, 44,26, 49,26, 49,31, 44,31);
    POLY(detail, 26,38, 30,38, 30,42, 26,42);
  } else if (type == 3) { // bishop
    ellipse(fill, 50, 13, 5, 5);
    ellipse(fill, 50, 38, 14, 21);
    POLY(fill, 36,55, 64,55, 66,63, 34,63);
    POLY(fill, 40,62, 60,62, 66,72, 34,72);
    POLY(fill, 28,72, 72,72, 76,80, 24,80);
    POLY(fill, 22,80, 78,80, 82,90, 18,90);
    POLY(detail, 46,28, 49,26, 58,40, 55,43);
  } else if (type == 4) { // rook
    POLY(fill, 27,12, 38,12, 38,19, 44,19, 44,12, 56,12, 56,19, 62,19, 62,12, 73,12, 73,32, 27,32);
    POLY(fill, 33,31, 67,31, 63,40, 37,40);
    POLY(fill, 37,39, 63,39, 68,72, 32,72);
    POLY(fill, 27,72, 73,72, 77,80, 23,80);
    POLY(fill, 21,80, 79,80, 83,90, 17,90);
  } else if (type == 5) { // queen
    ellipse(fill, 20, 24, 5, 5); ellipse(fill, 35, 15, 5, 5); ellipse(fill, 50, 11, 5, 5);
    ellipse(fill, 65, 15, 5, 5); ellipse(fill, 80, 24, 5, 5);
    POLY(fill, 20,26, 34,52, 36,17, 44,50, 50,13, 56,50, 64,17, 66,52, 80,26, 74,70, 26,70);
    POLY(fill, 27,68, 73,68, 77,80, 23,80);
    POLY(fill, 21,80, 79,80, 83,90, 17,90);
  } else { // king
    POLY(fill, 46,4, 54,4, 54,11, 61,11, 61,18, 54,18, 54,26, 46,26, 46,18, 39,18, 39,11, 46,11);
    ellipse(fill, 50, 38, 21, 14);
    POLY(fill, 30,40, 70,40, 76,72, 24,72);
    POLY(fill, 26,70, 74,70, 78,80, 22,80);
    POLY(fill, 21,80, 79,80, 83,90, 17,90);
    POLY(detail, 30,38, 70,38, 70,41, 30,41);
  }
  (void)needBase;
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
  const int fr = color == 0 ? 250 : 70, fg = color == 0 ? 250 : 70, fb = color == 0 ? 250 : 76;
  const int orr = color == 0 ? 28 : 14, og = color == 0 ? 28 : 14, ob = color == 0 ? 30 : 16;
  out.assign((size_t)N * N, 0);
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int a = 0, r = 0, g = 0, b = 0;
    for (int sy = 0; sy < SS; sy++) for (int sx = 0; sx < SS; sx++) {
      size_t i = (size_t)(y * SS + sy) * M + (x * SS + sx);
      if (cov[i]) { a++; if (det[i]) { r += orr; g += og; b += ob; } else { r += fr; g += fg; b += fb; } }
      else if (dil[i]) { a++; r += orr; g += og; b += ob; }
    }
    if (!a) continue;
    uint32_t alpha = (uint32_t)(a * 255 / (SS * SS));
    out[(size_t)y * N + x] = (alpha << 24) | ((uint32_t)(r / a) << 16) | ((uint32_t)(g / a) << 8) | (uint32_t)(b / a);
  }
}

} // namespace gfx
