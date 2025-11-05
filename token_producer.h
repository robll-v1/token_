/**
 * @file token_producer.h
 * @brief Token生产者类 - 定期向TokenManager添加token的线程
 * 
 * TokenProducer在独立线程中运行，定期（每500ms）向TokenManager添加一个token。
 * 支持优雅停止，可以通过stop()方法停止生产。
 */

#pragma once

#include "token_manager.h"
#include <atomic>
#include <thread>

/**
 * @class TokenProducer
 * @brief Token生产者类
 * 
 * 在独立线程中运行，定期向TokenManager添加token。
 * 默认每500ms添加一个token，直到达到最大数量或调用stop()。
 */
class TokenProducer
{
private:
    std::shared_ptr<TokenManager> token_manager_;  // 共享的TokenManager指针
    std::thread prod_thread_;                       // 生产者线程
    std::atomic<bool> running_{false};             // 运行标志（原子变量，线程安全）

public:
    /**
     * @brief 构造函数
     * @param token_manager 共享的TokenManager指针
     * 
     * 创建生产者对象，但不会自动启动线程。
     * 需要调用start()方法来启动生产线程。
     */
    TokenProducer(std::shared_ptr<TokenManager> token_manager) : token_manager_(token_manager) {}
    
    /**
     * @brief 析构函数
     * 
     * 自动停止生产者线程，确保资源正确释放。
     */
    ~TokenProducer() { stop(); }

    /**
     * @brief 启动生产者线程
     * 
     * 创建并启动一个新线程，在该线程中定期生产token。
     * 每500ms尝试添加一个token（如果未达到最大数量）。
     * 线程会一直运行直到调用stop()。
     */
    void start () {
        running_ = true;
        prod_thread_ = std::thread([this]() {
            while (running_.load()) {
                token_manager_->AddToken();  // 尝试添加token（如果未达到上限）
                // 等待500ms后再生产下一个token
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
    }
    
    /**
     * @brief 停止生产者线程
     * 
     * 设置运行标志为false，并等待线程结束。
     * 线程会在下一次循环检查时退出。
     */
    void stop () {
        running_ = false;  // 设置停止标志
        if (prod_thread_.joinable()) {
            prod_thread_.join();  // 等待线程结束
        }
    }
};
