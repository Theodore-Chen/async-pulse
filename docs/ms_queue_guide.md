# MS Queue 与 Hazard Pointer 完整指南

## 概述

本项目实现了一个基于 Michael-Scott 算法的无锁队列（`ms_queue`），位于 [src/queue/ms_queue.h](../src/queue/ms_queue.h)。该队列采用策略模式设计，支持多种内存回收策略，默认使用 Hazard Pointer 策略确保内存安全。

### 核心特性

- **无锁实现**：基于 Michael-Scott 算法，支持多生产者多消费者
- **策略模式**：通过模板参数注入不同的内存回收策略
- **内存安全**：默认使用 Hazard Pointer 防止 use-after-free
- **缓存行对齐**：防止伪共享，提升并发性能

---

## 竞态条件分析

### 问题描述

MS 队列在 `dequeue_impl` 中存在一个 **use-after-free** 竞态条件，可能导致：

1. 死循环
2. 段错误 (Segmentation Fault)
3. 内存损坏 ("malloc(): unaligned tcache chunk detected")

### 竞态条件时间线

| 时间 | 线程 A (当前线程) | 线程 B (其他消费者) | 内存状态 |
|------|------------------|-------------------|----------|
| T1 | `head = 0x1000` | - | `0x1000` 有效 |
| T2 | `next = head->next` | - | 读取 next |
| T3 | - | CAS 成功: `head_ = 0x2000` | `0x1000` 仍有效 |
| T4 | - | `delete (node*)0x1000` | `0x1000` **已释放** |
| T5 | CAS 成功 | - | head_ 更新为 0x2000 |
| T6 | `f(next->data)` | - | **访问已释放内存！** |

### 为什么会导致死循环

当访问已释放的内存时：

1. `next` 可能读取到垃圾值
2. 如果垃圾值恰好形成循环引用，线程会陷入无限循环
3. 在多消费者场景下，某个线程卡住会导致整个测试超时

---

## 解决方案

### 方案演进历史

1. **初始实现**：直接访问 `head->next` 和 `next->data` → GCC 死循环
2. **双重验证**：添加两次 `head` 验证 → GCC 通过，Clang 失败（"unaligned tcache chunk"）
3. **Hazard Pointer**：完整的 HP 实现 → 测试隔离问题（全局状态污染）
4. **最终方案**：数据移动 + 双重验证 → GCC 和 Clang 都通过

### 最终方案：数据移动 + 双重验证

```cpp
template <bool Blocking, typename Func>
bool dequeue_impl(Func&& f) {
    while (true) {
        node* head = head_.load(std::memory_order_acquire);

        // 验证 1: 读取 head 后立即验证
        if (head != head_.load(std::memory_order_acquire)) {
            continue;
        }

        node* next = head->next.load(std::memory_order_acquire);

        // 验证 2: 读取 next 后再次验证（防止 use-after-free）
        if (head != head_.load(std::memory_order_acquire)) {
            continue;
        }

        // 关键：在 CAS 之前先移动数据到本地临时变量
        // 这样即使 CAS 后 next 被其他线程删除，我们也已经保存了数据
        value_type temp_data = std::move(next->data);

        if (head_.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
            // CAS 成功，现在可以安全使用数据
            std::forward<Func>(f)(temp_data);
            delete head;
            return true;
        }
        // CAS 失败，temp_data 会在下次循环时被销毁
    }
}
```

### 为什么这个方案有效

1. **双重验证**：在访问 `head->next` 前后两次验证 `head` 未被修改
2. **数据移动**：在 CAS 之前将 `next->data` 移动到本地临时变量
3. **安全访问**：即使 CAS 成功后 `next` 被删除，`temp_data` 也是安全的本地副本

---

## 策略模式设计

### 架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                    ms_queue<T, GC>                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  MS Queue 核心逻辑（Michael-Scott 算法）                   │  │
│  │  - enqueue() / dequeue()                                  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                              │                                │
│                              ▼                                │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │           reclamation_strategy (接口)                      │  │
│  │  + protect(ptr, slot)                                     │  │
│  │  + clear_protect(slot)                                     │  │
│  │  + clear_all_protects()                                    │  │
│  │  + retire(ptr, deleter)                                    │  │
│  │  + try_reclaim()                                           │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                               │
                ┌──────────────┼──────────────┐
                ▼              ▼              ▼
    ┌──────────────────┐  ┌──────────────────┐
    │ hazard_pointer_  │  │  defer_delete_   │
    │    strategy      │  │    strategy       │
    └──────────────────┘  └──────────────────┘
```

### 策略接口

```cpp
namespace detail {

class reclamation_strategy {
public:
    virtual ~reclamation_strategy() = default;

    // 保护指针，防止被回收
    virtual void protect(void* ptr, size_t slot) = 0;

    // 清除保护
    virtual void clear_protect(size_t slot) = 0;

    // 清除所有保护
    virtual void clear_all_protects() = 0;

    // 延迟回收指针
    virtual void retire(void* ptr, std::function<void(void*)> deleter) = 0;

    // 尝试回收延迟节点
    virtual void try_reclaim() = 0;
};

} // namespace detail
```

### 可用策略

#### 1. Hazard Pointer 策略（推荐）

```cpp
class hazard_pointer_strategy : public reclamation_strategy {
public:
    static constexpr size_t MAX_HAZARD_POINTERS = 8;
    static constexpr size_t MAX_THREADS = 128;
    static constexpr size_t RETIRE_LIMIT = 100;
};
```

**特性**：

- 线程本地 hazard pointers（每个线程独立的保护槽位）
- 全局 hazard pointer 注册表（跨线程可见）
- 延迟回收队列（retire list）
- 基于 ABA 问题的安全检查

#### 2. 延迟删除策略

```cpp
class defer_delete_strategy : public reclamation_strategy {
private:
    static thread_local retire_list t_retire_list;
    static constexpr size_t RETIRE_LIMIT = 100;
};
```

**特性**：

- 简单的线程本地延迟队列
- 无保护机制（`protect()` 和 `clear_protect()` 为空实现）
- 达到阈值时批量回收

### 策略选择指南

| 场景 | 推荐策略 | 原因 |
|------|---------|------|
| 生产环境、高并发 | `hazard_pointer_strategy` | 最安全，通过 ASan 验证 |
| 性能敏感、低并发 | `defer_delete_strategy` | 开销更低，简单场景下足够安全 |
| 学习研究 | 两种都试试 | 对比性能和安全性 |

---

## API 参考

### 类型定义

```cpp
template <typename T, typename GC = detail::hazard_pointer_strategy>
class ms_queue {
public:
    using value_type = T;
    using gc_type = GC;
};
```

### 构造和析构

```cpp
ms_queue();
~ms_queue();

// 禁止拷贝和移动
ms_queue(const ms_queue&) = delete;
ms_queue& operator=(const ms_queue&) = delete;
ms_queue(ms_queue&&) = delete;
ms_queue& operator=(ms_queue&&) = delete;
```

### 查询方法

```cpp
// 检查队列是否为空
bool empty();

// 获取队列大小（近似值）
size_t size();
```

### 入队方法

```cpp
// 拷贝入队
bool enqueue(const value_type& val);

// 移动入队
bool enqueue(value_type&& val);

// 就地构造
template <typename... Args>
bool emplace(Args&&... args);

// 使用 Lambda 入队
template <typename Func>
bool enqueue_with(Func&& f);

// 尝试入队（非阻塞版本）
template <typename Func>
bool try_enqueue_with(Func&& f);
```

### 出队方法

```cpp
// 拷贝出队
bool dequeue(value_type& val);

// Optional 出队
std::optional<value_type> dequeue();

// 使用 Lambda 出队
template <typename Func>
bool dequeue_with(Func&& f);

// 尝试出队（非阻塞版本）
template <typename Func>
bool try_dequeue_with(Func&& f);
```

### 关闭和清理

```cpp
// 关闭队列（拒绝新入队，但可以继续出队）
void close();

// 检查是否已关闭
bool is_closed();

// 清空队列（删除所有节点）
void clear();
```

---

## 使用示例

### 基本用法

```cpp
#include "queue/ms_queue.h"

// 创建队列（默认使用 Hazard Pointer 策略）
ms_queue<uint32_t> queue;

// 入队
queue.enqueue(42);
queue.emplace(100);  // 就地构造

// 出队
uint32_t value;
if (queue.dequeue(value)) {
    std::cout << "Dequeued: " << value << std::endl;
}

// 检查状态
if (queue.empty()) {
    std::cout << "Queue is empty" << std::endl;
}
```

### 使用 Lambda 回调

```cpp
// 入队回调
queue.enqueue_with([](uint32_t& dest) {
    dest = 42;
});

// 出队回调
queue.dequeue_with([](uint32_t& src) {
    std::cout << "Got: " << src << std::endl;
});
```

### 选择不同策略

```cpp
#include "opt/hazard_pointer_strategy.h"
#include "opt/reclamation_strategy.h"

// 默认使用 Hazard Pointer
ms_queue<uint32_t> queue1;

// 显式指定 Hazard Pointer
ms_queue<uint32_t, detail::hazard_pointer_strategy> queue2;

// 使用延迟删除策略
ms_queue<uint32_t, detail::defer_delete_strategy> queue3;
```

### 生产者-消费者模式

```cpp
#include "queue/ms_queue.h"
#include <thread>
#include <vector>

ms_queue<std::string> queue;

// 生产者线程
auto producer = [&]() {
    for (int i = 0; i < 1000; ++i) {
        queue.emplace("Message " + std::to_string(i));
    }
    queue.close();
};

// 消费者线程
auto consumer = [&]() {
    std::string msg;
    while (queue.dequeue(msg)) {
        std::cout << "Processing: " << msg << std::endl;
    }
};

std::thread t1(producer);
std::thread t2(consumer);

t1.join();
t2.join();
```

### 多生产者多消费者

```cpp
ms_queue<int> queue;
std::vector<std::thread> producers;
std::vector<std::thread> consumers;

// 启动 4 个生产者
for (int i = 0; i < 4; ++i) {
    producers.emplace_back([&, id = i]() {
        for (int j = 0; j < 250; ++j) {
            queue.enqueue(id * 250 + j);
        }
    });
}

// 启动 4 个消费者
for (int i = 0; i < 4; ++i) {
    consumers.emplace_back([&]() {
        int value;
        while (queue.dequeue(value)) {
            // 处理 value
        }
    });
}

// 等待所有生产者完成
for (auto& t : producers) {
    t.join();
}
queue.close();

// 等待所有消费者完成
for (auto& t : consumers) {
    t.join();
}
```

---

## 性能考虑

### 内存对齐

队列的关键成员都使用缓存行对齐，避免伪共享：

```cpp
alignas(CACHE_LINE_SIZE) std::atomic<node*> head_;
alignas(CACHE_LINE_SIZE) std::atomic<node*> tail_;
alignas(CACHE_LINE_SIZE) std::atomic<bool> is_closed_{false};
```

### 内存序选择

- `memory_order_acquire`: 用于读取操作，确保可见性
- `memory_order_release`: 用于写入操作，确保可见性
- `memory_order_acq_rel`: 用于 CAS 操作，同时提供 acquire 和 release 语义

### 数据移动开销

额外的移动构造操作开销：

- 对于基本类型（如 `uint32_t`）：等同于拷贝，几乎无开销
- 对于复杂类型：一次移动构造，通常比拷贝更高效

### 策略性能对比

| 策略 | 内存开销 | CPU 开销 | 安全性 | 适用场景 |
|------|---------|---------|--------|---------|
| `hazard_pointer_strategy` | 中等（全局表 + 延迟队列） | 低（检查保护指针） | 高 | 生产环境、高并发场景 |
| `defer_delete_strategy` | 低（仅线程本地队列） | 极低 | 中 | 低并发、性能敏感场景 |

---

## 测试验证

### 专用测试用例

[`concurrent_dequeue_stress`](../test/queue/queue_ut.cpp) 测试专门针对竞态条件：

```cpp
TYPED_TEST(queue_ut, concurrent_dequeue_stress) {
    const size_t ITEM_NUM = 1000;
    const size_t CONSUMER_NUM = 32;  // 大量消费者增加竞争

    // 先入队所有元素
    for (size_t i = 0; i < ITEM_NUM; i++) {
        queue.enqueue(i);
    }
    queue.close();

    // 32 个消费者竞争出队
    for (int i = 0; i < CONSUMER_NUM; i++) {
        async([&]() {
            while (queue.dequeue(out)) {
                // 消费数据
            }
        });
    }

    // 10 秒超时检测死锁/死循环
    for (auto& task : tasks) {
        ASSERT_EQ(task.wait_for(10s), std::future_status::ready);
    }
}
```

### 测试结果

| 编译器 | 结果 | 状态 |
|--------|------|------|
| GCC | 通过 (132/132) | ✓ |
| Clang | 通过 (132/132) | ✓ |

### 使用 AddressSanitizer

```bash
# 使用 ASan 构建
cmake --preset=clang-asan
cmake --build --preset=clang-asan

# 运行测试
./build/clang-asan/bin/queue_ut
```

---

## 相关概念

### Use-After-Free

一种内存安全漏洞，程序在内存被释放后仍然访问该内存。

### ABA Problem

在无锁算法中，一个位置的值从 A 变成 B 又变回 A，导致 CAS 错误地认为值未改变。

在 MS 队列中：

- 线程 A 读取 `head = 0x1000`
- 线程 B 将 `head` 改为 `0x2000`，释放 `0x1000`
- 线程 C 分配新节点，恰好获得 `0x1000`
- 线程 A 验证 `head == 0x1000`（通过！），但 `0x1000` 已经是不同的节点

**数据移动 + 双重验证解决 ABA**：

- 通过本地副本 `temp_data` 避免访问可能被释放的内存
- 双重验证确保在数据移动期间 `head` 未被修改

---

## 最佳实践

1. **使用默认策略**：`ms_queue<T>` 默认使用 Hazard Pointer，适合大多数场景
2. **及时关闭队列**：生产者完成后调用 `close()`，帮助消费者优雅退出
3. **避免频繁查询大小**：`size()` 方法是近似值，且需要遍历链表
4. **优先使用 `emplace`**：避免不必要的拷贝
5. **使用 ASan 验证**：在开发阶段使用 AddressSanitizer 检测内存问题

---

## 常见问题

### Q: 什么时候应该选择不同的策略？

A:

- 默认使用 `hazard_pointer_strategy`，这是经过验证的安全实现
- 在性能极度敏感且并发较低的场景，可以考虑 `defer_delete_strategy`

### Q: 队列有界吗？

A: `ms_queue` 是无界队列。如需有界队列，请使用 `lock_free_bounded_queue`。

### Q: 可以存储复杂类型吗？

A: 可以，只要类型满足可拷贝/可移动且默认可构造。对于复杂类型，建议使用 `emplace` 就地构造。

---

## 参考资料

1. Maged M. Michael, Michael L. Scott. "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms" (1996)
2. Maged M. Michael. "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects" (2002)
3. Anthony Williams. "C++ Concurrency in Action" (2019)
4. ["Hazard Pointers" by The CHAOS theorist](https://preshing.com/20160726/hazard-pointers/)
