// 说明：该文件为 Lemma 3.3 分块队列（BlockQueueLemma33）的最小自测。
// 步骤：
// 1) 先插入一批较大的值（1000+i），确保不会被第一批 pull 取出。
// 2) 再通过 batchPrepend 插入更小的一批（值为 0..19），应优先被 pull。
// 3) 连续 pull 三次，观察每批返回的元素数与分隔符 sep 是否合理。

#include <iostream>
#include <vector>
#include <random>
#include <cassert>
#include "BlockQueueLemma33.h"

int main()
{
  // 参数：M=8（每次 pull 期望输出上限），B=1e9（上界，插入值需 < B 才会进入队列）
  BlockQueueLemma33 q(8, 1e9);

  // 先插入大值，模拟“低优先级”元素
  for (int i = 0; i < 20; ++i)
  {
    q.insert(i, 1000 + i);
  }

  // 批量前置较小的键值对，将作为第一批被取出的候选
  std::vector<BQPair> prep;
  for (int i = 0; i < 20; ++i)
  {
    prep.push_back({100 + i, (double)i});
  }
  q.batchPrepend(prep);

  auto [s1, sep1] = q.pull();
  std::cout << "pull1 size=" << s1.size() << " sep=" << sep1 << "\n";
  assert(!s1.empty());
  auto [s2, sep2] = q.pull();
  std::cout << "pull2 size=" << s2.size() << " sep=" << sep2 << "\n";
  auto [s3, sep3] = q.pull();
  std::cout << "pull3 size=" << s3.size() << " sep=" << sep3 << "\n";
  return 0;
}
