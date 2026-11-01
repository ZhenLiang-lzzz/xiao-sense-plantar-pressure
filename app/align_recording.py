#!/usr/bin/env python3
"""Put both feet of a recording on one timeline.

Each board has its own clock, so board_ms cannot be compared between feet, and
the PC arrival time t carries BLE latency. A frame can only arrive late, never
early, so the frames with the smallest t - board_ms are the closest to the true
time. For each foot a line through those frames maps board_ms to the PC clock.

    python align_recording.py readout/recording_xxx.csv

Writes recording_xxx_aligned.csv with an added t_sync column, sorted by t_sync.
"""
import csv, os, sys

WINDOW_S = 2.0


def fit_clock(points):
    """Return slope, intercept mapping board seconds to PC seconds."""
    start = points[0][0]
    best = {}
    for board, pc in points:
        window = int((board-start)//WINDOW_S)
        if window not in best or pc-board < best[window][1]-best[window][0]:
            best[window] = (board, pc)
    fastest = list(best.values())
    if len(fastest) < 2:
        return 1.0, fastest[0][1]-fastest[0][0]
    n = len(fastest)
    mean_b = sum(b for b,_ in fastest)/n
    mean_p = sum(p for _,p in fastest)/n
    slope = sum((b-mean_b)*(p-mean_p) for b,p in fastest)/sum((b-mean_b)**2 for b,_ in fastest)
    return slope, mean_p-slope*mean_b


def align(path):
    with open(path, newline='') as f:
        rows = list(csv.DictReader(f))
    if not rows or not all(r.get('board_ms') for r in rows):
        raise ValueError("missing board_ms values: both boards need the batched BLE firmware")
    for foot in sorted({r['foot'] for r in rows}):
        own = [r for r in rows if r['foot']==foot]
        points = [(int(r['board_ms'])/1000, float(r['t'])) for r in own]
        slope, intercept = fit_clock(points)
        for r,(board,_) in zip(own, points):
            r['t_sync'] = f"{slope*board+intercept:.3f}"
        seqs = [int(r['seq']) for r in own]
        lost = sum((b-a-1) % 65536 for a,b in zip(seqs, seqs[1:]))
        print(f"{foot}: {len(own)} frames, {lost} lost, clock drift {(slope-1)*1e6:+.0f} ppm")
    rows.sort(key=lambda r: float(r['t_sync']))
    out = os.path.splitext(path)[0] + '_aligned.csv'
    with open(out, 'w', newline='') as f:
        writer = csv.DictWriter(f, ['t_sync'] + [k for k in rows[0] if k!='t_sync'])
        writer.writeheader()
        writer.writerows(rows)
    return out


if __name__ == "__main__":
    print(align(sys.argv[1]))
