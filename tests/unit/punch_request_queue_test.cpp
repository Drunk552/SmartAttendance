#include "business/punch_request_queue.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

using smart_attendance::business::PendingPunch;
using smart_attendance::business::PunchRequestQueue;

PendingPunch makePending(int userId) {
    return {{userId, 1000, 8 * 60, {}}, "User"};
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testFullQueueUnblocksAfterPop() {
    PunchRequestQueue queue(1);
    std::atomic<bool> producerStop{false};
    std::atomic<bool> consumerStop{false};
    require(queue.push(makePending(1), producerStop),
            "first request should fill the queue");

    std::atomic<bool> secondPushed{false};
    std::thread producer([&] {
        secondPushed.store(queue.push(makePending(2), producerStop));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    require(!secondPushed.load(), "producer should wait while queue is full");
    const auto first = queue.waitPop(consumerStop);
    require(first && first->request.userId == 1,
            "consumer should receive the oldest request");
    producer.join();
    require(secondPushed.load() && queue.size() == 1,
            "freeing one slot should wake the producer");
}

void testStoppedProducerIsWokenWithoutEnqueue() {
    PunchRequestQueue queue(1);
    std::atomic<bool> producerStop{false};
    require(queue.push(makePending(1), producerStop),
            "setup request should fill the queue");

    std::atomic<bool> pushResult{true};
    std::thread producer([&] {
        pushResult.store(queue.push(makePending(2), producerStop));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    producerStop.store(true);
    queue.wakeAll();
    producer.join();

    require(!pushResult.load() && queue.size() == 1,
            "stopped producer must not add a new punch");
}

void testConsumerDrainsBeforeStopping() {
    PunchRequestQueue queue(2);
    std::atomic<bool> producerStop{false};
    std::atomic<bool> consumerStop{true};
    require(queue.push(makePending(1), producerStop) &&
                queue.push(makePending(2), producerStop),
            "setup should enqueue both requests");

    const auto first = queue.waitPop(consumerStop);
    const auto second = queue.waitPop(consumerStop);
    const auto finished = queue.waitPop(consumerStop);
    require(first && second && !finished,
            "consumer should drain queued punches before stopping");
}

} // namespace

int main() {
    testFullQueueUnblocksAfterPop();
    testStoppedProducerIsWokenWithoutEnqueue();
    testConsumerDrainsBeforeStopping();
    std::cout << "punch_request_queue_test: PASS\n";
    return 0;
}
