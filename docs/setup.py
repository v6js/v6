import re
import pathlib

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPORT = ROOT / "bench" / "report.md"
OUT_DIR = ROOT / "docs" / "blog" / "intro"

ROW_RE = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*([\d.]+)")

NODE_COLOR = "#90ee90"
V6_COLOR = "#87ceeb"

GROUPS = [
    (
        "bench-fs",
        "Filesystem I/O",
        [
            "fs-readdir-many-dirs",
            "fs-readdir-single-dir-repeated",
            "fs-read-large-file",
            "fs-read-many-small-files",
            "fs-stat-repeated",
            "fs-write-many-small-files",
        ],
    ),
    (
        "bench-object-shape",
        "Object shape and property access",
        [
            "object-props",
            "getters-setters",
            "prototype-chain",
            "object-assign-merge",
            "symbol-weakmap",
        ],
    ),
    (
        "bench-numeric",
        "Numeric and tight-loop computation",
        [
            "factorial",
            "fibonacci",
            "loop-sum",
            "matrix-multiply",
            "bitwise-ops",
        ],
    ),
    (
        "bench-collections",
        "Collections: arrays, typed arrays, maps and sets",
        [
            "array-from-iterables",
            "array-includes-indexof",
            "array-ops",
            "array-sort",
            "typed-array-ops",
            "map-set-ops",
        ],
    ),
    (
        "bench-async",
        "Async and the event loop",
        [
            "promise-all",
            "promise-chain",
            "event-emitter",
            "generators",
        ],
    ),
    (
        "bench-text",
        "Strings, regex, paths and serialization",
        [
            "string-concat",
            "string-methods",
            "regex-match",
            "buffer-encoding",
            "json-roundtrip",
            "date-ops",
            "path-ops",
        ],
    ),
    (
        "bench-syntax",
        "Language syntax and control flow",
        [
            "destructuring-spread",
            "default-rest-params",
            "optional-chaining",
            "template-literals",
            "switch-heavy",
            "try-catch-heavy",
            "for-in-enum",
        ],
    ),
]


def parse_report(path):
    text = "\n" + path.read_text(encoding="utf-8")
    fixtures = {}
    for block in text.split("\n## ")[1:]:
        lines = block.strip().splitlines()
        name = lines[0].strip()
        scale = 1000.0 if "Mean [s]" in block else 1.0
        node_mean = v6_mean = None
        for line in lines:
            m = ROW_RE.match(line)
            if not m:
                continue
            label, mean = m.group(1), float(m.group(2)) * scale
            if label.startswith("nodejs"):
                node_mean = mean
            elif label.startswith("v6 "):
                v6_mean = mean
        if node_mean is not None and v6_mean is not None:
            fixtures[name] = (node_mean, v6_mean)
    return fixtures


def plot_group(slug, title, names, fixtures, out_dir):
    rows = [(name, *fixtures[name]) for name in names if name in fixtures]
    rows.sort(key=lambda r: r[1], reverse=True)

    labels = [r[0] for r in rows]
    node_ms = [r[1] for r in rows]
    v6_ms = [r[2] for r in rows]

    fig_height = 0.55 * len(rows) + 1.3
    fig, ax = plt.subplots(figsize=(9, fig_height))
    y = list(range(len(rows)))
    bar_h = 0.36

    ax.barh(
        [i + bar_h / 2 for i in y],
        node_ms,
        height=bar_h,
        color=NODE_COLOR,
        edgecolor="#4a4a4a",
        linewidth=0.5,
        label="node",
    )
    ax.barh(
        [i - bar_h / 2 for i in y],
        v6_ms,
        height=bar_h,
        color=V6_COLOR,
        edgecolor="#4a4a4a",
        linewidth=0.5,
        label="v6 (daemon)",
    )

    ax.set_xscale("log")
    ax.set_xlabel("mean time, ms (log scale)")
    ax.set_yticks(y)
    ax.set_yticklabels(labels, fontsize=9)
    ax.set_ylim(-0.6, len(rows) - 0.4)
    ax.invert_yaxis()
    ax.legend(loc="lower right", fontsize=9)
    ax.set_title(title)
    fig.tight_layout()
    out_path = out_dir / f"{slug}.png"
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    return out_path


def main():
    fixtures = parse_report(REPORT)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    covered = set()
    for slug, title, names in GROUPS:
        covered.update(names)
        out_path = plot_group(slug, title, names, fixtures, OUT_DIR)
        print(f"wrote {out_path}")

    missing = set(fixtures) - covered
    if missing:
        print("fixtures not assigned to a group:", sorted(missing))


if __name__ == "__main__":
    main()
