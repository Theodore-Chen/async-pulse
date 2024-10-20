#include <atomic>
#include <iostream>

template <typename T>
class LockFreeQueue {
   private:
    struct Node {
        T data;
        std::atomic<Node*> next;
        Node(T const& data) : data(data), next(nullptr) {}
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

   public:
    LockFreeQueue() : head(new Node(T())), tail(head.load(std::memory_order_relaxed)) {}

    ~LockFreeQueue() {
        while (Node* const old_head = head.load(std::memory_order_relaxed)) {
            head.store(old_head->next, std::memory_order_relaxed);
            delete old_head;
        }
    }

    LockFreeQueue(const LockFreeQueue& other) = delete;
    LockFreeQueue& operator=(const LockFreeQueue& other) = delete;

    void enqueue(T const& data) {
        Node* new_node = new Node(data);
        Node* old_tail = tail.load();
        while (!tail.compare_exchange_weak(old_tail, new_node, std::memory_order_release, std::memory_order_relaxed)) {
            // Tail was moved, retry with the new tail
        }
        old_tail->next.store(new_node, std::memory_order_release);
    }

    bool dequeue(T& result) {
        Node* old_head = head.load(std::memory_order_relaxed);
        while (old_head != nullptr && !head.compare_exchange_weak(old_head, old_head->next, std::memory_order_release,
                                                                  std::memory_order_relaxed)) {
            // Head was moved, retry with the new head
        }
        if (old_head == nullptr) {
            return false;  // Queue is empty
        }
        result = old_head->data;
        Node* next = old_head->next.load(std::memory_order_relaxed);
        delete old_head;
        tail.compare_exchange_strong(next, next, std::memory_order_release, std::memory_order_relaxed);
        return true;
    }
};

int main() {
    LockFreeQueue<int> queue;

    queue.enqueue(1);
    queue.enqueue(2);
    queue.enqueue(3);

    int value;
    while (queue.dequeue(value)) {
        std::cout << value << std::endl;
    }

    return 0;
}