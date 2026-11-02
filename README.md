# PS4 Chess

Chess for PS4 homebrew (OpenOrbis) and a native Android app that plays against the console over your home Wi-Fi.
No web page, no browser and no HTTP server are involved: the phone and the PS4 talk directly over the local network.

## How the LAN connection works
* **PS4** opens a TCP port (**7788**) and a UDP responder on the same number when you choose *Play on Phone (LAN)* in the menu.
* **Android app** finds the console by itself: it sends a UDP broadcast `PS4CHESS?` (the PS4 answers `PS4CHESS|7788|PS4 Chess`) and in parallel probes the Wi-Fi network. A manual "Type address" box is the fallback.
* **Protocol**: plain TCP, one message per line. The PS4 greets every connection with `{"t":"hello","app":"PS4CHESS",...}`; the app sends `hello`, `ping` (every 4 s), `move e2e4`, `resign`, `draw`, `draw_yes`, `draw_no`, `new`, `hist`; the PS4 answers with `{"t":"state",...}` lines. The PS4 is authoritative: it validates every move with the same rules engine.
* The app reconnects by itself after a Wi-Fi drop; a phone that went to sleep is replaced by its next connection.
* The network layer never blocks the game loop (non-blocking sockets, `MSG_DONTWAIT`, 1 ms timeouts, no `select()`), and the top-right corner of the PS4 screen shows `N fps  M Hz  net X ms` so any stall is visible.

## PS4 game
* Modes: local 2 players, vs Robot (5 levels), LAN with the Android app. Clocks, draw offers, hints, evaluation bar, undo, 4 board themes, animated moves, move list.
* Full rules (check, checkmate, stalemate, castling, en passant, promotion, repetition, 50-move, insufficient material); the move generator matches published perft numbers.
* The screen is redrawn only when something visible changed; text is cached as textures (an idle game costs ~14 draw calls per loop).
* Controls: D-pad / left stick, X select, O cancel (hold O = pause menu), Square flip, Triangle undo. If a button does nothing use *Controller test* in the menu (mapping: `BTN_*` near the top of `ps4/main.cpp`).

## Android app
`android/` - Java, no libraries. Its own chess engine (perft-verified), robot, board and pieces drawn on a Canvas, sounds and vibration.
* **Play Computer** (5 levels) and **Play a Friend** (pass and play) work offline; **Play on PS4** uses the LAN described above.
* chess.com-style screens, drag & drop, legal-move dots, clocks, captured pieces, move list with review, hints, evaluation bar, draw offers, promotion chooser, game-over card, settings.
* Build: GitHub **Actions -> android-apk -> Artifacts -> PS4Chess-apk** (`app-debug.apk`). Allow "install unknown apps".

## Try the Android app without a PS4
`clang++ -O2 -std=c++11 tools/ps4sim.cpp -o ps4sim && ./ps4sim` (works in Termux or on a PC) runs the PS4 side of the protocol with a robot player. The app finds it like a real console. `./ps4sim 7788 3 b` = robot level 3, you play Black.

## Layout
`core/` rules engine, AI and the LAN layer (header-only, shared by PS4 and the simulator) - `ps4/` SDL front end, embedded font, piece rasteriser -
`android/` the app - `tools/` generators (`gen_pieces.py`, `gen_font.py`) and `ps4sim.cpp` - `tests/` all test suites (`sh tests/run_all.sh`) - `.github/workflows/` CI: tests, PKG, APK.
Piece artwork: `tools/piece_shapes.py`; `python3 tools/gen_pieces.py` regenerates it for PS4 and Android.

## Licences
Text uses **Poppins** (c) The Poppins Project Authors, SIL Open Font License 1.1 (embedded in `ps4/fontdata.hpp` and `android/app/src/main/assets/fonts/`).
