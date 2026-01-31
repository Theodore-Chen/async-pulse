#pragma once

#include <atomic>
#include <optional>
#include <memory>
#include <mutex>

#include "opt/cache_line.h"

namespace detail {

// 默认的节点删除器
template <typename Node>
struct default_node_disposer {
    void operator()(Node* p) const {
        delete p;
    }
};

}  // namespace detail

// 使用 mutex + shared_ptr 的简化版 MS queue
// 不使用 Hazard Pointer，改用更简单的同步机制
template <typename T>
class ms_queue {
   public:
    using value_type = T;

   private:
    struct node {
        value_type data;
        std::shared_ptr<node> next;
        mutable std::mutex next_mutex;  // 保护 next 指针的互斥锁

        node() : next(nullptr) {}

        template <typename U>
        explicit node(U&& val) : data(std::forward<U>(val)), next(nullptr) {}
    };

    using node_ptr = std::shared_ptr<node>;

   public:
    ms_queue() : head_(std::make_shared<node>()), tail_(head_) {}

    ~ms_queue() {
        close();
        clear();
    }

    ms_queue(const ms_queue&) = delete;
    ms_queue& operator=(const ms_queue&) = delete;
    ms_queue(ms_queue&&) = delete;
    ms_queue& operator=(ms_queue&&) = delete;

    bool empty() {
        node_ptr head = std::atomic_load(&head_);
        node_ptr tail = std::atomic_load(&tail_);
        return (head == tail) && (head->next == nullptr);
    }

    size_t size() {
        size_t count = 0;
        node_ptr head = std::atomic_load(&head_);
        node_ptr tail = std::atomic_load(&tail_);
        node_ptr curr = head->next;

        while (curr != nullptr && head != tail) {
            count++;
            if (curr == tail) {
                break;
            }
            curr = curr->next;
        }
        return count;
    }

    bool enqueue(const value_type& val) {
        if (is_closed_.load()) {
            return false;
        }

        node_ptr new_node = std::make_shared<node>(val);

        while (true) {
            node_ptr tail = std::atomic_load(&tail_);
            node_ptr next;

            // 使用 mutex 保护 tail->next 的读取
            {
                std::lock_guard<std::mutex> lock(tail->next_mutex);
                next = tail->next;
                if (next == nullptr) {
                    tail->next = new_node;
                    // 尝试推进 tail
                    std::atomic_compare_exchange_strong(&tail_, &tail, new_node);
                    return true;
                }
            }

            // tail 落后了，帮助推进
            std::atomic_compare_exchange_strong(&tail_, &tail, next);
        }
    }

    bool enqueue(value_type&& val) {
        if (is_closed_.load()) {
            return false;
        }

        node_ptr new_node = std::make_shared<node>(std::move(val));

        while (true) {
            node_ptr tail = std::atomic_load(&tail_);
            node_ptr next;

            {
                std::lock_guard<std::mutex> lock(tail->next_mutex);
                next = tail->next;
                if (next == nullptr) {
                    tail->next = new_node;
                    std::atomic_compare_exchange_strong(&tail_, &tail, new_node);
                    return true;
                }
            }

            std::atomic_compare_exchange_strong(&tail_, &tail, next);
        }
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        return enqueue(value_type(std::forward<Args>(args)...));
    }

    template <typename Func>
    bool enqueue_with(Func&& f) {
        if (is_closed_.load()) {
            return false;
        }

        node_ptr new_node = std::make_shared<node>();
        f(new_node->data);

        while (true) {
            node_ptr tail = std::atomic_load(&tail_);
            node_ptr next;

            {
                std::lock_guard<std::mutex> lock(tail->next_mutex);
                next = tail->next;
                if (next == nullptr) {
                    tail->next = new_node;
                    std::atomic_compare_exchange_strong(&tail_, &tail, new_node);
                    return true;
                }
            }

            std::atomic_compare_exchange_strong(&tail_, &tail, next);
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
        while (true) {
            node_ptr head = std::atomic_load(&head_);
            node_ptr tail = std::atomic_load(&tail_);

            // 读取 head->next
            node_ptr next;
            {
                std::lock_guard<std::mutex> lock(head->next_mutex);
                next = head->next;
            }

            if (head == tail) {
                if (next == nullptr) {
                    if (is_closed_.load()) {
                        return false;
                    }
                    continue;
                }
                // 帮助推进 tail
                std::atomic_compare_exchange_strong(&tail_, &tail, next);
                continue;
            }

            // 尝试推进 head
            if (std::atomic_compare_exchange_strong(&head_, &head, next)) {
                value_type temp_data = std::move(next->data);
                std::forward<Func>(f)(temp_data);
                return true;
            }
        }
    }

    template <typename Func>
    bool try_dequeue_with(Func&& f) {
        return dequeue_with(std::forward<Func>(f));
    }

    void close() {
        is_closed_.store(true);
    }

    bool is_closed() {
        return is_closed_.load();
    }

    void clear() {
        head_ = std::make_shared<node>();
        tail_ = head_;
    }

   private:
    node_ptr head_;  // 使用 shared_ptr，通过 atomic_xxx 函数进行原子操作
    node_ptr tail_;
    std::atomic<bool> is_closed_{false};
};
