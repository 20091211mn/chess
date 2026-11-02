#!/bin/sh
# Runs every test. Usage: sh tests/run_all.sh   (from the repository root)
set -e
OUT=${TMPDIR:-/tmp}/chess_tests; mkdir -p "$OUT"
CXX=${CXX:-g++}
echo "== engine (perft + rules)"; $CXX -O2 -std=c++11 tests/test_engine.cpp -o "$OUT/te" && "$OUT/te" | tail -3
echo "== ai";                    $CXX -O2 -std=c++11 tests/test_ai.cpp -o "$OUT/ta" && "$OUT/ta" | tail -4
echo "== PS4 LAN layer (plain TCP lines + UDP discovery)"; $CXX -O1 -std=c++11 tests/test_net.cpp -o "$OUT/tn" && "$OUT/tn" | tail -3
echo "== PS4 UI (stub SDL)";     $CXX -O1 -w -std=c++11 -Itests/stub -Icore -Ips4 tests/test_app.cpp -o "$OUT/tp" && "$OUT/tp" | tail -2
echo "== PS4 UI draw-call budget"; $CXX -O2 -w -std=c++11 -Itests/stub -Icore -Ips4 tests/perf_app.cpp -o "$OUT/pf" && "$OUT/pf"
echo "== Android app (JVM): type-check against API stubs + discovery"
if command -v java >/dev/null 2>&1 && java --list-modules 2>/dev/null | grep -q jdk.compiler; then
  $CXX -O2 -std=c++11 tools/ps4sim.cpp -o "$OUT/ps4sim"
  java android/tests/RunTests.java "$OUT/ps4sim" android | tail -3
  echo "== Android app core: perft, AI, TCP client, LAN session vs the PS4 simulator"
  java android/tests/CoreTests.java "$OUT/ps4sim" android | tail -4
  echo "== Android app UI: every screen rendered with Java2D + simulated touches"
  rm -rf "$OUT/uiclasses" && java android/tests/Build.java android "$OUT/uiclasses" && java -cp "$OUT/uiclasses" AppTests "$OUT/ps4sim" "$OUT/appshots" | tail -4
else echo "JDK not installed - Android suites skipped"; fi
echo "ALL SUITES DONE"
