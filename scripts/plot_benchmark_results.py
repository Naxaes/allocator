#!/usr/bin/env python3
import json
import math
import sys
from pathlib import Path

SVG_WIDTH = 960
SVG_HEIGHT = 540
MARGIN_LEFT = 90
MARGIN_RIGHT = 40
MARGIN_TOP = 50
MARGIN_BOTTOM = 70
COLORS = [
    "#1f77b4",
    "#ff7f0e",
    "#2ca02c",
    "#d62728",
    "#9467bd",
    "#8c564b",
]


def usage() -> int:
    print("Usage: plot_benchmark_results.py <benchmark.json> <output.svg> [metric]", file=sys.stderr)
    print("metric: avg_ns_per_op (default) | avg_ns | median_ns | min_ns | max_ns", file=sys.stderr)
    return 1


def load_results(path: Path):
    payload = json.loads(path.read_text())
    return payload["results"]


def group_by_allocator(results, metric: str):
    grouped = {}
    for item in results:
        grouped.setdefault(item["allocator"], []).append((item["payload_size"], float(item[metric]), item))
    for allocator in grouped:
        grouped[allocator].sort(key=lambda entry: entry[0])
    return grouped


def scale_x(value: float, min_x: float, max_x: float) -> float:
    plot_width = SVG_WIDTH - MARGIN_LEFT - MARGIN_RIGHT
    if max_x == min_x:
        return MARGIN_LEFT + plot_width / 2.0
    return MARGIN_LEFT + ((value - min_x) / (max_x - min_x)) * plot_width


def scale_y(value: float, min_y: float, max_y: float) -> float:
    plot_height = SVG_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM
    if max_y == min_y:
        return MARGIN_TOP + plot_height / 2.0
    return MARGIN_TOP + plot_height - ((value - min_y) / (max_y - min_y)) * plot_height


def build_svg(grouped, metric: str) -> str:
    all_sizes = [size for series in grouped.values() for size, _, _ in series]
    all_values = [value for series in grouped.values() for _, value, _ in series]

    min_x = min(all_sizes)
    max_x = max(all_sizes)
    min_y = 0.0
    max_y = max(all_values)
    if max_y <= min_y:
        max_y = min_y + 1.0

    y_ticks = 5
    x_ticks = sorted(set(all_sizes))
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{SVG_WIDTH}" height="{SVG_HEIGHT}" viewBox="0 0 {SVG_WIDTH} {SVG_HEIGHT}">',
        '<style>text{font-family:Menlo,Consolas,monospace;font-size:12px;fill:#222} .title{font-size:18px;font-weight:bold} .axis{stroke:#333;stroke-width:1.2} .grid{stroke:#ddd;stroke-width:1;stroke-dasharray:4 4}</style>',
        f'<text x="{SVG_WIDTH / 2}" y="28" text-anchor="middle" class="title">Allocator benchmark ({metric})</text>',
        f'<line class="axis" x1="{MARGIN_LEFT}" y1="{SVG_HEIGHT - MARGIN_BOTTOM}" x2="{SVG_WIDTH - MARGIN_RIGHT}" y2="{SVG_HEIGHT - MARGIN_BOTTOM}" />',
        f'<line class="axis" x1="{MARGIN_LEFT}" y1="{MARGIN_TOP}" x2="{MARGIN_LEFT}" y2="{SVG_HEIGHT - MARGIN_BOTTOM}" />',
    ]

    for tick in range(y_ticks + 1):
        value = min_y + ((max_y - min_y) * tick / y_ticks)
        y = scale_y(value, min_y, max_y)
        parts.append(f'<line class="grid" x1="{MARGIN_LEFT}" y1="{y:.2f}" x2="{SVG_WIDTH - MARGIN_RIGHT}" y2="{y:.2f}" />')
        parts.append(f'<text x="{MARGIN_LEFT - 10}" y="{y + 4:.2f}" text-anchor="end">{value:.2f}</text>')

    for size in x_ticks:
        x = scale_x(size, min_x, max_x)
        parts.append(f'<line class="grid" x1="{x:.2f}" y1="{MARGIN_TOP}" x2="{x:.2f}" y2="{SVG_HEIGHT - MARGIN_BOTTOM}" />')
        parts.append(f'<text x="{x:.2f}" y="{SVG_HEIGHT - MARGIN_BOTTOM + 24}" text-anchor="middle">{size}</text>')

    parts.append(f'<text x="{SVG_WIDTH / 2}" y="{SVG_HEIGHT - 20}" text-anchor="middle">payload size (bytes)</text>')
    parts.append(f'<text x="24" y="{SVG_HEIGHT / 2}" transform="rotate(-90 24 {SVG_HEIGHT / 2})" text-anchor="middle">{metric}</text>')

    legend_x = SVG_WIDTH - MARGIN_RIGHT - 220
    legend_y = MARGIN_TOP + 8

    for index, (allocator, series) in enumerate(sorted(grouped.items())):
        color = COLORS[index % len(COLORS)]
        points = " ".join(
            f"{scale_x(size, min_x, max_x):.2f},{scale_y(value, min_y, max_y):.2f}"
            for size, value, _ in series
        )
        parts.append(f'<polyline fill="none" stroke="{color}" stroke-width="2.5" points="{points}" />')
        for size, value, item in series:
            x = scale_x(size, min_x, max_x)
            y = scale_y(value, min_y, max_y)
            title = (
                f"{allocator} size={size} avg_ns_per_op={item.get('avg_ns_per_op', 0):.2f} "
                f"min={item.get('min_ns', 0)} med={item.get('median_ns', 0)} max={item.get('max_ns', 0)}"
            )
            parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="4" fill="{color}"><title>{title}</title></circle>')

        ly = legend_y + index * 22
        parts.append(f'<line x1="{legend_x}" y1="{ly}" x2="{legend_x + 24}" y2="{ly}" stroke="{color}" stroke-width="3" />')
        parts.append(f'<text x="{legend_x + 32}" y="{ly + 4}" text-anchor="start">{allocator}</text>')

    parts.append('</svg>')
    return "\n".join(parts)


def main(argv: list[str]) -> int:
    if len(argv) not in (3, 4):
        return usage()

    input_path = Path(argv[1])
    output_path = Path(argv[2])
    metric = argv[3] if len(argv) == 4 else "avg_ns_per_op"

    if metric not in {"avg_ns_per_op", "avg_ns", "median_ns", "min_ns", "max_ns"}:
        return usage()

    results = load_results(input_path)
    grouped = group_by_allocator(results, metric)
    svg = build_svg(grouped, metric)
    output_path.write_text(svg)
    print(f"wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

