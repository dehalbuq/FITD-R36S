#!/usr/bin/env python3
"""Check the 25Hz timebase accumulator for multiple rendering cadences."""
for fps in (25, 30, 40, 50, 60, 75, 120):
    prev = 0
    carry = 0
    ticks = 0
    for i in range(1, fps + 1):
        now = i * 1000 // fps
        elapsed = max(0, now - prev)
        prev = now
        carry += elapsed * 25
        ticks += carry // 1000
        carry %= 1000
    assert ticks == 25, (fps, ticks)
    print(f"render={fps} FPS -> clock={ticks} ticks/s (expected 25)")
