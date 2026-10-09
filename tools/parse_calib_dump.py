#!/usr/bin/env python3
"""把 DUT 固件打印的 `calib: dump ...` 行还原成 Rust 工具的输入表。

用法:
    python3 tools/parse_calib_dump.py rtt.log --ch 0 > ad.txt
    (cd ../adc_poly_calibration && ./target/debug/adc-poly calib \\
        --input ad.txt --mode code --x-col 0 --y-col 1 \\
        --code-min 0 --code-max 65535 --phys-min 0 --phys-max 10 --degree 3)

输出两列: ideal <TAB> measured_raw，行序按 ideal 升序。
"""
import argparse
import re
import sys

CHUNK = re.compile(r'^.*calib: dump (grid|ch(\d)) (\d+)((?: \d+)*)\s*$')


def parse(path):
    grid, chans = {}, {i: {} for i in range(4)}
    for line in open(path, encoding='utf-8', errors='replace'):
        m = CHUNK.search(line)
        if not m:
            continue
        off = int(m.group(3))
        vals = [int(v) for v in m.group(4).split()]
        tgt = grid if m.group(1) == 'grid' else chans[int(m.group(2))]
        for i, v in enumerate(vals):
            tgt[off + i] = v
    return grid, chans


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--ch', type=int, default=0, choices=range(4))
    a = ap.parse_args()

    grid, chans = parse(a.log)
    if not grid:
        sys.exit('未找到 `calib: dump grid` 行')
    if len(chans[a.ch]) != len(grid):
        sys.exit(f'ch{a.ch} 点数 {len(chans[a.ch])} != grid {len(grid)}')

    for i in sorted(grid):
        print(f'{grid[i]}\t{chans[a.ch][i]}')


if __name__ == '__main__':
    main()
