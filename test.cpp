#include <cassert>
#include <cstdio>
#include "chess.h"
int main(){
  Chess g;
  assert(g.moveStr("e2e4"));
  assert(!g.moveStr("e2e4"));   // not black's piece / empty
  assert(g.moveStr("e7e5"));
  assert(!g.moveStr("d1d5"));   // blocked? d-file path clear but own pawn d2
  assert(g.moveStr("g1f3"));
  assert(g.moveStr("b8c6"));
  assert(g.moveStr("f1c4"));
  assert(!g.moveStr("c6c4"));   // knight can't go straight
  puts("All tests passed");
}
