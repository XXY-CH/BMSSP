# 项目实现：BMSSP

## 项目概述
严格按论文 Algorithm 1-3 与 Lemma 实现 BMSSP，和 Dijkstra 完全隔离，对比时间与复杂度，并内置 5s 超时保护与准确性校验。

## 优化成果（2025-01-21）
- BlockQueue 重构为 D0/D1 分块，复杂度对齐 Lemma 3.3
- 主循环减少空 Pull/冗余操作，保持摊还复杂度
- Makefile 支持 O2/O0 与 lemma33/baseline 变体对比
- 统计 Pull/Batch/Insert（可选），支持多规模基准与绘图

## 核心文件
```
Graph.h            基础图结构（Edge, Graph, INF_DIST）
Dijkstra.h         标准 Dijkstra 实现
BMSSP.h            论文 Algorithm 1-3 完整实现：
                                     - FindPivots（Alg.1）: Bellman-Ford 式 k 轮松弛
                                     - BaseCase（Alg.2） : 单源 mini-Dijkstra
                                     - 递归（Alg.3）     : 分层分块推进
                                     - BlockQueue（Lemma 3.3）
compare_main.cpp   对比主程序：随机图 + 准确性校验 + 性能统计
Makefile           构建系统：O2/O0 与多变体、基准与绘图入口
scripts/benchmark.sh 多规模基准脚本（CSV 输出，可收集操作计数）
scripts/plot_bench.py 绘图脚本（比值/绝对时间/操作计数）
```

## 参数（论文公式）
- Sigma = max(1, floor(log(n)^(1/3)))
- Tau   = max(1, floor(log(n)^(2/3)))
- M     = 2^(ell-1) * Tau（动态分块容量）
- 超时：5s（BMSSP.h 中可改后重编译）

## 编译与快速试跑
```bash
# 默认：O2 + lemma33（论文队列）
make compare_bmssp
./bin/compare_bmssp <n> <outdeg> <seed>
# 示例
./bin/compare_bmssp 1000 5 42

# O2 vs O0 性能对比（均为 lemma33）
make benchmark

# 单独 O0 版本
make o0 && ./compare_bmssp_o0 1000 5 42
```

## 构建变体与调试
- lemma33（默认）：make compare_bmssp && ./bin/compare_bmssp 1000 5 42
- baseline（堆队列、O2）：make baseline && ./bin/compare_bmssp_baseline 1000 5 42
- 调试（打印关键阶段）：
    - 非 lemma33：make debug && ./bin/compare_bmssp_debug 1000 5 42
    - lemma33：make debug_lemma33 && ./bin/compare_bmssp_debug_lemma33 1000 5 42
- O0 变体：
    - 非 lemma33：make o0 && ./bin/compare_bmssp_o0 1000 5 42
    - lemma33：make o0_lemma33 && ./bin/compare_bmssp_o0_lemma33 1000 5 42

运行时环境变量：
- 调试开关：BMSSP_DEBUG=1 ./bin/compare_bmssp 1000 5 42
- 统计操作计数：BMSSP_STATS=1 ./bin/compare_bmssp 1000 5 42（输出 stats: pulls/batches/inserts）
- 正确性严格模式：BMSSP_STRICT=1 ./bin/compare_bmssp 1000 5 42（若存在 mismatches/missing 或运行异常，进程非零退出）

论文 BlockQueue 自测（仅数据结构）：
```bash
cd opt_landings && make && ./test_blockqueue
```

## 典型输出
```
n=1000, outdeg=5, Sigma=1, Tau=3
BMSSP_time(s)=0.000099, Dijkstra_time(s)=0.000083, time_ratio(BMSSP/Dij)=1.188
verify: checked=934, mismatches=0, missing=0
# 若开启 BMSSP_STATS=1，会额外输出：
stats: pulls=..., batches=..., inserts=...
```

## 多规模基准（CSV）
- 一键默认基准：
    - make bench               # 使用 scripts/benchmark.sh 的默认参数
    - make bench-all           # small/medium/large 预设 + 变体 "lemma33 baseline"
    - make bench-all-stats     # 同上，但自动 BMSSP_STATS=1，CSV 含操作计数
    - 严格校验：BMSSP_STRICT=1 make bench-all-stats（遇到 status: FAIL 立即失败）
- 自定义示例：
    - bash benchmark.sh -n "1000 3000 10000" -d 5 -s "42 43" -v "lemma33 baseline" -r 2 -o results.csv
- 预设规模：
    - small : n={500,1000,2000}, outdeg=5
    - medium: n={5000,10000,20000}, outdeg=5
    - large : n={50000,100000}, outdeg=5

CSV 列：
model,variant,n,outdeg,seed,sigma,tau,bmssp_time_s,dijkstra_time_s,ratio,checked,mismatches,missing,pulls,batches,inserts

## 绘图（时间比/绝对时间/操作计数）
```bash
# 安装依赖（建议虚拟环境）
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# 绘图（输入为基准 CSV，输出到 plots/）
python scripts/plot_bench.py bench_all.csv --out plots
```
将生成：
- ratio_small.png / ratio_medium.png / ratio_large.png（BMSSP/Dijkstra 时间比）
- bmssp_time_*.png / dijkstra_time_*.png（对数坐标）
- 若 CSV 含计数：pulls_*.png / batches_*.png / inserts_*.png
- summary_ratio_median.csv（跨模型各变体的中位时间比摘要）

## 大中小三种示例（从零到图）
```bash
# 1) 生成基准 CSV（含操作计数）
make bench-all-stats             # 产出 bench_all.csv

# 2) 绘图
make plot PLOT_IN=bench_all.csv PLOT_OUT=plots

# 3) 快速对比 O2 vs O0（lemma33）
make benchmark
```

## 一次性执行所有测试（含自测与小规模冒烟）
```bash
make test-all
```
会执行：
- BlockQueue Lemma 3.3 自测（opt_landings/test_blockqueue）
- lemma33 小规模冒烟并输出 stats（n=1000, outdeg=5, seed=42，严格模式，默认开启计数 BMSSP_STATS=1）
- baseline 小规模冒烟并输出 stats（n=1000, outdeg=5, seed=42，严格模式，默认开启计数 BMSSP_STATS=1）

如需临时关闭计数，可执行：
```bash
BMSSP_STATS=0 make test-all
```

## 校验与排障建议
- 结果不一致：先 baseline 与 lemma33 各跑一遍；必要时用 debug(_lemma33) 观察 B' / B_sep 演化与 Pull/Batch 次数
- 性能异常：对比 O0/O2；检查是否存在“空 Pull”或 Σ/τ 过小导致递归层数偏深
- 崩溃/超时：5s 超时会报出失败层 ell；如 n 很大，先 baseline + O2 复现，再切换 lemma33

## 已验证
- 与 Dijkstra 校验一致，测试样例均通过
- 多规模下稳定运行，时间与理论复杂度趋势吻合

## 技术特性
- 论文严格对齐（Algorithm 1-3 + Lemma）
- 与 Dijkstra 完全隔离实现
- BlockQueue 摊还复杂度与论文一致
- 5s 超时保护，支持 O2/O0 对比
- 可选 Pull/Batch/Insert 统计与可视化
