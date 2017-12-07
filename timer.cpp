#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

const int NUM_THREAD = 4;

using namespace std::literals::chrono_literals;

boost::asio::io_service io_service(NUM_THREAD);
std::mutex io_mutex;

template <typename Rep, typename Period, typename CallbackT>
void wait_thread(const std::chrono::duration<Rep, Period>& sleep_duration, CallbackT callback) {
    std::this_thread::sleep_for(sleep_duration);
    callback();
}

std::string async_recv_sim() {
    std::this_thread::sleep_for(1s);

    return "abc";
}

template <typename Rep, typename Period, typename CompletionToken>
void custom_async_wait(const std::chrono::duration<Rep, Period>& sleep_duration, CompletionToken&& token) {
    using handler_type = typename boost::asio::handler_type<CompletionToken, void()>::type;
    handler_type handler(std::forward<decltype(token)>(token));
    boost::asio::async_result<handler_type> result(handler);
    std::thread thr([sleep_duration, handler](){wait_thread(sleep_duration, handler);});
    thr.detach();
    result.get();
}

void async_wait_task(int task_id, boost::asio::yield_context yield) {
    boost::asio::steady_timer timer(io_service);
    timer.expires_from_now(std::chrono::seconds(1));

    std::cout << task_id << ": start wait" << std::endl;
    timer.async_wait(yield);
    std::cout << task_id << ": end wait" << std::endl;
}

void custom_async_wait_task(int task_id, boost::asio::yield_context yield) {
    std::cout << task_id << ": start wait" << std::endl;
    custom_async_wait(1s, yield);
    std::cout << task_id << ": end wait" << std::endl;
}

void cpu_task_with_async_wait(int task_id, boost::asio::yield_context yield) {
    boost::asio::steady_timer timer(io_service);
    timer.expires_from_now(std::chrono::seconds(1));

    std::unique_lock<std::mutex> lk(io_mutex);
    std::cout << task_id << ": start wait" << std::endl;
    lk.unlock();
    timer.async_wait(yield);
    lk.lock();
    std::cout << task_id << ": end wait" << std::endl;
    lk.unlock();

    lk.lock();
    std::cout << task_id << ": start \"using CPU\"" << std::endl;
    lk.unlock();
    std::this_thread::sleep_for(1s);
    lk.lock();
    std::cout << task_id << ": end \"using CPU\"" << std::endl;
    lk.unlock();
}

int main() {
    for (int i = 0; i < 5; i++) {
        boost::asio::spawn(io_service, std::bind(cpu_task_with_async_wait, i, std::placeholders::_1));
    }

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREAD);

    for (int i = 0; i < NUM_THREAD; i++) {
        threads.emplace_back([](){io_service.run();});
    }
    for (int i = 0; i < NUM_THREAD; i++) {
        threads[i].join();
    }
}

