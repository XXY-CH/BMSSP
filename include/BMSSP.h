// BMSSP.h - Optimized implementation based on paper Algorithm 1-3
// 说明：本文档实现了论文 BMSSP 算法（Algorithm 1~3）与其使用的分块队列数据结构。
#pragma once
#include "Graph.h"
#include <vector>
#include <queue>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cctype>
#include <cstdint>

// Compile-time gating for debug logs
#ifndef BMSSP_ENABLE_DEBUG
#define BMSSP_ENABLE_DEBUG 0
#endif

struct BMSSPResult
{
  // 新的边界 B'（分段处理中用于截断的值）。
  double Bprime;
  // 完成的顶点集合 X。
  std::vector<int> X;
  // X 中每个顶点的真实距离（与 dist 数组一致的快照）。
  std::vector<double> Xdist;
};

struct BMSSPStats
{
  // 分块队列的 pull 次数（提取一批次）。
  uint64_t pulls = 0;
  // 批量前置次数（batchPrepend 调用计数）。
  uint64_t batches = 0;
  // 插入次数（insert 调用计数）。
  uint64_t inserts = 0;
};

#ifdef BMSSP_USE_LEMMA33
#include "opt_landings/BlockQueueLemma33.h"

// Adapter to match existing BlockQueue interface using Lemma 3.3 DS
class BlockQueue
{
public:
  struct Item
  {
    // 关键字（顶点 id）
    int key;
    // 关键值（用于排序的距离）
    double val;
  };
  BlockQueue() = default;
  BlockQueue(int M, double B) { dq_.init(M, B); } // 初始化底层 Lemma 3.3 结构
  void init(int M, double B) { dq_.init(M, B); }
  void insert(int key, double val) { dq_.insert(key, val); } // 单元素插入
  void batchPrepend(const std::vector<Item> &batch)
  {
    if (batch.empty())
      return;
    std::vector<BQPair> L;
    L.reserve(batch.size());
    for (auto &it : batch)
      L.push_back({it.key, it.val});
    dq_.batchPrepend(L); // 批量前置
  }
  std::pair<std::vector<int>, double> pull() { return dq_.pull(); } // 提取一批最小关键值的元素与分隔符
  bool empty() const { return dq_.empty(); }                        // 判空

private:
  BlockQueueLemma33 dq_;
};

#else

// Optimized BlockQueue implementation (heap-based baseline)
class BlockQueue
{
public:
  struct Item
  {
    int key;
    double val;
  };

  BlockQueue() = default;
  BlockQueue(int M, double B) : block_size(M), upper_bound(B) {}

  void init(int M, double B)
  {
    // 设定块大小与上界分隔值
    block_size = std::max(1, M);
    upper_bound = B;
    // 清理内部索引与堆
    present.clear();
    heap.clear();
  }

  // Insert: O(max{1, log(N/M)}) amortized
  void insert(int key, double val)
  {
    // 仅接受小于上界的元素
    if (!(val < upper_bound))
      return;

    // 如果已有条目且新值不更优，则忽略
    auto it = present.find(key);
    if (it != present.end() && val >= it->second)
      return;

    // 记录更优值并入堆（惰性去重）
    present[key] = val;
    heap.push_back({key, val});
    std::push_heap(heap.begin(), heap.end(), Compare{});
  }

  // BatchPrepend: O(L·max{1, log(L/M)}) amortized
  void batchPrepend(const std::vector<Item> &batch)
  {
    if (batch.empty())
      return;

    // 批量合入更优条目
    for (const auto &item : batch)
    {
      if (!(item.val < upper_bound))
        continue;
      auto it = present.find(item.key);
      if (it == present.end() || item.val < it->second)
      {
        present[item.key] = item.val;
        heap.push_back(item);
      }
    }

    // 统一重建堆（避免多次 push_heap 成本）
    if (!heap.empty())
      std::make_heap(heap.begin(), heap.end(), Compare{});
  }

  // Pull: O(|S'|) amortized
  std::pair<std::vector<int>, double> pull()
  {
    // 若无元素，直接返回空与上界
    if (present.empty())
      return {{}, upper_bound};

    std::vector<int> result;
    result.reserve(std::min(block_size, (int)present.size()));

    // Extract up to block_size valid smallest items
    for (int i = 0; i < block_size && !heap.empty(); ++i)
    {
      Item cur;
      if (!popMinValid(cur))
        break;

      auto it = present.find(cur.key);
      if (it != present.end() && it->second == cur.val)
      {
        // 确认当前条目未过期，产出其键值
        result.push_back(cur.key);
        present.erase(it);
      }
      else
      {
        --i; // Try again（弹出的是过期条目，循环尝试下一个）
      }
    }

    // 若本批为空，返回上界作为分隔符
    if (result.empty())
      return {{}, upper_bound};

    // Determine separator as the smallest remaining valid value (or upper_bound)
    double sep = upper_bound;
    double mv;
    if (peekMinValid(mv))
      sep = mv;

    return {result, sep};
  }

  bool empty() const { return present.empty(); }

private:
  // 每次 pull 期望产出的元素数（块大小）
  int block_size = 1;
  // 分隔上界：不纳入队列的最大允许值
  double upper_bound = INF_DIST;
  // 惰性去重堆（包含可能过期的条目）
  std::vector<Item> heap;
  // 当前有效的 key->val 映射（判定过期与更优）
  std::unordered_map<int, double> present;

  struct Compare
  {
    bool operator()(const Item &a, const Item &b) const
    {
      return a.val > b.val; // Min-heap
    }
  };

  bool popMinValid(Item &out)
  {
    // 反复弹出堆顶，直到找到与 present 匹配的有效条目
    while (!heap.empty())
    {
      std::pop_heap(heap.begin(), heap.end(), Compare{});
      Item cur = heap.back();
      heap.pop_back();

      auto it = present.find(cur.key);
      if (it != present.end() && it->second == cur.val)
      {
        out = cur;
        return true;
      }
    }
    return false;
  }

  // Peek the minimum valid value without removing; lazily discard stale tops
  bool peekMinValid(double &outVal)
  {
    // Ensure heap property holds (batchPrepend may rebuild already)
    if (!heap.empty())
    {
      std::make_heap(heap.begin(), heap.end(), Compare{});
    }
    while (!heap.empty())
    {
      const Item &cur = heap.front();
      auto it = present.find(cur.key);
      if (it != present.end() && it->second == cur.val)
      {
        outVal = cur.val;
        return true;
      }
      std::pop_heap(heap.begin(), heap.end(), Compare{});
      heap.pop_back();
    }
    return false;
  }
};

#endif // BMSSP_USE_LEMMA33

// Algorithm 1: FindPivots
static void FindPivots(const Graph &g, double B, const std::vector<int> &F, int Sigma,
                       std::vector<double> &dist,
                       std::vector<int> &P, std::vector<int> &W)
{
  // n：顶点总数；dv：局部距离；root：记录起始 pivot（F 中元素）
  const int n = g.n;
  std::vector<double> dv(n, INF_DIST);
  std::vector<int> root(n, -1);

  W.clear();
  std::vector<char> seen(n, 0);
  std::vector<int> curr;
  // 初始化：从 F 中可行的根开始（dist[r] < B）
  for (int r : F)
  {
    if (0 <= r && r < n && dist[r] < B)
    {
      dv[r] = dist[r];
      root[r] = r;
      curr.push_back(r);
      if (!seen[r])
      {
        seen[r] = 1;
        W.push_back(r);
      }
    }
  }

  // Sigma rounds of relaxation
  for (int round = 0; round < Sigma && !curr.empty(); ++round)
  {
    std::vector<int> next;
    for (int u : curr)
    {
      if (!(dv[u] < B))
        continue;
      for (const auto &e : g.adj[u])
      {
        double nd = dv[u] + e.w;
        if (nd < B && nd <= dv[e.to])
        {
          if (nd < dv[e.to])
          {
            dv[e.to] = nd;
            root[e.to] = root[u];
            next.push_back(e.to);
          }
          if (!seen[e.to])
          {
            seen[e.to] = 1;
            W.push_back(e.to);
          }
        }
      }
    }
    curr.swap(next);

    // 若访问集过大（> Sigma*|F|），退化为把 F 直接作为 pivot 输出
    if ((int)W.size() > Sigma * (int)F.size())
    {
      P = F;
      return;
    }
  }

  // Select pivots with subtree size >= Sigma
  // 统计每个根的“子树规模”（即被其最短路触达的顶点数量）
  std::unordered_map<int, int> subtree_size;
  for (int v : W)
  {
    if (root[v] != -1)
      subtree_size[root[v]]++;
  }

  P.clear();
  for (int r : F)
  {
    // 子树规模达阈值的元素入选为 pivot
    if (subtree_size[r] >= Sigma)
      P.push_back(r);
  }
}

// Algorithm 2: BaseCase
static BMSSPResult BaseCase(const Graph &g, int k, double B,
                            std::vector<double> &dist, int Sigma)
{
  // 局部 Dijkstra（以 k 为源），限定距离 < B
  std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pq;
  std::vector<double> local_dist(g.n, INF_DIST);
  std::vector<bool> processed(g.n, false);

  local_dist[k] = dist[k];
  pq.push({dist[k], k});

  std::vector<std::pair<int, double>> completed;
  double Bprime = B; // default successful

  while (!pq.empty())
  {
    auto [d, u] = pq.top();
    pq.pop();

    // 超界或已处理则跳过
    if (processed[u] || d > local_dist[u] || d >= B)
      continue;

    processed[u] = true;
    // 若发现更优局部距离，写回全局 dist；记录完成序列
    if (local_dist[u] < dist[u])
    {
      dist[u] = local_dist[u];
      if (u != k)
        completed.push_back({u, local_dist[u]});
    }

    // stop when Σ+1 vertices (excluding k) are finalized; set B' to the (Σ+1)-th key
    // 当完成 Σ+1 个顶点（不含 k）后，以最后一个完成的距离作为 B'，并结束
    if ((int)completed.size() >= Sigma + 1)
    {
      Bprime = completed.back().second;
      break;
    }

    for (const auto &e : g.adj[u])
    {
      double nd = d + e.w;
      if (nd < B && nd < local_dist[e.to])
      {
        local_dist[e.to] = nd;
        pq.push({nd, e.to});
      }
    }
  }

  std::vector<int> X;
  std::vector<double> Xdist;
  // 仅收集 < B' 的完成顶点进入 X
  for (auto &p : completed)
  {
    if (p.second < Bprime)
    {
      X.push_back(p.first);
      Xdist.push_back(p.second);
    }
  }

  return {Bprime, X, Xdist};
}

// Algorithm 3: BMSSP main class
class BMSSPAlgo
{
private:
  static constexpr int pow2(int x) { return (x <= 0) ? 1 : (1 << x); }

  const Graph &g;
  int s;
  int Sigma, Tau;
  std::vector<double> dist;
  std::vector<std::pair<int, int>> completeLog;
  std::chrono::steady_clock::time_point startTime{};
  bool statsEnabled = false;
  BMSSPStats stats_{};

public:
  BMSSPAlgo(const Graph &g_, int s_, int /*n_*/, int Sigma_, int Tau_)
      : g(g_), s(s_), Sigma(Sigma_), Tau(Tau_), dist(g_.n, INF_DIST)
  {
    // 源点距离置 0
    dist[s] = 0.0;
    // 若传入 Sigma/Tau 为 0，则按论文公式用 log(n) 推导默认值
    int N = std::max(1, g_.n);
    double L = std::log((double)N);
    if (Sigma_ == 0)
      Sigma = std::max(1, (int)std::floor(std::pow(L, 1.0 / 3.0)));
    if (Tau_ == 0)
      Tau = std::max(1, (int)std::floor(std::pow(L, 2.0 / 3.0)));
    // 通过环境变量开启统计
    if (const char *stenv = std::getenv("BMSSP_STATS"))
    {
      int v = std::atoi(stenv);
      statsEnabled = (v != 0);
    }
  }

  const std::vector<double> &distances() const { return dist; }
  const std::vector<std::pair<int, int>> &completions() const { return completeLog; }
  const BMSSPStats &stats() const { return stats_; }

  BMSSPResult run(int ell, double B, const std::vector<int> &F)
  {
    // 超时守护：从第一次进入开始计 5 秒
    auto now = std::chrono::steady_clock::now();
    if (startTime.time_since_epoch().count() == 0)
      startTime = now;
    if (std::chrono::duration<double>(now - startTime).count() > 5.0)
      throw std::runtime_error("BMSSP timeout >5s at ell=" + std::to_string(ell));

    // 递归到底：ell=0 使用 BaseCase 处理
    if (ell == 0)
    {
      if (F.empty())
        return {B, {}};
      return BaseCase(g, F[0], B, dist, Sigma);
    }

    // 选取 pivots（P）与访问集合（A）
    std::vector<int> P, A;
    FindPivots(g, B, F, Sigma, dist, P, A);

    // Large workload fallback
    // 当工作量过大（|A| > Sigma*|F| 且 P==F）时，回退为 BaseCase
    if (P.size() == F.size() && std::equal(P.begin(), P.end(), F.begin()) &&
        A.size() > Sigma * F.size())
    {
      if (!F.empty())
        return BaseCase(g, F[0], B, dist, Sigma);
      else
        return {B, {}};
    }

    // 分块队列（容量 M，分隔上界 B）
    const int M = pow2(ell - 1) * Tau;
    BlockQueue dq(M, B);

    // Insert pivots
    // 将 pivot 的当前标签插入队列
    for (int v : P)
    {
      if (std::isfinite(dist[v]) && dist[v] < B)
      {
        dq.insert(v, dist[v]);
        if (statsEnabled)
          stats_.inserts++;
      }
    }

    // 预设 B0'：若 P 非空，取其最小标签；否则置为 +∞
    double B0prime = 0;
    if (!P.empty())
    {
      B0prime = dist[P[0]];
      for (int v : P)
        if (dist[v] < B0prime)
          B0prime = dist[v];
    }
    else
    {
      B0prime = INF_DIST;
    }

    // Main loop - optimized to reduce empty pulls
    std::vector<int> Uacc;
    size_t Ucount = 0;

    while (!dq.empty())
    {
      // 环内超时守护
      auto now2 = std::chrono::steady_clock::now();
      if (std::chrono::duration<double>(now2 - startTime).count() > 5.0)
        throw std::runtime_error("BMSSP timeout >5s in-loop at ell=" + std::to_string(ell));

      // 从队列拉取一批 Fnext 与分隔值 Bsep
      auto [Fnext, Bsep] = dq.pull();
      if (statsEnabled)
        stats_.pulls++;
      if (Fnext.empty())
      {
        // 空批：以当前分隔值作为 B0'，并结束
        B0prime = Bsep;
        break;
      }

      // 递归处理该批，获得其完成集合与子分隔 B'
      BMSSPResult sub = run(ell - 1, Bsep, Fnext);
      Ucount += sub.X.size();
      Uacc.insert(Uacc.end(), sub.X.begin(), sub.X.end());

      // 在最底层记录完成日志：u 由 pivot 完成（用于诊断）
      if (ell == 1 && Fnext.size() == 1)
      {
        int pivot = Fnext[0];
        for (int u : sub.X)
          completeLog.emplace_back(u, pivot);
      }

      // 写回更优的距离
      for (size_t i = 0; i < sub.X.size(); ++i)
      {
        int u = sub.X[i];
        double du = sub.Xdist[i];
        if (du < dist[u])
          dist[u] = du;
      }

      // 对完成顶点的出边进行松弛，并依据区间放入队列或待批量前置
      processEdgeRelaxation(sub.X, Bsep, sub.Bprime, B, dq);

      // Per Algorithm 3 step 4: batch-prepend pivots in Fnext with labels in [B', Bsep)
      // 将区间 [B', Bsep) 中的 pivot 标签批量前置
      if (!Fnext.empty())
      {
        std::vector<BlockQueue::Item> pivotBatch;
        pivotBatch.reserve(Fnext.size());
        for (int v : Fnext)
        {
          double dv = dist[v];
          if (dv >= sub.Bprime && dv < Bsep)
            pivotBatch.push_back({v, dv});
        }
        if (!pivotBatch.empty())
        {
          dq.batchPrepend(pivotBatch);
          if (statsEnabled)
            stats_.batches++;
        }
      }

      // 过量完成保护：超出 Ulimit 时提前返回
      const size_t Ulimit = (size_t)Sigma * (size_t)pow2(ell) * (size_t)Tau;
      if (Ucount > Ulimit)
      {
        B0prime = sub.Bprime;
        break;
      }
    }

    // 若队列耗尽但 B0' 仍未设置，则退回为当前 B
    if (dq.empty() && B0prime == 0)
      B0prime = B;

    // 若 B0' 为 +∞，则也退回为 B（全局结束）
    if (!std::isfinite(B0prime))
      B0prime = B;

    // 根据 B0' 收集完成集合 X（去重）
    std::vector<bool> mark(g.n, false);
    std::vector<int> X;
    X.reserve(Uacc.size());

    for (int v : Uacc)
    {
      if (!mark[v] && dist[v] < B0prime)
      {
        mark[v] = true;
        X.push_back(v);
      }
    }

    // 合并 A 中满足条件的顶点到 X
    for (int v : A)
    {
      if (!mark[v] && dist[v] < B0prime)
      {
        mark[v] = true;
        X.push_back(v);
      }
    }

    std::vector<double> Xd;
    Xd.reserve(X.size());
    for (int v : X)
      Xd.push_back(dist[v]);

    return {B0prime, X, Xd};
  }

private:
  void processEdgeRelaxation(const std::vector<int> &completed_vertices,
                             double Bsep, double Bprime, double B,
                             BlockQueue &dq)
  {
    // 局部收集待批量前置的条目（放入 [B', Bsep) 区间）
    std::vector<BlockQueue::Item> localPending;
    localPending.reserve(completed_vertices.size() * 4);

    for (int u : completed_vertices)
    {
      for (const auto &e : g.adj[u])
      {
        double cand = dist[u] + e.w;
        if (cand <= dist[e.to])
        {
          dist[e.to] = cand;

          // 依据区间分派：
          // [Bsep, B) 直接插入；[B', Bsep) 暂存，稍后 batchPrepend；其余忽略
          if (cand >= Bsep && cand < B)
          {
            dq.insert(e.to, cand);
            if (statsEnabled)
              stats_.inserts++;
          }
          else if (cand >= Bprime && cand < Bsep)
          {
            localPending.push_back({e.to, cand});
          }
        }
      }
    }
    if (!localPending.empty())
    {
      dq.batchPrepend(localPending);
      if (statsEnabled)
        stats_.batches++;
    }
  }
};
