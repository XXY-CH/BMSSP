// Dijkstra.h - Isolated standard Dijkstra for comparison
// 说明：本文件实现标准 Dijkstra 算法（使用小根堆），作为对比基准。
#pragma once
#include "Graph.h"
#include <queue>
#include <utility>

inline std::vector<double> dijkstra(const Graph &g, int s)
{
  // 初始化距离数组为 +∞
  std::vector<double> dist(g.n, INF_DIST);
  // 源点距离置 0
  dist[s] = 0.0;
  // 小根堆中元素类型：（当前距离，顶点）
  using P = std::pair<double, int>;
  std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
  // 将源点入堆
  pq.push({0.0, s});
  while (!pq.empty())
  {
    // 取出堆顶的最小距离对
    auto [d, u] = pq.top();
    pq.pop();
    // 若该条目已过期（存在更短距离），则跳过
    if (d > dist[u])
      continue;
    // 松弛 u 的所有出边
    for (const auto &e : g.adj[u])
    {
      double nd = d + e.w;
      if (nd < dist[e.to])
      {
        dist[e.to] = nd;
        // 推入新状态到堆中（可能存在重复条目，靠上面的过期判断过滤）
        pq.push({nd, e.to});
      }
    }
  }
  // 返回从 s 到所有顶点的最短距离
  return dist;
}
