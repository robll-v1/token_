/**
 * @file token_manager.h
 * @brief Token管理器类 - 线程安全的Token存储和管理
 * 
 * TokenManager负责管理token的存储和线程安全的访问。
 * 它使用互斥锁和条件变量来确保多线程环境下的安全性。
 * 支持以下操作：
 * - 添加token（如果未达到最大数量）
 * - 尝试消费token（非阻塞）
 * - 阻塞等待消费token（直到有足够token）
 * - 可中断的消费token（可以响应停止信号）
 */

#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

/**
 * @class TokenManager
 * @brief 线程安全的Token管理器
 * 
 * 该类使用互斥锁和条件变量实现线程安全的token管理。
 * 支持多个生产者和消费者并发访问。
 */
class TokenManager{
private:
    const size_t max_tokens_;           // 最大token数量限制
    size_t current_tokens_;              // 当前token数量
    mutable std::mutex mtx_;             // 保护共享数据的互斥锁
    std::condition_variable cond_;        // 用于线程间通信的条件变量

public:
    /**
     * @brief 构造函数
     * @param max_tokens 允许的最大token数量
     */
    explicit TokenManager(size_t max_tokens) : max_tokens_(max_tokens), current_tokens_(0) {}
    
    /**
     * @brief 添加一个token
     * @return 如果成功添加返回true，如果已达到最大数量返回false
     * 
     * 线程安全地增加token数量，如果未达到上限则增加并通知等待的消费者。
     */
    bool AddToken () {
        std::lock_guard<std::mutex> lock(mtx_);
        if (current_tokens_ < max_tokens_) {
            current_tokens_++;
            cond_.notify_all();  // 通知所有等待的消费者
            return true;   
        }        
        return false;  // 已达到最大数量，无法添加
    }

    /**
     * @brief 尝试消费指定数量的token（非阻塞）
     * @param n 要消费的token数量
     * @return 如果成功消费返回true，如果token不足返回false
     * 
     * 这是一个非阻塞操作，如果token不足会立即返回false。
     */
    bool TryConsumeTokens (size_t n) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (current_tokens_ >= n) {
            current_tokens_ -= n;
            return true;
        }
        return false;  // token不足，无法消费
    }

    /**
     * @brief 阻塞等待并消费指定数量的token
     * @param n 要消费的token数量
     * @return 总是返回true（因为会等待直到有足够token）
     * 
     * 如果当前token不足，会阻塞等待直到有足够的token。
     * 注意：此方法无法被中断，可能导致线程永久阻塞。
     */
    bool ConsumeTokens (size_t n) {
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待直到有足够的token
        cond_.wait(lock, [this, n] () {
            return current_tokens_ >= n;
        });
        current_tokens_ -= n;
        return true;
    }

    /**
     * @brief 可中断地消费指定数量的token
     * @param n 要消费的token数量
     * @param stop_flag 指向停止标志的指针，如果为true则中断等待
     * @return 如果成功消费返回true，如果被停止信号中断返回false
     * 
     * 这是一个可中断的消费操作。如果token不足，会使用wait_for定期检查停止标志。
     * 每100ms检查一次，如果stop_flag为true则立即返回false。
     * 这允许线程在等待时响应停止信号，避免永久阻塞。
     */
    bool ConsumeTokensWithStopCheck (size_t n, std::atomic<bool>* stop_flag) {
        std::unique_lock<std::mutex> lock(mtx_);
        while (current_tokens_ < n) {
            // 检查停止标志
            if (stop_flag && stop_flag->load()) {
                return false;  // 被停止信号中断
            }
            // 使用wait_for定期检查，每100ms检查一次
            cond_.wait_for(lock, std::chrono::milliseconds(100), [this, n, stop_flag] () {
                return current_tokens_ >= n || (stop_flag && stop_flag->load());
            });
            // 再次检查停止标志
            if (stop_flag && stop_flag->load()) {
                return false;  // 被停止信号中断
            }
        }
        // 有足够的token，执行消费
        current_tokens_ -= n;
        return true;
    }

    /**
     * @brief 获取当前token数量
     * @return 当前token数量
     * 
     * 线程安全地获取当前token数量。
     */
    size_t GetTokens () const {
        std::lock_guard<std::mutex> lock(mtx_);
        return current_tokens_;
    }
    
    /**
     * @brief 析构函数
     */
    ~TokenManager();
};

// 析构函数实现（空实现）
TokenManager::~TokenManager(){
}
