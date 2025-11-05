/**
 * @file token_customer.h
 * @brief Token消费者类 - 从TokenManager消费token的线程
 * 
 * TokenCustomer在独立线程中运行，定期从TokenManager消费指定数量的token。
 * 支持可中断的消费操作，可以通过stop()方法优雅地停止。
 */

#pragma once 

#include "token_manager.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

/**
 * @class TokenCustomer
 * @brief Token消费者类
 * 
 * 在独立线程中运行，持续从TokenManager消费token。
 * 每次消费指定数量的token，并在成功后调用回调函数。
 * 支持优雅停止，可以响应停止信号中断等待。
 */
class TokenCustomer {
private:
    std::shared_ptr<TokenManager> token_manager_;      // 共享的TokenManager指针
    std::thread cons_thread_;                          // 消费者线程
    const size_t tokens_per_customer_;                 // 每次消费的token数量
    std::atomic<bool> running_ {false};               // 运行标志（原子变量，线程安全）
    std::function<void(bool)> call_back_;              // 消费成功后的回调函数
    const size_t max_cons_count_;                      // 最大消费次数（0表示无限制）
    size_t cons_count{0};                              // 当前消费次数
    std::chrono::steady_clock::time_point start_;      // 开始时间
    std::chrono::steady_clock::time_point end_;        // 结束时间

public:
    /**
     * @brief 构造函数
     * @param token_manager 共享的TokenManager指针
     * @param tokens_per_customer 每次消费的token数量
     * @param call_back 消费成功后的回调函数（可选）
     * 
     * 创建消费者对象，但不会自动启动线程。
     * 需要调用start()方法来启动消费线程。
     */
    TokenCustomer (std::shared_ptr<TokenManager> token_manager,
                    const size_t tokens_per_customer,
                    std::function<void(bool)> call_back = nullptr):
    token_manager_(std::move(token_manager)),
    tokens_per_customer_(tokens_per_customer),
    call_back_(call_back),
    max_cons_count_(0) {}  // 默认无限制消费

    /**
     * @brief 启动消费者线程
     * 
     * 创建并启动一个新线程，在该线程中持续消费token。
     * 线程会一直运行直到调用stop()或达到最大消费次数。
     * 使用ConsumeTokensWithStopCheck确保可以响应停止信号。
     */
    void start () {
        running_ = true;
        cons_thread_ = std::thread([this]() {
            start_ = std::chrono::steady_clock::now();  // 记录开始时间
            while (running_.load()) {
                // 检查是否达到最大消费次数
                if (max_cons_count_ > 0 && cons_count >= max_cons_count_) {
                    break;
                }
                // 尝试消费token（可中断）
                bool success = token_manager_->ConsumeTokensWithStopCheck(tokens_per_customer_, &running_);
                if (!success) {
                    break;  // 被停止信号中断
                }
                cons_count++;  // 增加消费计数

                // 如果设置了回调函数，调用它
                if (call_back_) {
                    call_back_(success);
                }
            }
            end_ = std::chrono::steady_clock::now();  // 记录结束时间
        });
    }

    /**
     * @brief 停止消费者线程
     * 
     * 设置运行标志为false，并等待线程结束。
     * 这会中断ConsumeTokensWithStopCheck中的等待。
     */
    void stop () {
        running_ = false;  // 设置停止标志
        if (cons_thread_.joinable()) {
            cons_thread_.join();  // 等待线程结束
        }
    }

    /**
     * @brief 析构函数
     * 
     * 自动停止消费者线程，确保资源正确释放。
     */
    ~TokenCustomer() {
        stop();
    }

    /**
     * @brief 计算运行时间
     * @return 运行时间（毫秒）
     * 
     * 返回从start()到线程结束的时间差。
     * 如果线程还未结束，返回的时间可能不准确。
     */
    uint32_t CountTime () const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end_ - start_).count();
    }
};