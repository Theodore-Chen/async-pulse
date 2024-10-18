#include <iostream>
#include <future>
#include <thread>

std::promise<void> p;

void work() {
    std::cout << "worker started" << std::endl;
    p.get_future().wait();
    std::cout << "worker finished" << std::endl;
}

void notify() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "notifier begin" << std::endl;
    p.set_value();
    std::cout << "notifier finish" << std::endl;
}

void promise_test() {
    std::cout << "### promise_test begin ###" << std::endl;
    std::thread workThread(work);
    notify();
    workThread.join();
    std::cout << "### promise_test end ###" << std::endl;
}
