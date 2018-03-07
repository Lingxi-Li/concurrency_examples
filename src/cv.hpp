#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace cv {

using namespace std::literals;

std::mutex mut;
std::condition_variable cond_var;
std::queue<unsigned> que;

void produce(const std::future<void>& start, unsigned loopcnt) {
  start.wait();
  while (loopcnt--) {
    std::this_thread::sleep_for(50ms);
    auto item = 1;
    {
      std::lock_guard<std::mutex> lock(mut);
      que.push(item);
    }
    cond_var.notify_one();
  }
}

void consume(const std::future<void>& start) {
  start.wait();
  while (true) {
    std::unique_lock<std::mutex> lock(mut);
    cond_var.wait(lock, [] { return !que.empty(); });
    auto item = que.front();
    if (item == unsigned(-1)) return;
    que.pop();
    lock.unlock();
    std::this_thread::sleep_for(50ms);
  }
}

void main() {
  constexpr auto loopcnt = 100;
  std::promise<void> start_promise;
  auto start_future = start_promise.get_future();
  std::thread producer_1(produce, std::ref(start_future), loopcnt);
  std::thread producer_2(produce, std::ref(start_future), loopcnt);
  std::thread consumer_1(consume, std::ref(start_future));
  std::thread consumer_2(consume, std::ref(start_future));
  std::this_thread::sleep_for(1s);
  start_promise.set_value();
  producer_1.join();
  producer_2.join();
  {
    std::lock_guard<std::mutex> lock(mut);
    que.push(unsigned(-1));
  }
  cond_var.notify_all();
  consumer_1.join();
  consumer_2.join();
  assert(que.size() == 1 && que.front() == unsigned(-1));
}

} // namespace cv
