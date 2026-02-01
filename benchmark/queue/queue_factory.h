#pragma once

#include <memory>
#include <type_traits>

template <typename QueueType, size_t Capacity = 2048>
struct queue_factory {
    static constexpr bool is_constructible_with_capacity =
        std::is_constructible_v<QueueType, size_t>;

    static std::unique_ptr<QueueType> create() {
        if constexpr (is_constructible_with_capacity) {
            return std::make_unique<QueueType>(Capacity);
        } else {
            return std::make_unique<QueueType>();
        }
    }

    static constexpr size_t capacity = Capacity;
};
