#pragma once

#include <cstddef>

constexpr size_t QUEUE_CAPACITY = 1024;

template <typename Queue>
void fill_queue_to_percentage(Queue& queue, double percentage) {
    size_t capacity = QUEUE_CAPACITY;
    size_t target_count = static_cast<size_t>(capacity * percentage);

    for (size_t i = 0; i < target_count; ++i) {
        queue.enqueue(static_cast<int>(i));
    }
}
