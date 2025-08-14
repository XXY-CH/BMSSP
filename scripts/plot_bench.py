#!/usr/bin/env python3
# 说明：将 benchmark.sh 生成的 CSV 转换为多张对比曲线图与汇总表。
# 仅新增中文注释，不改变任何代码逻辑。
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path

def parse_args():
    p = argparse.ArgumentParser(description="Plot BMSSP benchmark CSV")
    p.add_argument("csv", help="Input CSV produced by benchmark.sh")
    p.add_argument("--out", default="bench_plots", help="Output directory")
    p.add_argument("--style", default="whitegrid", help="Seaborn style")
    p.add_argument("--dpi", type=int, default=160)
    p.add_argument("--palette", default="tab10")
    return p.parse_args()


def ensure_numeric(df, cols):
    for c in cols:
        if c in df.columns:
            df[c] = pd.to_numeric(df[c], errors="coerce")
    return df


def main():
    args = parse_args()
    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    sns.set_style(args.style)
    sns.set_palette(args.palette)

    df = pd.read_csv(args.csv)
    df = ensure_numeric(df, [
        "n", "outdeg", "sigma", "tau",
        "bmssp_time_s", "dijkstra_time_s", "ratio",
        "checked", "mismatches", "missing",
        "pulls", "batches", "inserts"
    ])

    df = df[df["bmssp_time_s"].notna() & df["dijkstra_time_s"].notna()]

    if "model" not in df.columns:
        df["model"] = "custom"

    agg = df.groupby(["model", "n", "variant"], as_index=False).agg({
        "ratio": "median",
        "bmssp_time_s": "median",
        "dijkstra_time_s": "median"
    })

    for mdl, sub in agg.groupby("model"):
        plt.figure(figsize=(7,4))
        sns.lineplot(data=sub, x="n", y="ratio", hue="variant", marker="o")
        plt.title(f"Time Ratio (BMSSP/Dijkstra) - {mdl}")
        plt.xscale("log")
        plt.ylabel("median ratio")
        plt.xlabel("n (log scale)")
        plt.tight_layout()
        plt.savefig(outdir / f"ratio_{mdl}.png", dpi=args.dpi)
        plt.close()

    for mdl, sub in agg.groupby("model"):
        plt.figure(figsize=(7,4))
        sns.lineplot(data=sub, x="n", y="bmssp_time_s", hue="variant", marker="o")
        plt.title(f"BMSSP Time (s) - {mdl}")
        plt.xscale("log")
        plt.yscale("log")
        plt.ylabel("median BMSSP time (s)")
        plt.xlabel("n (log scale)")
        plt.tight_layout()
        plt.savefig(outdir / f"bmssp_time_{mdl}.png", dpi=args.dpi)
        plt.close()

        plt.figure(figsize=(7,4))
        sns.lineplot(data=sub, x="n", y="dijkstra_time_s", hue="variant", marker="o")
        plt.title(f"Dijkstra Time (s) - {mdl}")
        plt.xscale("log")
        plt.yscale("log")
        plt.ylabel("median Dijkstra time (s)")
        plt.xlabel("n (log scale)")
        plt.tight_layout()
        plt.savefig(outdir / f"dijkstra_time_{mdl}.png", dpi=args.dpi)
        plt.close()

    summary = agg.groupby(["variant", "model"], as_index=False)["ratio"].median()
    summary.to_csv(outdir / "summary_ratio_median.csv", index=False)

    if {"pulls", "batches", "inserts"}.issubset(df.columns):
        agg_ops = df.groupby(["model", "n", "variant"], as_index=False).agg({
            "pulls": "median",
            "batches": "median",
            "inserts": "median"
        })
        for mdl, sub in agg_ops.groupby("model"):
            for col, ylab in [("pulls", "median pulls"), ("batches", "median batches"), ("inserts", "median inserts")]:
                if col not in sub.columns:
                    continue
                plt.figure(figsize=(7,4))
                sns.lineplot(data=sub, x="n", y=col, hue="variant", marker="o")
                plt.title(f"{col.capitalize()} - {mdl}")
                plt.xscale("log")
                plt.ylabel(ylab)
                plt.xlabel("n (log scale)")
                plt.tight_layout()
                plt.savefig(outdir / f"{col}_{mdl}.png", dpi=args.dpi)
                plt.close()

    print(f"Saved plots to {outdir}")


if __name__ == "__main__":
    main()
