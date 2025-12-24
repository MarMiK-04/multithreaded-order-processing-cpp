#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <chrono>

struct Order {
    int order_id;
    int quantity;
    double price;
};

class OrderQueue {
private:
    std::queue<Order> q;
    std::mutex m;
    std::condition_variable cv;
    bool done = false;

public:
    void push(const Order& order) {
        {
            std::lock_guard<std::mutex> lock(m);
            q.push(order);
        }
        cv.notify_one();
    }

    bool pop(Order& order) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&]() { return !q.empty() || done; });

        if (q.empty())
            return false;

        order = q.front();
        q.pop();
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(m);
            done = true;
        }
        cv.notify_all();
    }
};

void orderProducer(OrderQueue& queue, int total_orders) {
    for (int i = 0; i < total_orders; i++) {
        queue.push({i, (i % 10) + 1, 100.0 + i});
    }
    queue.shutdown();
}

void orderConsumer(OrderQueue& queue, std::atomic<int>& processed) {
    Order order;
    while (queue.pop(order)) {
        processed.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    const int TOTAL_ORDERS = 10000;
    const int WORKER_THREADS = 4;

    OrderQueue queue;
    std::atomic<int> processed{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer(orderProducer, std::ref(queue), TOTAL_ORDERS);

    std::vector<std::thread> workers;
    for (int i = 0; i < WORKER_THREADS; i++) {
        workers.emplace_back(orderConsumer,
                             std::ref(queue),
                             std::ref(processed));
    }

    producer.join();
    for (auto& t : workers)
        t.join();

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Processed Orders: " << processed.load() << "\n";
    std::cout << "Time Taken: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms\n";
}
