#include <iostream>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>

int main () {
    std::thread t1(producer);
    std::thread t2(consumer);

    t1.join
}