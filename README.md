# Token 管理系统

## 📋 项目简介

这是一个基于 **生产者-消费者模式** 的线程安全 Token 管理系统，使用 C++11 标准库实现。系统模拟了多个生产者线程定期生成 Token，多个消费者线程并发消费 Token 的场景，展示了现代 C++ 并发编程的核心概念和实践。

## 🎯 面试场景说明

本项目作为 **C++ 并发编程面试题**，主要考察以下能力：

- **多线程编程基础**：线程创建、同步、通信
- **线程安全设计**：互斥锁、条件变量的使用
- **现代 C++ 特性**：智能指针、lambda 表达式、原子操作
- **资源管理**：RAII 原则、异常安全
- **问题解决能力**：线程阻塞、死锁预防、优雅停止

## 🏗️ 架构设计

### 核心组件

```
┌─────────────────┐
│   TokenManager  │  ← 核心管理器（线程安全的 Token 存储）
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
┌───▼───┐ ┌──▼──────┐
│Producer│ │Customer │  ← 多个生产者和消费者线程
└────────┘ └─────────┘
```

### 类设计

1. **TokenManager** (`token_manager.h`)
   - 线程安全的 Token 存储和管理
   - 使用 `std::mutex` 保护共享数据
   - 使用 `std::condition_variable` 实现线程间通信
   - 支持阻塞和非阻塞的消费操作
   - **关键特性**：可中断的消费操作（避免永久阻塞）

2. **TokenProducer** (`token_producer.h`)
   - 独立线程运行的生产者
   - 每 500ms 生产一个 Token
   - 支持优雅停止

3. **TokenCustomer** (`token_customer.h`)
   - 独立线程运行的消费者
   - 持续尝试消费指定数量的 Token
   - 支持回调函数通知消费成功
   - 支持优雅停止（可中断等待）

## 🔑 技术要点

### 1. 线程同步机制

**互斥锁 (Mutex)**
```cpp
std::mutex mtx_;  // 保护共享数据 current_tokens_
std::lock_guard<std::mutex> lock(mtx_);  // RAII 自动加锁解锁
```

**条件变量 (Condition Variable)**
```cpp
std::condition_variable cond_;
cond_.wait(lock, predicate);  // 阻塞等待直到条件满足
cond_.wait_for(lock, timeout, predicate);  // 可中断的等待
```

### 2. 原子操作

```cpp
std::atomic<bool> running_{false};  // 线程安全的标志位
running_.load();  // 原子读取
running_ = false;  // 原子写入
```

### 3. 智能指针

```cpp
std::shared_ptr<TokenManager> token_manager_;  // 共享所有权
std::unique_ptr<TokenProducer> producer;  // 独占所有权
```

### 4. 可中断的等待机制

**问题**：传统的 `condition_variable::wait()` 无法响应停止信号，可能导致线程永久阻塞。

**解决方案**：使用 `wait_for()` 定期检查停止标志
```cpp
bool ConsumeTokensWithStopCheck(size_t n, std::atomic<bool>* stop_flag) {
    while (current_tokens_ < n) {
        if (stop_flag && stop_flag->load()) {
            return false;  // 响应停止信号
        }
        cond_.wait_for(lock, std::chrono::milliseconds(100), predicate);
    }
    // ...
}
```

### 5. RAII 资源管理

```cpp
~TokenProducer() { stop(); }  // 析构函数自动清理
~TokenCustomer() { stop(); }  // 确保线程正确退出
```

## 🚀 编译和运行

### 环境要求

- C++11 或更高版本的编译器
- 支持多线程的 C++ 标准库

### 编译命令

**Linux/macOS:**
```bash
g++ -std=c++11 -pthread main.cpp -o token_system
```

**Windows (MinGW):**
```bash
g++ -std=c++11 main.cpp -o token_system.exe
```

**Windows (MSVC):**
```bash
cl /EHsc /std:c++11 main.cpp
```

### 运行

```bash
./token_system        # Linux/macOS
token_system.exe      # Windows
```

### 预期输出

```
token manager show case
initialize the manager, the max_tokens is :10
active the token manager
initial the consumer, per 3: 
1
2
3
4
5
Waiting for consumers and producers to run...
consumer 1 success consume: 3 tokens
consumer 2 success consume: 3 tokens
...
total time: 10000
consumer[1] 
consumer[2] 
...
last tokens: X
case finish
```

## 📊 面试考察点详解

### 1. 线程安全设计

**考察点**：
- 如何保护共享数据？
- 为什么使用 `mutable` 修饰互斥锁？
- 死锁如何预防？

**答案要点**：
- 使用互斥锁保护所有对 `current_tokens_` 的访问
- `mutable` 允许在 `const` 方法中使用互斥锁
- 使用 `lock_guard` 和 `unique_lock` 的 RAII 特性避免死锁

### 2. 条件变量的使用

**考察点**：
- 为什么使用条件变量而不是轮询？
- `wait()` 和 `wait_for()` 的区别？
- 为什么需要 `notify_all()`？

**答案要点**：
- 条件变量避免 CPU 空转，提高效率
- `wait()` 无限等待，`wait_for()` 可设置超时
- `notify_all()` 唤醒所有等待的线程（多个消费者场景）

### 3. 线程停止机制

**考察点**：
- 如何优雅地停止线程？
- 如何避免线程永久阻塞？
- 为什么需要 `joinable()` 检查？

**答案要点**：
- 使用原子标志位 `running_` 控制线程循环
- 使用可中断的等待机制（`ConsumeTokensWithStopCheck`）
- `joinable()` 检查避免重复 join 导致的未定义行为

### 4. 现代 C++ 特性

**考察点**：
- 智能指针的选择（`shared_ptr` vs `unique_ptr`）？
- Lambda 表达式的使用场景？
- 原子操作的作用？

**答案要点**：
- `shared_ptr` 用于多线程共享资源，`unique_ptr` 用于独占所有权
- Lambda 用于线程函数，捕获 `this` 访问成员变量
- 原子操作保证标志位的读写是线程安全的，无需加锁

### 5. 资源管理

**考察点**：
- RAII 原则的应用？
- 析构函数中为什么需要 `stop()`？
- 如何确保异常安全？

**答案要点**：
- 使用智能指针和 `lock_guard` 实现 RAII
- 析构函数中停止线程，确保资源正确释放
- 使用 RAII 确保即使发生异常也能正确清理

## 🔍 可能的扩展方向

### 1. 性能优化
- 使用读写锁（`shared_mutex`）优化读多写少的场景
- 实现无锁队列（lock-free queue）
- 使用线程池管理生产者和消费者

### 2. 功能增强
- 支持不同的消费优先级
- 支持 Token 的过期机制
- 添加统计信息（生产/消费速率、等待时间等）
- 支持配置文件动态调整参数

### 3. 错误处理
- 添加异常处理机制
- 实现超时机制
- 添加日志记录

### 4. 测试
- 单元测试（gtest）
- 并发测试（压力测试、竞态条件测试）
- 性能基准测试

## 📝 代码结构

```
token_/
├── main.cpp              # 主程序入口
├── token_manager.h       # Token管理器（核心类）
├── token_producer.h      # 生产者类
├── token_customer.h      # 消费者类
└── README.md            # 项目说明文档
```

## 💡 面试建议

1. **代码审查能力**：能够指出代码中的潜在问题（如线程阻塞、资源泄漏）
2. **设计思路**：能够解释为什么选择某种设计（如为什么用 `shared_ptr`）
3. **问题解决**：能够提出改进方案（如可中断等待的实现）
4. **性能意识**：能够分析性能瓶颈和优化方向
5. **调试能力**：能够分析多线程问题的调试方法

## 🐛 常见问题

### Q: 为什么消费者线程会永久阻塞？
**A**: 如果使用 `ConsumeTokens()` 而不是 `ConsumeTokensWithStopCheck()`，线程在等待时会无法响应停止信号。解决方案是使用可中断的等待机制。

### Q: 为什么需要 `joinable()` 检查？
**A**: 对已经 join 过的线程再次 join 会导致未定义行为。`joinable()` 检查确保线程是可 join 的。

### Q: 为什么使用 `shared_ptr` 而不是 `unique_ptr`？
**A**: 多个生产者和消费者都需要访问同一个 `TokenManager`，需要共享所有权，因此使用 `shared_ptr`。

## 📚 参考资源

- [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action)
- [cppreference.com - Thread support library](https://en.cppreference.com/w/cpp/thread)
- [Effective Modern C++](https://www.aristeia.com/books.html)

---

**作者**: 面试候选人  
**日期**: 2024  
**版本**: 1.0

