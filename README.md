# PS4 Chess

Chess for PS4 homebrew (OpenOrbis) with a chess.com-style look, a robot opponent, and a phone client over your home network.

## Features
* **Modes:** local 2 players (optional auto-flip), vs Robot (5 levels), LAN - the PS4 hosts and a phone plays from its browser.
* **Rules:** check, checkmate, stalemate, castling, en passant, promotion (piece chooser), threefold repetition, 50-move rule, insufficient material, clocks with increment, timeout. The move generator matches the published perft numbers for six standard test positions.
* **Look:** anti-aliased Poppins text, shaded vector pieces, 4 board themes, animated moves, last-move highlight, check glow, legal-move dots, captured pieces and material count, move list, evaluation bar, hint arrow.
* **Dialogs:** pause menu, resign confirmation, draw offers (robot decides by evaluation; phone and local players accept/decline), rematch with colours swapped.
* **Phone page:** SVG pieces, drag & drop or tap-tap, evaluation bar, sounds + vibration, move review (first/prev/next/last), draw offers, game-over card, auto-reconnect with heartbeat, installable ("Add to Home screen").
* **Joining:** the PS4 shows a **QR code** and the address; scan it with the phone camera.

## Phone over LAN
1. Menu -> **Game mode: Play on Phone (LAN)**. The server starts immediately.
2. Scan the QR code (or open `http://<ps4-ip>:8080`) on a phone on the same Wi-Fi. No app, no build.
3. Not sure the network works? Open `http://<ps4-ip>:8080/test` first - a plain page that says "PS4 Chess server is reachable".
4. Press **Play**. The phone gets the other colour. A phone that went to sleep is replaced automatically when it reconnects.

Run the web page on a computer/Termux without a PS4: `clang++ -O2 -std=c++11 tools/server.cpp -o chessweb && ./chessweb 8080 2`.

## Android app (native APK)
`android/` is a real Android app, not a web page: its own chess engine (verified with perft), its own robot, its own board/pieces/screens drawn on a Canvas, sounds and vibration.
* **Play Computer** (5 levels), **Play a Friend** (pass and play) - fully offline.
* **Play on PS4**: finds the console by itself (UDP broadcast + network scan), connects with a built-in WebSocket client and plays the game running on the PS4. Auto-reconnect, review, hints, evaluation bar, draw offers and rematch work there too.
* chess.com-style home, setup (level, colour, time control), game screen (clocks, captured pieces, move list, drag & drop, legal-move dots, last move, check glow, animations), promotion chooser, game-over card, 4 board themes, settings.
Build: GitHub **Actions -> android-apk -> Artifacts -> PS4Chess-apk** (`app-debug.apk`). Install it (allow "install unknown apps") and open it.
Piece artwork lives in `tools/piece_shapes.py`; `python3 tools/gen_pieces.py` regenerates it for PS4, web and Android.

## Controls
D-pad / left stick move, X select, O cancel (hold O = pause menu), Square flip, Triangle undo.
If a button does nothing, open **Controller test** in the menu; the mapping is the `BTN_*` arrays near the top of `ps4/main.cpp`.

## Layout
`core/` rules engine, AI, QR encoder, HTTP+WebSocket server (header-only, tested) - `web/index.html` phone client -
`ps4/` SDL front end, embedded font and piece rasteriser - `tests/` test suites (`sh tests/run_all.sh`) - `.github/workflows/` CI: tests + PKG build.
After editing `web/index.html` run `python3 tools/gen_web.py` (CI does it automatically).

## Credits / licences
Text uses **Poppins** (c) The Poppins Project Authors, SIL Open Font License 1.1 - embedded as rasterised glyphs in `ps4/fontdata.hpp` (`tools/gen_font.py`).
