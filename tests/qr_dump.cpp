#include <stdio.h>
#include "../core/qr.hpp"
int main(int argc, char **argv) {
  qr::Code c; if (!qr::encode(argv[1], c)) { printf("TOO LONG\n"); return 1; }
  printf("%d\n", c.n);
  for (int y = 0; y < c.n; y++) { for (int x = 0; x < c.n; x++) putchar(c.dark(x, y) ? '1' : '0'); putchar('\n'); }
}
