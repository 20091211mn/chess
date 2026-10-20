#!/usr/bin/env python3
"""Encodes URLs with core/qr.hpp and checks that a real QR decoder (OpenCV) reads them back. Usage: test_qr.py <qr_dump binary>"""
import subprocess, sys, random
try:
    import cv2, numpy as np
except Exception:
    print("opencv not installed - QR decode test skipped"); sys.exit(0)
exe = sys.argv[1]
random.seed(7)
tests = ["http://172.26.1.57:8080", "http://192.168.0.5:8080", "http://255.255.255.255:8080", "http://a.b", "z" * 53]
tests += ["http://%d.%d.%d.%d:%d" % (random.randint(1,255), random.randint(0,255), random.randint(0,255), random.randint(1,255), random.choice([80,8080,18080])) for _ in range(30)]
bad = 0
for t in tests:
    out = subprocess.run([exe, t], capture_output=True, text=True).stdout.split("\n")
    n = int(out[0]); rows = out[1:1+n]
    img = np.ones(((n+8)*10, (n+8)*10), np.uint8) * 255
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch == "1": img[(y+4)*10:(y+5)*10, (x+4)*10:(x+5)*10] = 0
    val, _, _ = cv2.QRCodeDetector().detectAndDecode(img)
    if val != t and hasattr(cv2, 'QRCodeDetectorAruco'):   # the older detector misses some valid codes
        val, _, _ = cv2.QRCodeDetectorAruco().detectAndDecode(img)
    if val != t: bad += 1; print("MISMATCH", t, "->", repr(val))
print("QR decode: %d/%d OK" % (len(tests) - bad, len(tests)))
sys.exit(1 if bad else 0)
