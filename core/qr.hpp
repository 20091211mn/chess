// Minimal QR Code generator: byte mode, error-correction level L, versions 1-3 (up to 53 bytes).
// Enough for "http://255.255.255.255:8080". Single RS block per version, so no interleaving.
#pragma once
#include <stdint.h>
#include <string>
#include <stdlib.h>
#include <algorithm>
#include <vector>

namespace qr {

struct Code {
  int n;
  std::vector<uint8_t> m;
  Code() : n(0) {}
  bool dark(int x, int y) const { return x >= 0 && y >= 0 && x < n && y < n && m[(size_t)y * n + x]; }
};

namespace detail {
struct Gf {
  uint8_t e[512], l[256];
  Gf() { int x = 1; for (int i = 0; i < 255; i++) { e[i] = (uint8_t)x; l[x] = (uint8_t)i; x <<= 1; if (x & 0x100) x ^= 0x11D; } for (int i = 255; i < 512; i++) e[i] = e[i - 255]; }
  uint8_t mul(int a, int b) const { return (a && b) ? e[l[a] + l[b]] : 0; }
};
inline const Gf &gf() { static Gf g; return g; }

inline std::vector<uint8_t> rsEncode(const std::vector<uint8_t> &data, int ecLen) {
  const Gf &g = gf();
  std::vector<uint8_t> gen(1, 1);
  for (int i = 0; i < ecLen; i++) {           // multiply by (x - alpha^i)
    std::vector<uint8_t> ng(gen.size() + 1, 0);
    for (size_t j = 0; j < gen.size(); j++) { ng[j] ^= gen[j]; ng[j + 1] ^= g.mul(gen[j], g.e[i]); }
    gen = ng;
  }
  std::vector<uint8_t> rem(ecLen, 0);
  for (size_t i = 0; i < data.size(); i++) {
    uint8_t f = data[i] ^ rem[0];
    rem.erase(rem.begin()); rem.push_back(0);
    for (int j = 0; j < ecLen; j++) rem[j] ^= g.mul(gen[j + 1], f);
  }
  return rem;
}

struct Builder {
  int n;
  std::vector<uint8_t> mod, fn;
  explicit Builder(int size) : n(size), mod((size_t)size * size, 0), fn((size_t)size * size, 0) {}
  void set(int x, int y, bool d) { if (x < 0 || y < 0 || x >= n || y >= n) return; mod[(size_t)y * n + x] = d; fn[(size_t)y * n + x] = 1; }
  void finder(int cx, int cy) {
    for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
      int d = std::max(abs(dx), abs(dy));
      set(cx + dx, cy + dy, d != 2 && d != 4);
    }
  }
  void align(int cx, int cy) { for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) set(cx + dx, cy + dy, std::max(abs(dx), abs(dy)) != 1); }
  void format(int mask) {
    int data = (1 << 3) | mask; // ECC level L = 01
    int rem = data;
    for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = ((data << 10) | rem) ^ 0x5412;
    auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };
    for (int i = 0; i <= 5; i++) set(8, i, bit(i));
    set(8, 7, bit(6)); set(8, 8, bit(7)); set(7, 8, bit(8));
    for (int i = 9; i < 15; i++) set(14 - i, 8, bit(i));
    for (int i = 0; i < 8; i++) set(n - 1 - i, 8, bit(i));
    for (int i = 8; i < 15; i++) set(8, n - 15 + i, bit(i));
    set(8, n - 8, true);
  }
};

inline bool maskBit(int mask, int x, int y) {
  switch (mask) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
  }
}

inline int penalty(const std::vector<uint8_t> &m, int n) {
  int p = 0;
  for (int pass = 0; pass < 2; pass++) {
    for (int a = 0; a < n; a++) {
      int run = 1;
      for (int b = 1; b < n; b++) {
        int c0 = pass ? m[(size_t)(b - 1) * n + a] : m[(size_t)a * n + b - 1], c1 = pass ? m[(size_t)b * n + a] : m[(size_t)a * n + b];
        if (c0 == c1) { run++; if (run == 5) p += 3; else if (run > 5) p++; } else run = 1;
      }
    }
  }
  for (int y = 0; y + 1 < n; y++) for (int x = 0; x + 1 < n; x++) {
    int c = m[(size_t)y * n + x];
    if (c == m[(size_t)y * n + x + 1] && c == m[(size_t)(y + 1) * n + x] && c == m[(size_t)(y + 1) * n + x + 1]) p += 3;
  }
  int dark = 0; for (size_t i = 0; i < m.size(); i++) dark += m[i];
  int total = n * n, k = 0, diff = abs(dark * 20 - total * 10);
  while (diff > total) { diff -= total; k++; }
  p += k * 10;
  return p;
}
} // namespace detail

inline bool encode(const std::string &text, Code &out) {
  static const int cap[3] = {17, 32, 53}, dataCw[3] = {19, 34, 55}, ecCw[3] = {7, 10, 15};
  int v = -1;
  for (int i = 0; i < 3; i++) if ((int)text.size() <= cap[i]) { v = i; break; }
  if (v < 0) return false;
  int n = 21 + 4 * v;
  std::vector<uint8_t> bits;
  auto put = [&](int val, int len) { for (int i = len - 1; i >= 0; i--) bits.push_back((uint8_t)((val >> i) & 1)); };
  put(4, 4); put((int)text.size(), 8);
  for (size_t i = 0; i < text.size(); i++) put((uint8_t)text[i], 8);
  size_t maxBits = (size_t)dataCw[v] * 8;
  for (int i = 0; i < 4 && bits.size() < maxBits; i++) bits.push_back(0);
  while (bits.size() % 8) bits.push_back(0);
  std::vector<uint8_t> data;
  for (size_t i = 0; i < bits.size(); i += 8) { int b = 0; for (int j = 0; j < 8; j++) b = (b << 1) | bits[i + j]; data.push_back((uint8_t)b); }
  for (int pad = 0xEC; (int)data.size() < dataCw[v]; pad ^= 0xEC ^ 0x11) data.push_back((uint8_t)pad);
  std::vector<uint8_t> ec = detail::rsEncode(data, ecCw[v]);
  std::vector<uint8_t> all = data; all.insert(all.end(), ec.begin(), ec.end());

  detail::Builder b(n);
  for (int i = 0; i < n; i++) { b.set(6, i, i % 2 == 0); b.set(i, 6, i % 2 == 0); }
  b.finder(3, 3); b.finder(n - 4, 3); b.finder(3, n - 4);
  if (v >= 1) { int c = 4 * (v + 1) + 10; b.align(c, c); }
  b.format(0); // reserve format area (rewritten per mask below)

  // zig-zag data placement
  std::vector<uint8_t> base = b.mod;
  size_t bi = 0;
  for (int right = n - 1; right >= 1; right -= 2) {
    if (right == 6) right = 5;
    for (int vert = 0; vert < n; vert++) for (int j = 0; j < 2; j++) {
      int x = right - j; bool up = ((right + 1) & 2) == 0; int y = up ? n - 1 - vert : vert;
      if (!b.fn[(size_t)y * n + x] && bi < all.size() * 8) { base[(size_t)y * n + x] = (all[bi >> 3] >> (7 - (bi & 7))) & 1; bi++; }
    }
  }
  int bestMask = 0, bestPen = 1 << 30; std::vector<uint8_t> best;
  for (int mask = 0; mask < 8; mask++) {
    detail::Builder t(n); t.fn = b.fn; t.mod = base;
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) if (!t.fn[(size_t)y * n + x] && detail::maskBit(mask, x, y)) t.mod[(size_t)y * n + x] ^= 1;
    t.format(mask);
    int p = detail::penalty(t.mod, n);
    if (p < bestPen) { bestPen = p; bestMask = mask; best = t.mod; }
  }
  (void)bestMask;
  out.n = n; out.m = best;
  return true;
}

} // namespace qr
