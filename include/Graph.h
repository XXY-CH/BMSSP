// Graph.h - Minimal directed graph utilities for 项目实现
// 说明：本文件定义最小化的有向图结构及边结构，供 BMSSP 与 Dijkstra 使用。
#pragma once
#include <vector>
#include <limits>

struct Edge
{
  // 终点编号
  int to;
  // 边权（非负权）
  double w;
};

struct Graph
{
  // 顶点数
  int n = 0;
  // 邻接表：adj[u] 存放从 u 出发的所有出边
  std::vector<std::vector<Edge>> adj; // outgoing
  Graph() = default;
  // 构造函数：初始化 n 个顶点并分配邻接表
  explicit Graph(int n_) : n(n_), adj(n_) {}
  // 重置图为 n_ 个顶点，并清空所有边
  void reset(int n_)
  {
    n = n_;
    adj.assign(n_, {});
  }
  // 添加一条有向边 u->v，权值为 w；越界时忽略
  void addEdge(int u, int v, double w)
  {
    if (0 <= u && u < n && 0 <= v && v < n)
      adj[u].push_back({v, w});
  }
};

// 无穷大距离常量，用于初始化最短路数组
constexpr double INF_DIST = std::numeric_limits<double>::infinity();
