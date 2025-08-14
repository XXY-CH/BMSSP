#!/usr/bin/env bash
# 说明：本脚本用于批量构建与运行 BMSSP 基准测试，生成可供绘图的 CSV。
set -uo pipefail

# Multi-scale benchmark runner for BMSSP project
# - 支持变体：lemma33, baseline, o0_lemma33, o0, debug_lemma33, debug
# - 从程序输出解析指标，写入 CSV

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."  # 切换到项目根，便于调用 make 与二进制

usage() {
  cat <<EOF
Usage: bash scripts/benchmark.sh [options]

Options:
  -n "SCALES"   Space-separated n list (default: "500 1000 2000 5000")
  -d OUTDEG     Out-degree per node (default: 5)
  -s "SEEDS"    Space-separated seeds (default: "42 43 44")
  -r REPEATS    Repeats per (n,seed,variant), incrementing seed (default: 1)
  -v "VARIANTS" Space-separated variants (default: "lemma33 baseline")
                 Allowed: lemma33 baseline o0_lemma33 o0 debug_lemma33 debug
  -m MODEL      Preset model: small | medium | large | all (overrides -n/-d). Default: none
  -o FILE       Output CSV file (default: bench_results_YYYYmmdd_HHMMSS.csv)
  -q            Quiet (less console output)
  -h            Show this help

Examples:
  bash scripts/benchmark.sh -n "1000 3000 10000" -d 5 -s "42 43" -v "lemma33 baseline" -r 2
  bash scripts/benchmark.sh -n "2000" -d 3 -v "o0_lemma33 lemma33" -o results.csv
EOF
}

NS=(500 1000 2000 5000)
OUTDEG=5
SEEDS=(42 43 44)
REPEATS=1
VARIANTS=(lemma33 baseline)
OUT_FILE="bench_results_$(date +%Y%m%d_%H%M%S).csv"
QUIET=0
MODEL_PRESET=""

while getopts ":n:d:s:r:v:m:o:qh" opt; do
  case $opt in
    n) read -r -a NS <<<"${OPTARG}" ;;
    d) OUTDEG="${OPTARG}" ;;
    s) read -r -a SEEDS <<<"${OPTARG}" ;;
    r) REPEATS="${OPTARG}" ;;
    v) read -r -a VARIANTS <<<"${OPTARG}" ;;
    m) MODEL_PRESET="${OPTARG}" ;;
    o) OUT_FILE="${OPTARG}" ;;
    q) QUIET=1 ;;
    h) usage; exit 0 ;;
    :) echo "Option -$OPTARG requires an argument" >&2; usage; exit 2 ;;
    \?) echo "Invalid option: -$OPTARG" >&2; usage; exit 2 ;;
  esac
done

log() { if [[ $QUIET -eq 0 ]]; then echo "$@"; fi; }

variant_make_target() {
  case "$1" in
    lemma33) echo "compare_bmssp" ;;
    baseline) echo "baseline" ;;
    o0_lemma33) echo "o0_lemma33" ;;
    o0) echo "o0" ;;
    debug_lemma33) echo "debug_lemma33" ;;
    debug) echo "debug" ;;
    *) echo "" ;;
  esac
}

variant_binary() {
  case "$1" in
  lemma33) echo "./bin/compare_bmssp" ;;
  baseline) echo "./bin/compare_bmssp_baseline" ;;
  o0_lemma33) echo "./bin/compare_bmssp_o0_lemma33" ;;
  o0) echo "./bin/compare_bmssp_o0" ;;
  debug_lemma33) echo "./bin/compare_bmssp_debug_lemma33" ;;
  debug) echo "./bin/compare_bmssp_debug" ;;
    *) echo "" ;;
  esac
}

for v in "${VARIANTS[@]}"; do
  tgt="$(variant_make_target "$v")"
  [[ -z "$tgt" ]] && { echo "Unknown variant: $v" >&2; exit 3; }
  log "[build] make $tgt"
  make "$tgt" >/dev/null || { echo "make $tgt failed" >&2; exit 3; }
done

echo "model,variant,n,outdeg,seed,sigma,tau,bmssp_time_s,dijkstra_time_s,ratio,checked,mismatches,missing,pulls,batches,inserts" > "$OUT_FILE"

run_matrix() {
  local MODEL_LABEL="$1"
  for v in "${VARIANTS[@]}"; do
    bin="$(variant_binary "$v")"
    [[ -x "$bin" ]] || { echo "Binary not found or not executable: $bin" >&2; exit 4; }
    for s in "${SEEDS[@]}"; do
      for n in "${NS[@]}"; do
        rep=0
        base_seed="$s"
        while [[ $rep -lt $REPEATS ]]; do
          cur_seed=$(( base_seed + rep ))
          log "[run] model=$MODEL_LABEL v=$v n=$n d=$OUTDEG seed=$cur_seed"
          out="$(BMSSP_STRICT=${BMSSP_STRICT:-0} $bin "$n" "$OUTDEG" "$cur_seed" 2>/dev/null || true)"
          first_line="$(sed -n '1p' <<<"$out")"
          second_line="$(sed -n '2p' <<<"$out")"
          verify_line="$(grep -E '^verify:' <<<"$out" | head -n1 || true)"
          stats_line="$(grep -E '^stats:' <<<"$out" | head -n1 || true)"

          n_parsed=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="n") print $(i+1)}' <<<"$first_line")
          d_parsed=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="outdeg") print $(i+1)}' <<<"$first_line")
          sigma=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="Sigma") print $(i+1)}' <<<"$first_line")
          tau=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="Tau") print $(i+1)}' <<<"$first_line")
          t_bmssp=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="BMSSP_time(s)") print $(i+1)}' <<<"$second_line")
          t_dij=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="Dijkstra_time(s)") print $(i+1)}' <<<"$second_line")
          ratio=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i ~ /time_ratio\(BMSSP\/Dij\)/) print $(i+1)}' <<<"$second_line")
          checked=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="checked") print $(i+1)}' <<<"$verify_line")
          mismatches=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="mismatches") print $(i+1)}' <<<"$verify_line")
          missing=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="missing") print $(i+1)}' <<<"$verify_line")

          pulls=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="pulls") print $(i+1)}' <<<"$stats_line")
          batches=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="batches") print $(i+1)}' <<<"$stats_line")
          inserts=$(awk -F'[=, ]+' '{for(i=1;i<=NF;i++) if($i=="inserts") print $(i+1)}' <<<"$stats_line")

          n_parsed=${n_parsed:-$n}
          d_parsed=${d_parsed:-$OUTDEG}
          sigma=${sigma:-NA}
          tau=${tau:-NA}
          t_bmssp=${t_bmssp:-NA}
          t_dij=${t_dij:-NA}
          ratio=${ratio:-NA}
          checked=${checked:-0}
          mismatches=${mismatches:-0}
          missing=${missing:-0}
          pulls=${pulls:-NA}
          batches=${batches:-NA}
          inserts=${inserts:-NA}

          echo "$MODEL_LABEL,$v,$n_parsed,$d_parsed,$cur_seed,$sigma,$tau,$t_bmssp,$t_dij,$ratio,$checked,$mismatches,$missing,$pulls,$batches,$inserts" >> "$OUT_FILE"

          status_line="$(grep -E '^status:' <<<"$out" | head -n1 || true)"
          status_word=$(awk '{print $2}' <<<"$status_line")
          if [[ "${BMSSP_STRICT:-0}" == "1" && "$status_word" != "OK" ]]; then
            echo "[error] correctness failed at model=$MODEL_LABEL v=$v n=$n d=$OUTDEG seed=$cur_seed" >&2
            exit 7
          fi

          ((rep++))
        done
      done
    done
  done
}

if [[ -n "$MODEL_PRESET" ]]; then
  declare -a MODELS
  case "$MODEL_PRESET" in
    all) MODELS=(small medium large) ;;
    small|medium|large) MODELS=("$MODEL_PRESET") ;;
    *) echo "Unknown model preset: $MODEL_PRESET" >&2; exit 5 ;;
  esac

  for mdl in "${MODELS[@]}"; do
    case "$mdl" in
      small)
        NS=(500 1000 2000)
        OUTDEG=5 ;;
      medium)
        NS=(5000 10000 20000)
        OUTDEG=5 ;;
      large)
        NS=(50000 100000)
        OUTDEG=5 ;;
    esac
    run_matrix "$mdl"
  done
else
  run_matrix custom
fi

log "Done. Results saved to: $OUT_FILE"
exit 0
