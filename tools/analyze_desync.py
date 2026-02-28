#!/usr/bin/env python3
"""Compare sanitized PLW dumps between peers to find exact diverging bytes."""

from pathlib import Path


def main():
    frame = 1428
    states = Path("states")

    for plw_idx in range(2):
        f0 = states / f"0_{frame}_plw{plw_idx}_san"
        f1 = states / f"1_{frame}_plw{plw_idx}_san"

        if not f0.exists() or not f1.exists():
            print(f"Missing files for PLW[{plw_idx}]")
            continue

        d0 = f0.read_bytes()
        d1 = f1.read_bytes()

        print(f"=== PLW[{plw_idx}] sanitized comparison (peer 0 vs peer 1) ===")
        print(f"    Size: {len(d0)} bytes")

        diffs = [i for i in range(min(len(d0), len(d1))) if d0[i] != d1[i]]

        if not diffs:
            print("    IDENTICAL!")
            print()
            continue

        # Group into ranges
        ranges = []
        start = diffs[0]
        end = diffs[0]
        for d in diffs[1:]:
            if d == end + 1:
                end = d
            else:
                ranges.append((start, end))
                start = end = d
        ranges.append((start, end))

        print(f"    Differing bytes: {len(diffs)}")
        print(f"    Diff ranges: {len(ranges)}")
        print()

        for s, e in ranges:
            sz = e - s + 1
            v0 = " ".join(f"{b:02x}" for b in d0[s : e + 1])
            v1 = " ".join(f"{b:02x}" for b in d1[s : e + 1])
            if sz > 32:
                v0 = " ".join(f"{b:02x}" for b in d0[s : s + 32]) + "..."
                v1 = " ".join(f"{b:02x}" for b in d1[s : s + 32]) + "..."
            print(f"    +0x{s:04X} ({sz:3d}B): peer0={v0}")
            print(f"    {' ' * (6 + 4 + 7)}peer1={v1}")
            print()

        # Also print surrounding context for each diff
        print("  Context around diffs:")
        for s, e in ranges[:10]:
            ctx_start = max(0, s - 8)
            ctx_end = min(len(d0), e + 9)
            ctx0 = " ".join(f"{b:02x}" for b in d0[ctx_start:ctx_end])
            ctx1 = " ".join(f"{b:02x}" for b in d1[ctx_start:ctx_end])
            print(f"    +0x{ctx_start:04X}: {ctx0}")
            print(f"    +0x{ctx_start:04X}: {ctx1}")
            print()


if __name__ == "__main__":
    main()
