
#include "thread_pool.h"
// using namespace std::chrono_literals;

struct MyData {
    uint32_t id = 0;
    uint32_t outPut = 0;
};

void ThreadPoolTest() {
    std::function<void(MyData&)> calcMyData = [](MyData& data) {
        volatile uint32_t a = 0;
        for (size_t i = 0; i < 1000000; i++) {
            a += 1;
        }
        data.outPut = data.id * 2;
    };
    const size_t TEST_CNT = 1000;

    std::cout << "Thread Poll Test Gegin" << std::endl;
    auto begin = std::chrono::system_clock::now();

    ThreadPool<MyData> threadPool(calcMyData);
    std::vector<std::future<MyData>> handles;
    for (size_t i = 0; i < TEST_CNT; i++) {
        MyData data;
        data.id = i;
        handles.push_back(std::move(threadPool.Push(data)));
    }

    std::vector<uint32_t> result;
    for (auto&& handle : handles) {
        if (handle.valid()) {
            handle.wait();
            MyData data = handle.get();
            if (data.outPut == data.id * 2) {
                result.push_back(data.outPut);
            }
        }
    }

    auto end = std::chrono::system_clock::now();
    int64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    std::cout << "Thread Poll Test Finish, result size = " << result.size() << ", time = " << duration << "ms"
              << std::endl;
}
