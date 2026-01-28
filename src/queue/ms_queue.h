#pragma once

#include <atomic>
#include <optional>

#include "opt/cache_line.h"
#include "hp/hp.h"

namespace detail {

// 默认的节点删除器
template <typename Node>
struct default_node_disposer {
    void operator()(Node* p) const {
        delete p;
    }
};

}  // namespace detail

template <typename T, typename HP = detail::hp::hp>
class ms_queue {
   public:
    using value_type = T;
    using hp_type = HP;

   private:
    static constexpr size_t HAZARDS_NEEDED = 2;
    using guard_array = typename HP::template guard_array<HAZARDS_NEEDED>;

    struct node {
        value_type data;
        std::atomic<node*> next;

        node() : next(nullptr) {}

        template <typename U>
        explicit node(U&& val) : data(std::forward<U>(val)), next(nullptr) {}
    };

    using node_disposer = detail::default_node_disposer<node>;

   public:
    ms_queue() = default;

    ~ms_queue() {
        close();
        clear();
    }

    ms_queue(const ms_queue&) = delete;
    ms_queue& operator=(const ms_queue&) = delete;
    ms_queue(ms_queue&&) = delete;
    ms_queue& operator=(ms_queue&&) = delete;

    bool empty() {
        node* head = head_.load(std::memory_order_acquire);
        node* tail = tail_.load(std::memory_order_acquire);
        node* next = head->next.load(std::memory_order_acquire);
        return (head == tail) && (next == nullptr);
    }

    size_t size() {
        size_t count = 0;
        node* head = head_.load(std::memory_order_acquire);
        node* tail = tail_.load(std::memory_order_acquire);
        node* curr = head->next.load(std::memory_order_acquire);

        while (curr != nullptr && head != tail) {
            count++;
            if (curr == tail) {
                break;
            }
            curr = curr->next.load(std::memory_order_acquire);
        }
        return count;
    }

    bool enqueue(const value_type& val) {
        return enqueue_with([&val](value_type& dest) { dest = val; });
    }

    bool enqueue(value_type&& val) {
        return enqueue_with([&val](value_type& dest) { dest = std::move(val); });
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        return enqueue_with([&args...](value_type& dest) { dest = value_type(std::forward<Args>(args)...); });
    }

    template <typename Func>
    bool enqueue_with(Func&& f) {
        if (is_closed_.load(std::memory_order_acquire)) {
            return false;
        }

        node* new_node = new node();
        f(new_node->data);

        while (true) {
            node* tail = tail_.load(std::memory_order_acquire);
            node* next = tail->next.load(std::memory_order_acquire);

            if (tail != tail_.load(std::memory_order_acquire)) {
                continue;
            }

            if (next == nullptr) {
                if (tail->next.compare_exchange_weak(next, new_node, std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                    tail_.compare_exchange_strong(tail, new_node, std::memory_order_acq_rel, std::memory_order_acquire);
                    return true;
                }
            } else {
                tail_.compare_exchange_strong(tail, next, std::memory_order_acq_rel, std::memory_order_acquire);
            }
        }
    }

    template <typename Func>
    bool try_enqueue_with(Func&& f) {
        return enqueue_with(std::forward<Func>(f));
    }

    bool dequeue(value_type& val) {
        return dequeue_with([&val](value_type& item) { val = std::move(item); });
    }

    std::optional<value_type> dequeue() {
        std::optional<value_type> result;
        auto f = [&result](value_type& val) { result.emplace(std::move(val)); };
        return dequeue_with(f) ? std::move(result) : std::nullopt;
    }

    template <typename Func>
    bool dequeue_with(Func&& f) {
        return dequeue_impl<true>(std::forward<Func>(f));
    }

    template <typename Func>
    bool try_dequeue_with(Func&& f) {
        return dequeue_impl<false>(std::forward<Func>(f));
    }

    void close() {
        is_closed_.store(true, std::memory_order_release);
    }

    bool is_closed() {
        return is_closed_.load(std::memory_order_relaxed);
    }

    void clear() {
        // 清空队列，遍历删除所有节点
        node* head = head_.load(std::memory_order_acquire);
        while (head != nullptr) {
            node* next = head->next.load(std::memory_order_acquire);
            delete head;
            head = next;
        }
        head_.store(nullptr, std::memory_order_release);
        tail_.store(nullptr, std::memory_order_release);
    }

    private:
    template <bool Blocking, typename Func>
    bool dequeue_impl(Func&& f) {
        guard_array guards;

        while (true) {
            node* head = guards.protect(0, head_, [](node* p) { return p; });

            if (!head) {
                return false;
            }

            node* tail = tail_.load(std::memory_order_acquire);
            node* next = guards.protect(1, head->next, [](node* p) { return p; });

            if (head != head_.load(std::memory_order_acquire)) {
                continue;
            }

            if (head == tail) {
                if (next == nullptr) {
                    if constexpr (!Blocking) {
                        return false;
                    }
                    if (is_closed_.load(std::memory_order_acquire)) {
                        return false;
                    }
                    continue;
                }
                tail_.compare_exchange_strong(tail, next, std::memory_order_acq_rel,
                                             std::memory_order_acquire);
                continue;
            }

            if (head_.compare_exchange_weak(head, next, std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
                value_type temp_data = std::move(next->data);
                std::forward<Func>(f)(temp_data);

                HP::template retire<node_disposer>(head);
                return true;
            }
        }
    }

   private:
    alignas(CACHE_LINE_SIZE) std::atomic<node*> head_{new node()};
    alignas(CACHE_LINE_SIZE) std::atomic<node*> tail_{head_.load(std::memory_order_relaxed)};
    alignas(CACHE_LINE_SIZE) std::atomic<bool> is_closed_{false};
};
