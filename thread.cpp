#include <iostream>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
const int maxx = 10;

class Queue {
public:
    void put (int val) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() -> bool { return q.size() <= maxx; });
        q.push(val);
        std::cout << "producer: " << val << std::endl;
        cv.notify_all();
    }

    int get () {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() -> bool { return !q.empty(); });
        int val = q.front();
        q.pop();
        std::cout << "consumer: " << val << std::endl; 
        cv.notify_all();
        return val;
    }
private:
    std::queue <int> q;
    std::mutex mtx;
    std::condition_variable cv;

};
void producer (Queue *q) {
    for (int i = 1; i <= 100; i++) {
        q->put(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void consumer (Queue *q) {
    for (int i = 1; i <= 100; i++) {
        q->get();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
int main () {
    Queue q;
    std::thread t1(producer, &q);
    std::thread t2(consumer, &q);

    t1.join();
    t2.join();

}