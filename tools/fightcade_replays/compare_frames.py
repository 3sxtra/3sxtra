import csv

cps = list(csv.DictReader(open("tools/fightcade_replays/ChunLi_vs_Ken_002_full.csv")))
sx = list(csv.DictReader(open("tools/fightcade_replays/ChunLi_vs_Ken_002_3sx.csv")))

print("=== CPS3 around is_in_match transition (frames 645-660) ===")
for i in range(630, 670):
    r = cps[i]
    print(
        f"  f={r['frame']} match={r.get('is_in_match', '?')} p1x={r['p1_x']} p2x={r['p2_x']} p1hp={r['p1_hp']} p1b={r.get('p1_busy', '?')} p1_in={r['p1_input']} p2_in={r['p2_input']}"
    )

print()
print("=== 3SX first 20 frames ===")
for i in range(min(30, len(sx))):
    r = sx[i]
    print(
        f"  f={r['frame']} p1x={r['p1_x']} p2x={r['p2_x']} p1hp={r['p1_hp']} p1b={r.get('p1_busy', '?')} p1_in={r['p1_input']} p2_in={r['p2_input']}"
    )

print()
print("=== 3SX around frame 45-65 (where is_in_match should start) ===")
for i in range(45, min(65, len(sx))):
    r = sx[i]
    print(
        f"  f={r['frame']} p1x={r['p1_x']} p2x={r['p2_x']} p1hp={r['p1_hp']} p2hp={r['p2_hp']} p1b={r.get('p1_busy', '?')}"
    )

print()
print("=== 3SX around frame 80-100 (where first HP divergence appears) ===")
for i in range(80, min(100, len(sx))):
    r = sx[i]
    print(
        f"  f={r['frame']} p1x={r['p1_x']} p2x={r['p2_x']} p1hp={r['p1_hp']} p2hp={r['p2_hp']} p1b={r.get('p1_busy', '?')}"
    )
