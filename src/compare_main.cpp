// compare_main.cpp - Build-only, isolated comparison: BMSSP (Alg 1-3) vs Dijkstra
// 说明：本程序用于对比论文 BMSSP 算法与标准 Dijkstra 的运行时间与正确性。
#include "./include/Graph.h"
#include "./include/Dijkstra.h"
#include "./include/BMSSP.h"
#include <chrono>
#include <random>
#include <iostream>
#include <iomanip>
#include <unordered_set>

static Graph gen_const_deg_graph(int n, int outdeg, uint32_t seed)
{
  Graph g(n);
  // 构造固定出度 outdeg 的随机有向图；权值为 (0,1] 实数
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> vid(0, n - 1);
  std::uniform_real_distribution<double> w(0.0, 1.0);
  for (int u = 0; u < n; ++u)
  {
    std::unordered_set<int> used;
    used.insert(u);
    for (int k = 0; k < outdeg; ++k)
    {
      int v = vid(rng);
      if (v == u || used.count(v))
      {
        v = (v + 1) % n;
      }
      used.insert(v);
      g.addEdge(u, v, w(rng) + 1e-9); // break ties minimally
    }
  }
  return g;
}

int main(int argc, char **argv)
{
  int n = (argc > 1 ? std::atoi(argv[1]) : 2000);
  // 参数：argv[1]=n 顶点数；argv[2]=固定出度；argv[3]=随机种子（可选）
  int mdeg = (argc > 2 ? std::atoi(argv[2]) : 2); // constant out-degree
  int s = 0;
  uint32_t seed = 42;
  if (argc > 3)
  {
    // allow overriding RNG seed via argv[3]
    unsigned long sv = 0;
    try
    {
      sv = std::stoul(argv[3]);
      seed = static_cast<uint32_t>(sv);
    }
    catch (...)
    {
      // keep default seed if parsing fails
    }
  }
  Graph g = gen_const_deg_graph(n, mdeg, seed);

  // Parameters per paper: k=floor(log(n)^{1/3}), t=floor(log(n)^{2/3}) using natural log
  auto lg = [](double x)
  // 论文参数：Sigma=floor(log(n)^{1/3}), Tau=floor(log(n)^{2/3})（自然对数）
  { return std::log(std::max(2.0, x)); };
  double L = lg((double)n);
  int Sigma = std::max(1, (int)std::floor(std::pow(L, 1.0 / 3.0)));
  int Tau = std::max(1, (int)std::floor(std::pow(L, 2.0 / 3.0)));
  if (const char *kenv = std::getenv("BMSSP_K"))
  // 环境变量可强制覆盖 K/T
  {
    int kv = std::atoi(kenv);
    if (kv > 0)
      Sigma = kv;
  }
  if (const char *tenv = std::getenv("BMSSP_T"))
  {
    int tv = std::atoi(tenv);
    if (tv > 0)
      Tau = tv;
  }
  int ellTop = std::max(1, (int)std::ceil(L / std::max(1, Tau)));
  // 递归深度 ellTop 依据 Tau 推导

  // Run BMSSP
  auto t1 = std::chrono::steady_clock::now();
  BMSSPAlgo bm(g, s, g.n, Sigma, Tau);
  BMSSPResult res;
  bool bm_ok = true;
  std::string bm_err;
  try
  {
    res = bm.run(ellTop, INF_DIST, std::vector<int>{s});
  }
  catch (const std::exception &ex)
  {
    bm_ok = false;
    bm_err = ex.what();
  }
  auto t2 = std::chrono::steady_clock::now();

  // Run Dijkstra
  auto t3 = std::chrono::steady_clock::now();
  auto distD = dijkstra(g, s);
  auto t4 = std::chrono::steady_clock::now();

  double tb = std::chrono::duration<double>(t2 - t1).count();
  double td = std::chrono::duration<double>(t4 - t3).count();
  double ratio = (td > 0 ? tb / td : 0.0);

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "n=" << n << ", outdeg=" << mdeg << ", Sigma=" << Sigma << ", Tau=" << Tau << ", seed=" << seed << "\n";
  // 打印摘要：规模、参数与种子
  std::cout << "BMSSP_time(s)=" << tb << ", Dijkstra_time(s)=" << td << ", time_ratio(BMSSP/Dij)=" << ratio << "\n";
  if (!bm_ok)
  {
    std::cout << "BMSSP_ERROR: " << bm_err << "\n";
  }

  // Correctness check: 与原 compare_main.cpp 一致的详细校验与诊断输出
  const auto &distBM = bm.distances();
  const auto &logC = bm.completions();
  std::vector<int> who(n, -1);
  for (auto &pr : logC)
    if (pr.first >= 0 && pr.first < n)
      who[pr.first] = pr.second;
  int checked = 0, mism = 0, missing = 0;
  int printCap = 50;
  for (int v : res.X)
  {
    if (v < 0 || v >= g.n)
      continue;
    if (!std::isfinite(distBM[v]))
    {
      missing++;
      continue;
    }
    if (std::fabs(distBM[v] - distD[v]) > 1e-9)
    {
      if (printCap-- > 0)
      {
        std::cout << "MISSMATCH v=" << v
                  << " bmssp=" << distBM[v]
                  << " dijkstra=" << distD[v]
                  << " diff=" << std::fabs(distBM[v] - distD[v])
                  << " pivot=" << who[v]
                  << "\n";
      }
      mism++;
    }
    checked++;
    std::cout << "checked v=" << v
              << " bmssp=" << distBM[v]
              << " dijkstra=" << distD[v]
              << " diff=" << std::fabs(distBM[v] - distD[v])
              << " pivot=" << who[v]
              << "\n";
  }
  std::cout << "verify: checked=" << checked << ", mismatches=" << mism << ", missing=" << missing << "\n";

  bool verify_ok = (bm_ok && mism == 0 && missing == 0);
  std::cout << "status: " << (verify_ok ? "OK" : "FAIL") << "\n";

  // 若 B' == INF，再做全图抽查与全局前 50 条 mismatches 输出
  int sampleMism = 0, sampleChecked = 0;
  if (!std::isfinite(res.Bprime))
  {
    int samplePrintCap = 10;
    for (int v = 0; v < std::min(n, 10); ++v)
    {
      if (std::fabs(distBM[v] - distD[v]) > 1e-9)
      {
        if (samplePrintCap-- > 0)
        {
          std::cout << "MISSMATCH(sample) v=" << v
                    << " bmssp=" << distBM[v]
                    << " dijkstra=" << distD[v]
                    << " diff=" << std::fabs(distBM[v] - distD[v])
                    << "\n";
        }
        sampleMism++;
      }
      sampleChecked++;
    }
    std::cout << "verify-sample(all): checked=" << sampleChecked << ", mismatches=" << sampleMism << "\n";
  }

  int globalCap = 50, globalCnt = 0;
  for (int v = 0; v < n && globalCap > 0; ++v)
  {
    if (!std::isfinite(distBM[v]) || !std::isfinite(distD[v]))
      continue;
    if (std::fabs(distBM[v] - distD[v]) > 1e-9)
    {
      std::cout << "MISSMATCH(global) v=" << v
                << " bmssp=" << distBM[v]
                << " dijkstra=" << distD[v]
                << " diff=" << std::fabs(distBM[v] - distD[v])
                << "\n";
      --globalCap;
      ++globalCnt;
    }
  }
  if (globalCnt > 0)
  {
    std::cout << "mismatch-global-printed=" << globalCnt << " (cap=50)\n";
  }

  if (const char *st = std::getenv("BMSSP_STATS"))
  {
    int v = std::atoi(st);
    if (v != 0)
    {
      const auto &stc = bm.stats();
      std::cout << "stats: pulls=" << stc.pulls
                << ", batches=" << stc.batches
                << ", inserts=" << stc.inserts << "\n";
    }
  }
  if (const char *strict = std::getenv("BMSSP_STRICT"))
  {
    int v = std::atoi(strict);
    if (v != 0)
    {
      if (!verify_ok)
        return 2;
    }
  }
  return 0;
}
