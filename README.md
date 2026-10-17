# PS4 Chess

Chess for PS4 homebrew (OpenOrbis) with a phone client over your home network.

* **Local 2 players** on one controller (optional auto-flip), **vs Robot** (5 levels), or **LAN**: the PS4 hosts, your phone (Android/iPhone) plays from its browser.
* Full rules: check, checkmate, stalemate, castling, en passant, promotion (choose piece), threefold repetition, 50-move rule, insufficient material, clocks with increment.
* Legal-move hints, last-move highlight, check glow, animated moves, captured pieces, move list (SAN), undo.

## Phone (Android) over LAN
1. Start the game, choose **Game mode: LAN (phone)**, press START.
2. The PS4 shows `http://<ip>:8080`. Open it in Chrome on a phone on the same Wi-Fi.
3. No app to install and no build step - the page is served by the PS4 itself. "Add to Home screen" gives an app-like icon.

## Controls
D-pad / left stick move, X select, O cancel (hold O = pause menu), Square flip, Triangle undo.
If a button does nothing, open **Controller test** in the menu and tell which numbers light up; the mapping is the `BTN_*` arrays at the top of `ps4/main.cpp`.

## Layout
`core/` rules engine, AI, HTTP+WebSocket server (header-only, tested) - `web/index.html` phone client -
`ps4/` SDL front end - `tests/` test suites (`sh tests/run_all.sh`) - `.github/workflows/` CI: tests + PKG build.
After editing `web/index.html` run `python3 tools/gen_web.py` (CI does it automatically).
