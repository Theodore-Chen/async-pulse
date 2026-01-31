#include <type_traits>
#include <memory>

template <typename T, typename = void>
struct has_capacity : std::false_type {};

template <typename T>
struct has_capacity<T, std::void_t<decltype(std::declval<T>().capacity())>> : std::true_type {};

template <typename T>
inline constexpr bool has_capacity_v = has_capacity<T>::value;

template <typename QueueType, size_t Capacity = 2048, typename Enable = void>
struct queue_factory;

template <typename QueueType, size_t Capacity>
struct queue_factory<QueueType, Capacity, std::enable_if_t<has_capacity_v<QueueType>>> {
    static constexpr size_t capacity_ = Capacity;
    static std::unique_ptr<QueueType> create() {
        return std::make_unique<QueueType>(capacity_);
    }
};

template <typename QueueType, size_t Capacity>
struct queue_factory<QueueType, Capacity, std::enable_if_t<!has_capacity_v<QueueType>>> {
    static constexpr size_t capacity_ = Capacity;
    static std::unique_ptr<QueueType> create() {
        return std::make_unique<QueueType>();
    }
};
