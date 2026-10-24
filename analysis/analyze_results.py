#!/usr/bin/env python3

import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / 'data' / 'benchmark_results'


def read_csv(path: Path):
    with path.open(newline='') as handle:
        return list(csv.DictReader(handle))


def main():
    rows = []
    for csv_file in sorted(RESULTS.glob('*.csv')):
        rows.extend(read_csv(csv_file))

    if not rows:
        print('No benchmark result files found.')
        return

    for row in rows:
        print(row)


if __name__ == '__main__':
    main()
