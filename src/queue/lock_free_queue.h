#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

template <typename T, size_t N>
class LockFreeQueue {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
    static_assert(N <= (1 << 30), "N too large for 32-bit index");

    // 64位版本化索引（高32位版本号，低32位索引）
    using VersionedIndex = uint64_t;
    static constexpr uint32_t INDEX_MASK = N - 1;

    struct Slot {
        std::atomic<uint32_t> version{0};  // 偶数表示可写，奇数表示已提交
        T data;
    };

    std::vector<Slot> slots{N};
    std::atomic<VersionedIndex> write_index_{0};
    std::atomic<VersionedIndex> read_index_{0};

    // 从64位值解码版本号和索引
    static uint32_t get_version(VersionedIndex val) { return val >> 32; }
    static uint32_t get_index(VersionedIndex val) { return val & 0xFFFFFFFF; }

    // 编码版本号和索引为64位值
    static VersionedIndex make_index(uint32_t version, uint32_t index) {
        return (static_cast<uint64_t>(version) << 32) | index;
    }

    template <typename CheckCondition, typename SlotCheck>
    bool access_slot(std::atomic<VersionedIndex>& current_idx,
                     std::atomic<VersionedIndex>& other_idx,
                     CheckCondition check_cond,
                     SlotCheck slot_check,
                     uint32_t& slot_idx) {
        VersionedIndex current;
        VersionedIndex other;
        uint32_t current_version, current_index;
        uint32_t new_version, new_index;

        do {
            current = current_idx.load(std::memory_order_relaxed);
            other = other_idx.load(std::memory_order_acquire);

            current_version = get_version(current);
            current_index = get_index(current);
            const uint32_t other_index = get_index(other);

            // 检查队列状态
            if (check_cond(current_index, other_index)) {
                return false;
            }

            slot_idx = current_index & INDEX_MASK;

            // 检查槽位状态
            const uint32_t expected_version = current_version % 2 == 0 ? current_version : current_version - 1;
            uint32_t slot_version = slots[slot_idx].version.load(std::memory_order_acquire);

            if (!slot_check(slot_version, expected_version)) {
                continue;
            }

            // 计算新索引
            new_index = (current_index + 1) % (N * 2);  // 允许虚拟索引扩展
            new_version = current_version;

            // 处理索引回绕
            if (new_index < current_index) {
                new_version += 2;  // 保持版本号奇偶性不变
            }
        } while (!current_idx.compare_exchange_weak(current, make_index(new_version, new_index),
                                                    std::memory_order_acq_rel, std::memory_order_relaxed));

        return true;
    }

   public:
    template <typename Arg>
    bool enqueue(Arg&& value) {
        uint32_t slot_idx;
        // 队列满检查
        auto is_full = [](uint32_t w, uint32_t r) { return (w - r) >= N; };
        // 期望偶数版本（可写）
        auto is_even = [](uint32_t slot_ver, uint32_t expected_ver) { return slot_ver == expected_ver; };
        if (!access_slot(write_index_, read_index_, is_full, is_even, slot_idx)) {
            return false;
        }

        // 写入数据并更新版本号（偶数->奇数）
        slots[slot_idx].data = std::forward<Arg>(value);
        slots[slot_idx].version.store(get_version(write_index_.load()) | 1, std::memory_order_release);
        return true;
    }

    bool dequeue(T& value) {
        uint32_t slot_idx;
        auto is_empty = [](uint32_t r, uint32_t w) { return r >= w; };
        auto is_odd = [](uint32_t slot_ver, uint32_t expected_ver) { return slot_ver == (expected_ver | 1); };
        if (!access_slot(read_index_, write_index_, is_empty, is_odd, slot_idx)) {
            return false;
        }

        // 读取数据并重置版本号（奇数->偶数）
        value = std::move(slots[slot_idx].data);
        slots[slot_idx].version.store(get_version(read_index_.load()) + 1, std::memory_order_release);
        return true;
    }

    size_t size() const noexcept {
        const uint32_t w = get_index(write_index_.load());
        const uint32_t r = get_index(read_index_.load());
        return (w >= r) ? (w - r) : (N * 2 - r + w);
    }

    bool empty() const { return get_index(read_index_.load()) >= get_index(write_index_.load()); }
};