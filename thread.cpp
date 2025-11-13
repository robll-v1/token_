#include <iostream>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>

class Queue {
public:
    void put (int val) {
        std::unique_lock<std::mutex> lock(mtx);
        q.push(val);
        std::cout << "producer: " << val << std::endl;
    }

    int get () {
        std::unique_lock<std::mutex> lock(mtx);
        int val = q.front();
        q.pop();
        std::cout << "consumer: " << val << std::endl; 
        return val;
    }
private:
    std::queue <int> q;
    std::mutex mtx;
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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
int main () {
    Queue q;
    std::thread t1(producer, &q);
    std::thread t2(consumer, &q);
    
    t1.join();
    t2.join();

}