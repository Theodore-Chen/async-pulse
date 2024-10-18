#include <iostream>
#include <future>
#include <thread>
#include <vector>

void work(std::shared_future<unsigned>&& sf, int input) {
    printf("work[%d] started\r\n", input);
    sf.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("work[%d] finished, value = %u\r\n", input, sf.get());
}

void notify(std::promise<unsigned>& p, unsigned cnt) {
    std::cout << "notifier begin" << std::endl;
    p.set_value(cnt);
    std::cout << "notifier finish" << std::endl;
}

void shared_future_test() {
    std::cout << "### shared_future_test begin ###" << std::endl;
    std::promise<unsigned> p;
    std::shared_future<unsigned> sf(p.get_future().share());
    notify(p, unsigned(10));
    std::thread t(work, std::move(sf), unsigned(1));
    t.join();
    std::cout << "### shared_future_test end ###" << std::endl;
}
