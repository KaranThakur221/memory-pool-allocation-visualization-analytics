#!/usr/bin/env python3

from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import pandas as pd

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / 'data' / 'benchmark_results'
OUTPUT_DIR = ROOT / 'screenshots' / 'experiments'


def main():
    csv_files = sorted(RESULTS.glob('*.csv'))
    if not csv_files:
        print('No CSV benchmark files to render.')
        return

    frames = [pd.read_csv(path) for path in csv_files]
    df = pd.concat(frames, ignore_index=True)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    if 'allocation_latency' in df.columns:
        plt.figure()
        plt.plot(df['experiment_id'], df['allocation_latency'], marker='o')
        plt.title('Allocation latency')
        plt.xlabel('Experiment ID')
        plt.ylabel('Latency (ns)')
        plt.tight_layout()
        plt.savefig(OUTPUT_DIR / 'allocation_latency.png', dpi=150)

    if 'memory_utilization' in df.columns:
        plt.figure()
        plt.plot(df['experiment_id'], df['memory_utilization'], marker='s')
        plt.title('Memory utilization')
        plt.xlabel('Experiment ID')
        plt.ylabel('Utilization')
        plt.tight_layout()
        plt.savefig(OUTPUT_DIR / 'memory_utilization.png', dpi=150)

    if 'external_fragmentation' in df.columns:
        plt.figure()
        plt.plot(df['experiment_id'], df['external_fragmentation'], marker='^')
        plt.title('External fragmentation')
        plt.xlabel('Experiment ID')
        plt.ylabel('Fragmentation ratio')
        plt.tight_layout()
        plt.savefig(OUTPUT_DIR / 'external_fragmentation.png', dpi=150)

    print(f'Generated charts in {OUTPUT_DIR}')


if __name__ == '__main__':
    main()
