#pragma once

#include <future>
#include <vector>
#include "thread_sync.h"

template <typename Queue>
std::vector<std::future<void>> create_consumers(Queue& queue, size_t count, start_sync& sync) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(count);

    auto task = [&queue, &sync]() {
        sync.wait();
        typename Queue::value_type value;
        while (queue.dequeue(value)) {
        }
    };

    for (size_t i = 0; i < count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, task));
    }

    return tasks;
}

template <typename Queue>
std::vector<std::future<void>> create_producers(Queue& queue,
                                                size_t count,
                                                size_t items_per_producer,
                                                start_sync& sync) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(count);

    auto task = [&queue, items_per_producer, &sync](size_t producer_id) {
        sync.wait();
        using element_type = typename Queue::value_type;
        for (size_t i = 0; i < items_per_producer; i++) {
            queue.enqueue(static_cast<element_type>(producer_id));
        }
    };

    for (size_t i = 0; i < count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, task, i));
    }

    return tasks;
}
