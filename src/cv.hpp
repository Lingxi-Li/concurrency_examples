#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace cv {

std::mutex mut;
std::condition_variable cond_var;
std::queue<unsigned> que;

std::atomic_uint counter(0);

void produce(const std::future<void>& start, unsigned loopcnt) {
  start.wait();
  while (loopcnt--) {
    {
      auto v = counter.fetch_add(1, std::memory_order_relaxed);
      std::lock_guard<std::mutex> lock(mut);
      que.push(v);
    }
    cond_var.notify_one();
  }
}

void consume(const std::future<void>& start) {
  start.wait();
  while (true) {
    std::unique_lock<std::mutex> lock(mut);
    cond_var.wait(lock, [] { return !que.empty(); });
    auto v = que.front();
    if (v == unsigned(-1)) return;
    que.pop();
    lock.unlock();
    counter.fetch_sub(1, std::memory_order_relaxed);
  }
}

void main() {
  using namespace std::chrono;
  constexpr auto loopcnt = 1000000;
  std::promise<void> start_promise;
  auto start_future = start_promise.get_future();
  auto producer_1 = std::async(std::launch::async, produce, std::ref(start_future), loopcnt);
  auto producer_2 = std::async(std::launch::async, produce, std::ref(start_future), loopcnt);
  auto consumer_1 = std::async(std::launch::async, consume, std::ref(start_future));
  auto consumer_2 = std::async(std::launch::async, consume, std::ref(start_future));
  std::this_thread::sleep_for(2s);
  start_promise.set_value();
  producer_1.wait();
  producer_2.wait();
  {
    std::lock_guard<std::mutex> lock(mut);
    que.push(unsigned(-1));
  }
  cond_var.notify_all();
  consumer_1.wait();
  consumer_2.wait();
  assert(counter.load(std::memory_order_relaxed) == 0);
}

} // namespace cv
