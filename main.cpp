/**
 * @file main.cpp
 * @brief Token管理系统演示程序
 * 
 * 该程序演示了生产者-消费者模式下的Token管理系统：
 * - TokenProducer: 定期生产token并添加到TokenManager
 * - TokenCustomer: 从TokenManager消费指定数量的token
 * - TokenManager: 管理token的存储和线程安全的访问
 */

#include "token_manager.h"
#include "token_customer.h"
#include "token_producer.h"
#include <vector>
#include <memory>
#include <chrono>
#include <iostream>
#include <thread>

/**
 * @brief 主函数 - Token管理系统演示
 * 
 * 程序流程：
 * 1. 创建TokenManager，设置最大token数量
 * 2. 创建并启动生产者线程
 * 3. 创建并启动多个消费者线程
 * 4. 等待一段时间让系统运行
 * 5. 停止所有消费者和生产者
 * 6. 输出统计信息
 */
int main () {
    std::cout << "token manager show case" << std::endl;

    // 初始化Token管理器，设置最大token数量为10
    const size_t max_tokens = 10;
    auto token_manager = std::make_shared<TokenManager>(max_tokens);
    std::cout << "initialize the manager, the max_tokens is :" << max_tokens << std::endl;

    // 创建生产者线程
    const size_t produced_count = 1;
    std::vector<std::unique_ptr<TokenProducer>> producers;
    for (size_t i = 0; i < produced_count; i++) {
        auto producer = std::make_unique<TokenProducer>(token_manager);
        producer->start();  // 启动生产者线程，每500ms生产一个token
        producers.emplace_back(std::move(producer));
        std::cout << "active the token manager" << std::endl;
    }

    // 创建消费者线程
    const size_t cons_count = 5;      // 消费者数量
    const size_t cons_per = 3;        // 每个消费者每次消费的token数量

    std::cout << "initial the consumer, per 3: " << std::endl;
    std::vector<std::unique_ptr<TokenCustomer>> consumers;
    auto start_time = std::chrono::steady_clock::now();  // 记录开始时间

    // 创建并启动所有消费者线程
    for (size_t i = 0; i < cons_count; i++) {
        // 创建消费者，并设置回调函数用于输出消费成功的信息
        auto consumer = std::make_unique<TokenCustomer>(token_manager, cons_per, [i](bool success) {
            std::cout << "consumer " << i + 1 << " success consume: " << cons_per << " tokens" << std::endl;
        });
        consumer->start();  // 启动消费者线程
        consumers.emplace_back(std::move(consumer));
        std::cout << i + 1 << std::endl;
    }

    // 等待一段时间让消费者和生产者运行
    // 这样可以让系统有时间产生和消费token，观察实际运行效果
    std::cout << "Waiting for consumers and producers to run..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 停止所有消费者线程
    for (const auto& consumer : consumers) {
        consumer->stop();
    }
    
    // 计算并输出运行时间
    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    std::cout << "info" << std::endl;
    std::cout << "total time: " << total_time << std::endl;
    
    // 输出每个消费者的信息
    for (size_t i = 0; i < cons_count; i++) {
        std::cout << "consumer[" << i + 1 << "] " << std::endl;
    }
    
    // 输出剩余token数量
    std::cout << "last tokens: " << token_manager->GetTokens() << std::endl;
    
    // 停止所有生产者线程
    for (auto& producer : producers) {
        producer->stop();
    }
    std::cout << "case finish" << std::endl;
}