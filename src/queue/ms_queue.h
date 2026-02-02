#pragma once

#include <atomic>
#include <optional>

#include "opt/cache_line.h"
#include "hp/hp.h"

namespace detail {

// 使用 Hazard Pointer + CAS 的高性能 MS queue
// 完全无锁实现，对标 libcds 架构
template <typename T>
class ms_queue {
   public:
    using value_type = T;

   private:
    static constexpr size_t kHPCount = 2;
    using hp_manager = detail::hp::hp;
    using hp_guards = typename hp_manager::template scoped_guards<kHPCount>;

    struct node {
        value_type data;
        std::atomic<node*> next{nullptr};

        node() = default;

        template <typename U>
        explicit node(U&& val) : data(std::forward<U>(val)), next(nullptr) {}
    };

    struct node_disposer {
        void operator()(void* p) const {
            delete static_cast<node*>(p);
        }
    };

   public:
    ms_queue() {
        auto* dummy = new node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
        hp_manager::construct();
    }

    ~ms_queue() {
        close();
        clear();

        node* dummy = head_.load(std::memory_order_relaxed);
        head_.store(nullptr, std::memory_order_relaxed);
        tail_.store(nullptr, std::memory_order_relaxed);
        delete dummy;
    }

    ms_queue(const ms_queue&) = delete;
    ms_queue& operator=(const ms_queue&) = delete;
    ms_queue(ms_queue&&) = delete;
    ms_queue& operator=(ms_queue&&) = delete;

    bool empty() const {
        hp_manager::guard guard;
        node* h = guard.protect(head_);
        return h->next.load(std::memory_order_acquire) == nullptr;
    }

    size_t size() const {
        size_t count = 0;
        hp_manager::guard guard;
        node* head = guard.protect(head_);
        node* tail = tail_.load(std::memory_order_relaxed);
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
        if (is_closed_.load(std::memory_order_acquire)) {
            return false;
        }

        auto* new_node = new node(val);

        hp_guards guards;

        while (true) {
            node* t = guards.template protect<node>(0, tail_);
            node* next = t->next.load(std::memory_order_acquire);

            if (tail_.load(std::memory_order_acquire) != t) {
                continue;
            }

            if (next != nullptr) {
                tail_.compare_exchange_strong(t, next, std::memory_order_release, std::memory_order_relaxed);
                continue;
            }

            node* expected = nullptr;
            if (t->next.compare_exchange_strong(expected, new_node, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }

        node* t = guards.get<node>(0);
        tail_.compare_exchange_strong(t, new_node, std::memory_order_release, std::memory_order_relaxed);

        return true;
    }

    bool enqueue(value_type&& val) {
        if (is_closed_.load(std::memory_order_acquire)) {
            return false;
        }

        auto* new_node = new node(std::move(val));

        hp_guards guards;

        while (true) {
            node* t = guards.template protect<node>(0, tail_);
            node* next = t->next.load(std::memory_order_acquire);

            if (tail_.load(std::memory_order_acquire) != t) {
                continue;
            }

            if (next != nullptr) {
                tail_.compare_exchange_strong(t, next, std::memory_order_release, std::memory_order_relaxed);
                continue;
            }

            node* expected = nullptr;
            if (t->next.compare_exchange_strong(expected, new_node, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }

        node* t = guards.get<node>(0);
        tail_.compare_exchange_strong(t, new_node, std::memory_order_release, std::memory_order_relaxed);

        return true;
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        return enqueue(value_type(std::forward<Args>(args)...));
    }

    template <typename Func>
    bool enqueue_with(Func&& f) {
        if (is_closed_.load(std::memory_order_acquire)) {
            return false;
        }

        auto* new_node = new node();
        f(new_node->data);

        hp_guards guards;

        while (true) {
            node* t = guards.template protect<node>(0, tail_);
            node* next = t->next.load(std::memory_order_acquire);

            if (tail_.load(std::memory_order_acquire) != t) {
                continue;
            }

            if (next != nullptr) {
                tail_.compare_exchange_strong(t, next, std::memory_order_release, std::memory_order_relaxed);
                continue;
            }

            node* expected = nullptr;
            if (t->next.compare_exchange_strong(expected, new_node, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }

        node* t = guards.get<node>(0);
        tail_.compare_exchange_strong(t, new_node, std::memory_order_release, std::memory_order_relaxed);

        return true;
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
        hp_guards guards;

        while (true) {
            node* h = guards.template protect<node>(0, head_);
            node* next = guards.template protect<node>(1, h->next);

            if (head_.load(std::memory_order_acquire) != h) {
                continue;
            }

            if (next == nullptr) {
                if (is_closed_.load(std::memory_order_acquire)) {
                    return false;
                }
                continue;
            }

            node* t = tail_.load(std::memory_order_acquire);
            if (h == t) {
                tail_.compare_exchange_strong(t, next, std::memory_order_release, std::memory_order_relaxed);
                continue;
            }

            if (head_.compare_exchange_strong(h, next, std::memory_order_acquire, std::memory_order_relaxed)) {
                value_type temp_data = std::move(next->data);
                std::forward<Func>(f)(temp_data);
                hp_manager::template retire<node_disposer>(h);
                return true;
            }
        }
    }

    template <typename Func>
    bool try_dequeue_with(Func&& f) {
        hp_manager::guard guard;

        node* h = guard.template protect<node>(head_);
        node* next = h->next.load(std::memory_order_acquire);

        if (next == nullptr) {
            return false;
        }

        node* t = tail_.load(std::memory_order_acquire);
        if (h == t && next != nullptr) {
            tail_.compare_exchange_strong(t, next, std::memory_order_release, std::memory_order_relaxed);
            return false;
        }

        if (head_.compare_exchange_strong(h, next, std::memory_order_acquire, std::memory_order_relaxed)) {
            value_type temp_data = std::move(next->data);
            std::forward<Func>(f)(temp_data);
            hp_manager::template retire<node_disposer>(h);
            return true;
        }
        return false;
    }

    void close() {
        is_closed_.store(true, std::memory_order_release);
    }

    bool is_closed() const {
        return is_closed_.load(std::memory_order_relaxed);
    }

    void clear() {
        value_type dummy;
        while (dequeue(dummy)) {}

        auto* new_dummy = new node();
        node* old_head = head_.load(std::memory_order_relaxed);
        node* old_tail = tail_.load(std::memory_order_relaxed);

        head_.store(new_dummy, std::memory_order_release);
        tail_.store(new_dummy, std::memory_order_release);

        hp_manager::template retire<node_disposer>(old_head);
    }

    static void initialize_hp() {
        hp_manager::construct();
    }

    static void shutdown_hp() {
        hp_manager::destruct();
    }

    static void attach_thread() {
        hp_manager::attach_thread();
    }

    static void detach_thread() {
        hp_manager::detach_thread();
    }

   private:
    std::atomic<node*> head_;
    std::atomic<node*> tail_;
    std::atomic<bool> is_closed_{false};
};

}  // namespace detail
