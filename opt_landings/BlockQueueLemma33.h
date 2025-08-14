#pragma once
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <limits>

// 基于分块的队列（论文 Lemma 3.3 的工程化实现，概要）：
// - 维护两段块序列：D0（仅 batch-prepend 进入的块，表示“比现有都小”的批次），D1（仅 insert 进入的块）。
// - 每个块的容量最多为 M；D1 的每个块维护“块上界 UB”（块内最大 val），并在平衡树（std::map）中按 UB 有序索引。
// - 三类操作复杂度：
//   Insert: 摊还 O(max{1, log(N/M)})
//   BatchPrepend: 摊还 O(L * max{1, log(L/M)})，可通过中位数拆分控制
//   Pull: 摊还 O(|S'|)，通过从 D0 优先、再从 D1 的最小 UB 块，直到收集满 M 个为止
// 说明：
//   该实现尽量贴近论文复杂度，适度控制常数。

struct BQPair
{
  // 键（顶点 id）
  int key;
  // 值（用于比较的距离标签）
  double val;
};

class BlockQueueLemma33
{
public:
  BlockQueueLemma33() = default;
  BlockQueueLemma33(int M, double B) { init(M, B); }

  void init(int M, double B)
  {
    // 设置块容量 m_ 与上界 bound_，并清空结构
    m_ = std::max(1, M);
    bound_ = B;
    clear();
  }

  void clear()
  {
    // 清空 D0、D1、索引树与键值映射
    D0_.clear();
    D1_.clear();
    ubTree_.clear();
    key2val_.clear();
  }

  // Insert(key,val) into D1 with upper bound tree
  void insert(int key, double val)
  {
    // 仅接受小于上界的条目
    if (!(val < bound_))
      return;
    // 若已有且不更优，直接忽略
    auto it = key2val_.find(key);
    if (it != key2val_.end() && val >= it->second)
      return;
    if (it != key2val_.end())
    {
      // delete old occurrence lazily: we keep key2val_ canonical
    }
    key2val_[key] = val;

    // 在 D1 的 UB 树中找到第一个 UB >= val 的块（lower_bound）
    auto pos = ubTree_.lower_bound(val);
    if (pos == ubTree_.end())
    {
      // 若不存在，则在 D1 尾部新建块并放入
      D1_.emplace_back();
      auto bi = std::prev(D1_.end());
      bi->push_back({key, val});
      double UB = blockUpperBound(*bi);
      ubTree_.emplace(UB, bi);
      if ((int)bi->size() > m_)
        splitBlockD1(bi);
      return;
    }
    auto bi = pos->second;
    bi->push_back({key, val});
    if ((int)bi->size() > m_)
      splitBlockD1(bi);
  }

  // Batch prepend L elements as strictly smaller than any existing
  void batchPrepend(const std::vector<BQPair> &L)
  {
    if (L.empty())
      return;
    // 逐键保留更优值（过滤）
    std::unordered_map<int, double> tmp;
    tmp.reserve(L.size() * 2);
    for (auto &p : L)
    {
      if (!(p.val < bound_))
        continue;
      auto itG = key2val_.find(p.key);
      double best = (itG == key2val_.end() ? std::numeric_limits<double>::infinity() : itG->second);
      if (p.val < best)
        tmp[p.key] = p.val;
    }
    if (tmp.empty())
      return;

    // 构建向量并按值排序（也可用中位数递归拆分），再按 m_ 大小切分为多个块
    std::vector<BQPair> arr;
    arr.reserve(tmp.size());
    for (auto &kv : tmp)
      arr.push_back({kv.first, kv.second});

    // 排序可行；理论分析下该路径的代价受控
    std::sort(arr.begin(), arr.end(), [](const BQPair &a, const BQPair &b)
              { return a.val < b.val; });

    // 按递增顺序将块前插到 D0；D0 表示“严格小于现有所有”的区间
    for (size_t i = 0; i < arr.size();)
    {
      size_t j = std::min(arr.size(), i + (size_t)m_);
      D0_.emplace_front();
      auto &blk = D0_.front();
      for (size_t t = i; t < j; ++t)
        blk.push_back(arr[t]);
      i = j;
    }

    // 更新全局 canonical 值
    for (auto &p : arr)
      key2val_[p.key] = p.val;
  }

  // Pull up to m_ smallest across D0+D1; return keys and separator sep
  std::pair<std::vector<int>, double> pull()
  {
    std::vector<int> out;
    if (empty())
      return {out, bound_};
    out.reserve(std::min(m_, (int)key2val_.size()));

    double sep = bound_;
    // 优先从 D0 再到 D1 取元素，直到达到 m_
    // D0 始终代表比 D1 更小的区间
    while (!D0_.empty() && (int)out.size() < m_)
    {
      auto &blk = D0_.front();
      while (!blk.empty() && (int)out.size() < m_)
      {
        auto p = blk.front();
        blk.pop_front();
        if (confirmCurrent(p))
          out.push_back(p.key);
      }
      if (blk.empty())
        D0_.pop_front();
    }
    if ((int)out.size() < m_)
    {
      // D1：按 UB 最小的块优先提取；块内无序但受 UB 约束
      while (!ubTree_.empty() && (int)out.size() < m_)
      {
        auto it = ubTree_.begin();
        auto bi = it->second; // list<BQPair>* iterator
        // pull from this block
        while (!bi->empty() && (int)out.size() < m_)
        {
          auto p = bi->front();
          bi->pop_front();
          if (confirmCurrent(p))
            out.push_back(p.key);
        }
        // recompute or remove block
        ubTree_.erase(it);
        if (!bi->empty())
        {
          double UB = blockUpperBound(*bi);
          ubTree_.emplace(UB, bi);
        }
        else
        {
          D1_.erase(bi);
        }
      }
    }

    if (!empty())
      sep = currentMinValue(); // 计算当前剩余最小值作为分隔符
    if (out.empty())
      return {out, sep};
    return {out, sep};
  }

  bool empty() const { return key2val_.empty(); }

private:
  using Block = std::list<BQPair>;
  int m_ = 1;
  double bound_ = std::numeric_limits<double>::infinity();
  std::list<Block> D0_;                                          // batch-prepend 区（front 块最小）
  std::list<Block> D1_;                                          // insert 区（按 UB 在 ubTree_ 中排序）
  std::map<double, typename std::list<Block>::iterator> ubTree_; // UB -> 块迭代器
  std::unordered_map<int, double> key2val_;                      // 每个 key 的规范值（用于判定过期）

  static double blockUpperBound(const Block &blk)
  {
    // 计算块内值的上界（最大值）；若出现非有限值则返回 +∞
    double ub = -std::numeric_limits<double>::infinity();
    for (auto &p : blk)
      ub = std::max(ub, p.val);
    if (!std::isfinite(ub))
      return std::numeric_limits<double>::infinity();
    return ub;
  }

  // After insert, if block exceeds m_, split using median
  void splitBlockD1(typename std::list<Block>::iterator bi)
  {
    // 当块大小超过 m_ 时，基于中位数进行二分拆分，并在 UB 树中重新注册
    Block &blk = *bi;
    std::vector<BQPair> tmp(blk.begin(), blk.end());
    blk.clear();
    // median split around mid
    size_t mid = tmp.size() / 2;
    std::nth_element(tmp.begin(), tmp.begin() + mid, tmp.end(), [](const BQPair &a, const BQPair &b)
                     { return a.val < b.val; });
    double pivot = tmp[mid].val;
    Block left, right;
    for (auto &p : tmp)
    {
      if (p.val <= pivot)
        left.push_back(p);
      else
        right.push_back(p);
    }
    // replace bi with left, insert right after
    *bi = std::move(left);
    auto biRight = D1_.insert(std::next(bi), std::move(right));

    // re-register UBs
    double UB1 = blockUpperBound(*bi);
    double UB2 = blockUpperBound(*biRight);
    ubTree_.emplace(UB1, bi);
    ubTree_.emplace(UB2, biRight);
  }

  bool confirmCurrent(const BQPair &p)
  {
    // 确认条目是否为当前有效值（过滤过期副本）；若有效则从 key2val_ 移除并返回 true
    auto it = key2val_.find(p.key);
    if (it == key2val_.end())
      return false;
    if (it->second != p.val)
      return false; // stale copy
    key2val_.erase(it);
    return true;
  }

  double currentMinValue() const
  {
    // 估计当前最小值：检查 D0 首块与 D1 的最小 UB 块
    double v = bound_;
    // check D0 front
    if (!D0_.empty())
    {
      for (auto &p : D0_.front())
      {
        v = std::min(v, p.val);
      }
    }
    // check D1 min-UB block front items
    if (!ubTree_.empty())
    {
      auto it = ubTree_.begin();
      for (auto &p : *it->second)
      {
        v = std::min(v, p.val);
      }
    }
    return v;
  }
};
