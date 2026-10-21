#!/usr/bin/env python3
"""Real-browser test: loads the phone page from the real C++ server in headless Chromium and plays moves
(tap-tap and drag & drop). Skips when Playwright/Chromium is not available. Usage: test_browser.py <chessweb binary>"""
import subprocess, sys, time
try:
    from playwright.sync_api import sync_playwright
except Exception:
    print("playwright not installed - browser test skipped"); sys.exit(0)

exe = sys.argv[1]
srv = subprocess.Popen([exe, "18082", "0"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(0.8)
fails = []
def check(c, m):
    if not c: fails.append(m); print("FAIL:", m)
try:
    with sync_playwright() as p:
        try: b = p.chromium.launch()
        except Exception as e: print("chromium unavailable - skipped:", str(e)[:80]); sys.exit(0)
        logs = []
        pg = b.new_context(viewport={"width": 390, "height": 800}).new_page()
        pg.on("console", lambda m: logs.append(m.text) if m.type == "error" else None)
        pg.on("pageerror", lambda e: logs.append(str(e)))
        pg.goto("http://127.0.0.1:18082/test"); check("reachable" in pg.inner_text("body"), "/test page reachable")
        pg.goto("http://127.0.0.1:18082/"); pg.wait_for_selector(".pc", timeout=5000)
        check(pg.locator(".sq").count() == 64 and pg.locator(".pc").count() == 32, "64 squares / 32 pieces")
        check("Your move" in pg.inner_text("#status"), "status says Your move")
        box = pg.locator("#board").bounding_box(); check(box["height"] > 200, "board has a real height (%s)" % box["height"])
        def pt(sq, flipped=False):
            f = "abcdefgh".index(sq[0]); r = int(sq[1]) - 1
            return (box["x"] + ((7 - f if flipped else f) + .5) * box["width"] / 8, box["y"] + ((r if flipped else 7 - r) + .5) * box["height"] / 8)
        def click(sq): x, y = pt(sq); pg.mouse.click(x, y)
        def moves(): return pg.inner_text("#moves")
        # tap-tap: e2 then e4
        click("e2"); pg.wait_for_timeout(150)
        check(pg.locator(".dot").count() == 2, "two legal-move dots for the e2 pawn")
        click("e4"); pg.wait_for_function("document.querySelector('#moves').innerText.indexOf('e4')>=0", timeout=4000)
        # wait for the robot's reply (two plies)
        pg.wait_for_function("document.querySelectorAll('#moves b').length>=2", timeout=8000)
        check(True, "robot answered")
        # drag and drop d2 -> d4
        x1, y1 = pt("d2"); x2, y2 = pt("d4")
        pg.mouse.move(x1, y1); pg.mouse.down(); pg.mouse.move((x1 + x2) / 2, (y1 + y2) / 2, steps=4); pg.mouse.move(x2, y2, steps=4); pg.mouse.up()
        pg.wait_for_function("document.querySelector('#moves').innerText.indexOf('d4')>=0", timeout=4000)
        # illegal move is refused (a pawn cannot jump 3 squares)
        pg.wait_for_function("document.querySelectorAll('#moves b').length>=4", timeout=8000)
        n0 = pg.locator("#moves b").count(); x1, y1 = pt("a2"); x2, y2 = pt("a5")
        pg.mouse.move(x1, y1); pg.mouse.down(); pg.mouse.move(x2, y2, steps=5); pg.mouse.up(); pg.wait_for_timeout(500)
        check(pg.locator("#moves b").count() == n0, "illegal drag is not played")
        # review: go back one move
        pg.click("#nPrev"); pg.wait_for_function("document.querySelector('#status').innerText.indexOf('Reviewing')>=0", timeout=3000)
        pg.click("#nLast"); pg.wait_for_function("document.querySelector('#status').innerText.indexOf('Reviewing')<0", timeout=3000)
        check(not logs, "no console/page errors: %s" % logs)
        b.close()
finally:
    srv.terminate()
print("BROWSER TEST " + ("FAILED: %s" % fails if fails else "PASSED"))
sys.exit(1 if fails else 0)
