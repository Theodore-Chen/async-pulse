#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

std::condition_variable cv;
std::mutex cv_m;
bool flag(false);

void work() {
    std::cout << "worker started" << std::endl;
    std::unique_lock<std::mutex> lk(cv_m);
    cv.wait(lk, [] { return flag; });
    std::cout << "worker finished" << std::endl;
}

void notify() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "notifier begin" << std::endl;
    {
        std::lock_guard<std::mutex> lk(cv_m);
        flag = true;
    }
    cv.notify_all();
    std::cout << "notifier finish" << std::endl;
}

void cv_test() {
    std::cout << "### cv_test begin ###" << std::endl;
    auto fu = std::async(std::launch::async, work);
    notify();
    fu.wait();
    std::cout << "### cv_test end ###" << std::endl;
}
