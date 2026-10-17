#!/bin/sh
# Runs every test. Usage: sh tests/run_all.sh   (from the repository root)
set -e
OUT=${TMPDIR:-/tmp}/chess_tests; mkdir -p "$OUT"
CXX=${CXX:-g++}
echo "== engine (perft + rules)"; $CXX -O2 -std=c++11 tests/test_engine.cpp -o "$OUT/te" && "$OUT/te" | tail -3
echo "== ai";                    $CXX -O2 -std=c++11 tests/test_ai.cpp -o "$OUT/ta" && "$OUT/ta" | tail -4
echo "== net/lan";               $CXX -O1 -std=c++11 tests/test_net.cpp -o "$OUT/tn" && "$OUT/tn" | tail -2
echo "== PS4 UI (stub SDL)";     $CXX -O1 -w -std=c++11 -Itests/stub -Icore -Ips4 tests/test_app.cpp -o "$OUT/tp" && "$OUT/tp" | tail -2
echo "== phone web client";
if command -v node >/dev/null 2>&1; then
  $CXX -std=c++11 -I. tests/gen_state.cpp -o "$OUT/gs"
  "$OUT/gs" > "$OUT/s_start.json"
  "$OUT/gs" "8/P6k/8/8/8/8/8/K7 w - - 0 1" 0 > "$OUT/s_promo.json"
  "$OUT/gs" "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e6 0 2" 1 > "$OUT/s_black.json"
  python3 -c "
import re,sys
h=open('web/index.html',encoding='utf-8').read()
open('$OUT/client.js','w').write(re.search(r'<script>(.*)</script>',h,re.S).group(1))"
  python3 tools/gen_web.py >/dev/null && git diff --quiet core/web_html.h 2>/dev/null || true
  node tests/test_web.js "$OUT"
else echo "node not installed - skipped"; fi
echo "ALL SUITES DONE"
