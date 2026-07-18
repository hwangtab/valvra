#!/bin/zsh
# Valvra bench runner — docs/35 §B4 protocol.
#
# A single valvra_bench pass is NOT a measurement: a background indexer
# (mediaanalysisd at 90% CPU turned a 68% HiFi row into 492%) can poison
# every row.  This runner (1) refuses to start on a busy machine,
# (2) runs the suite N times, (3) reports the per-row BEST throughput —
# the run least contaminated by other processes — which is the number
# comparable against docs/27's baselines.
#
# Usage:
#   scripts/run_bench.sh                 # 3 runs, best-of, markdown to stdout
#   BENCH_RUNS=5 scripts/run_bench.sh    # more runs
#   scripts/run_bench.sh --force         # skip the idle check (CI, known-busy)
#
# Append the output to a dated docs/NN-bench-YYYY-MM-DD.md when registering
# a new baseline (docs/27 §회귀 기준선).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/bench/valvra_bench"
RUNS="${BENCH_RUNS:-3}"
FORCE=0
[[ "${1:-}" == "--force" ]] && FORCE=1

if [[ ! -x "$BIN" ]]; then
    echo "error: $BIN not found — build with -DVALVRA_BUILD_BENCHES=ON" >&2
    exit 1
fi

# ── Idle check ───────────────────────────────────────────────────────────
# Any foreign process burning >40% of a core skews the wall-clock rows.
if [[ $FORCE -eq 0 ]]; then
    busy=$(ps -Ao %cpu=,comm= -r | awk '$1 > 40 {print; exit}')
    if [[ -n "$busy" ]]; then
        echo "error: machine not idle — busiest process: $busy" >&2
        echo "       wait for it to finish, or rerun with --force." >&2
        exit 2
    fi
fi

# ── N runs, per-row best-of ──────────────────────────────────────────────
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

for i in $(seq 1 "$RUNS"); do
    echo "run $i/$RUNS..." >&2
    "$BIN" > "$tmpdir/run$i.md"
done

# Rows look like: | Name · OS · ch · path | 1.46x | 68.51% | 3.586 | 3.977 |
# Key on column 1; keep the row with the highest realtime multiple (col 2).
awk -F'|' '
    /^\|/ && $3 ~ /x[[:space:]]*$/ {
        name = $2; gsub(/^[ \t]+|[ \t]+$/, "", name)
        rt = $3 + 0.0
        if (!(name in best) || rt > bestRt[name]) {
            best[name] = $0; bestRt[name] = rt
            if (!(name in seen)) { order[++n] = name; seen[name] = 1 }
        }
        next
    }
    NR == FNR && /^\|/ { header = header $0 "\n" }   # header/divider from run 1
    END {
        printf "%s", header
        for (i = 1; i <= n; i++) print best[order[i]]
    }
' "$tmpdir"/run*.md > "$tmpdir/best.md"

echo "<!-- bench: best-of-$RUNS runs, $(sysctl -n machdep.cpu.brand_string)," \
     "$(sw_vers -productVersion), $(date +%Y-%m-%d) -->"
cat "$tmpdir/best.md"
echo "best-of-$RUNS table written to stdout." >&2
