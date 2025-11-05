# MySQL 三大日志系统与两阶段提交

## 📚 三大日志系统

### 1. Binlog（二进制日志）

#### 位置
- 位于 **Server 层**的通用日志系统
- 与存储引擎无关，任何存储引擎都可以使用

#### 类型：逻辑日志

记录所有数据变更操作的**逻辑**：

- **Statement 模式**：记录 SQL 语句
  ```sql
  UPDATE users SET age = 25 WHERE id = 1;
  ```
  
- **Row 模式**：记录修改前和修改后的行数据
  ```json
  {
    "before": {"id": 1, "age": 20},
    "after": {"id": 1, "age": 25}
  }
  ```

#### 特点
- **顺序追加写**：日志文件写到一定大小后换用下一个文件
- **不覆盖**：历史日志不会丢失

#### 用途
1. **备份和数据恢复**：通过 binlog 重放操作恢复数据
2. **主从复制**：简化主从复制，不需要迁移全量数据，只需要转移操作逻辑
3. **数据审计**：记录所有数据变更历史

---

### 2. Redo Log（重做日志）

#### 位置
- 位于 **InnoDB 引擎**的物理日志
- 引擎层特有

#### 类型：物理日志

- **固定大小**：通常由 `innodb_log_file_size` 和 `innodb_log_files_in_group` 配置
- 记录数据页的**最小修改**（类似于 git diff）
- 仅记录数据页的**物理修改**，不记录逻辑操作

#### 特点
- **循环写**：写满后会覆盖重新写
- **实时写入**：事务提交时立即写入（WAL：Write-Ahead Logging）

#### 用途
1. **崩溃恢复（Crash Recovery）**：意外崩溃后通过 redo log 恢复数据
2. **日志先行，提升性能**：
   - 将操作先记录在日志中
   - 再慢慢更新到磁盘（减少随机写）
   - 提高写入性能

#### 工作流程
```
事务修改 → 写入 redo log buffer → 刷盘到 redo log file → 后台线程异步写入数据页
```

---

### 3. Undo Log（回滚日志）

#### 位置
- InnoDB 引擎层

#### 类型：逻辑日志

- 存储记录的**历史版本**
- 以**逻辑日志**的形式形成**版本链**
- 按照事务隔离级别逻辑存储

#### 用途
1. **事务回滚**：事务提交前的回滚操作
2. **MVCC（多版本并发控制）**：
   - 实现一致性读（Consistent Read）
   - 不同事务看到不同版本的数据
   - 支持 Read Committed 和 Repeatable Read 隔离级别

#### 版本链结构
```
当前记录 (事务ID=100) ← undo log (事务ID=99) ← undo log (事务ID=98) ...
```

---

## 🔄 两阶段提交（Two-Phase Commit, 2PC）

### 为什么需要两阶段提交？

**问题场景**：
- **Binlog** 在 Server 层，用于主从复制和备份
- **Redo Log** 在 InnoDB 引擎层，用于崩溃恢复
- 两者必须**保持一致**，否则会导致数据不一致

**不一致的后果**：
- 如果 redo log 提交了，但 binlog 没提交 → 主从数据不一致
- 如果 binlog 提交了，但 redo log 没提交 → 崩溃恢复后数据丢失

### 两阶段提交流程

#### 阶段一：Prepare（准备阶段）

```
1. 事务开始
2. 执行 SQL，修改数据页
3. 写入 undo log（用于回滚）
4. 写入 redo log buffer（未提交）
5. 刷盘 redo log（prepare 状态）
   └─ 此时 redo log 记录为 "prepare" 状态
```

**关键点**：
- Redo log 已经写入磁盘，但标记为 **prepare 状态**
- 此时如果崩溃，可以通过 redo log 恢复，但事务未提交

#### 阶段二：Commit（提交阶段）

```
6. 写入 binlog（提交状态）
7. 刷盘 binlog 到磁盘
8. 提交 redo log（commit 状态）
   └─ 将 redo log 中的 prepare 改为 commit
```

**关键点**：
- Binlog 先写入并刷盘
- 然后 redo log 从 prepare 改为 commit
- 事务正式提交

### 完整流程图

```
┌─────────────────────────────────────────────────────────┐
│                     事务执行流程                          │
└─────────────────────────────────────────────────────────┘

1. 开始事务
   │
   ├─→ 2. 执行 UPDATE/INSERT/DELETE
   │
   ├─→ 3. 写入 undo log（历史版本）
   │
   ├─→ 4. 写入 redo log buffer
   │
   ├─→ 5. 【阶段一】刷盘 redo log（prepare）
   │      └─ redo log: [XID=123, status=PREPARE]
   │
   ├─→ 6. 【阶段二】写入 binlog
   │      └─ binlog: [UPDATE users SET age=25 WHERE id=1]
   │
   ├─→ 7. 【阶段二】刷盘 binlog
   │
   └─→ 8. 【阶段二】提交 redo log（commit）
          └─ redo log: [XID=123, status=COMMIT]
```

### 崩溃恢复机制

#### 场景 1：崩溃发生在 Prepare 之前

```
状态：只有部分 redo log 在 buffer，未刷盘
恢复：回滚事务（undo log）
```

#### 场景 2：崩溃发生在 Prepare 之后，Commit 之前

```
状态：redo log 已刷盘（prepare），但 binlog 可能未刷盘
恢复流程：
1. 扫描 redo log，找到所有 prepare 状态的事务
2. 检查 binlog 中是否有对应的事务
   - 如果 binlog 中存在且完整 → 提交事务（redo log 改为 commit）
   - 如果 binlog 中不存在或不完整 → 回滚事务（undo log）
```

#### 场景 3：崩溃发生在 Commit 之后

```
状态：redo log commit，binlog 已提交
恢复：事务已完成，无需恢复
```

### 关键设计点

#### 1. XID（事务 ID）
- 同一个事务的 redo log 和 binlog 使用**相同的 XID**
- 用于崩溃恢复时匹配两个日志

#### 2. Binlog 的完整性检查
- Binlog 中有 `commit` 标记
- 恢复时检查 binlog 是否完整

#### 3. 组提交优化
- 多个事务可以**批量提交** binlog
- 减少磁盘 I/O

### 代码层面的理解

```c
// 伪代码：两阶段提交
void commit_transaction(Transaction* txn) {
    // 阶段一：Prepare
    redo_log_write_prepare(txn->xid, txn->changes);
    redo_log_flush();  // 刷盘
    
    // 阶段二：Commit
    binlog_write(txn->xid, txn->sql);  // 写入 binlog
    binlog_flush();  // 刷盘 binlog
    
    redo_log_write_commit(txn->xid);  // redo log 改为 commit
    redo_log_flush();  // 刷盘
}
```

### 崩溃恢复伪代码

```c
void crash_recovery() {
    // 1. 扫描所有 prepare 状态的 redo log
    List<Transaction> prepare_txns = scan_redo_log_prepare();
    
    for (Transaction txn : prepare_txns) {
        // 2. 检查 binlog 中是否有对应事务
        if (binlog_contains(txn->xid) && binlog_is_complete(txn->xid)) {
            // binlog 存在且完整 → 提交
            redo_log_commit(txn->xid);
        } else {
            // binlog 不存在或不完整 → 回滚
            undo_log_rollback(txn->xid);
        }
    }
}
```

---

## 📊 三大日志对比总结

| 特性 | Binlog | Redo Log | Undo Log |
|------|--------|----------|----------|
| **位置** | Server 层 | InnoDB 引擎层 | InnoDB 引擎层 |
| **类型** | 逻辑日志 | 物理日志 | 逻辑日志 |
| **记录内容** | SQL 语句或行数据变更 | 数据页物理修改 | 历史版本数据 |
| **写入方式** | 顺序追加 | 循环覆盖 | 版本链 |
| **主要用途** | 主从复制、备份恢复 | 崩溃恢复、性能优化 | 事务回滚、MVCC |
| **是否参与 2PC** | ✅ 是 | ✅ 是 | ❌ 否（参与回滚） |

---

## 🔍 实际应用场景

### 场景 1：主从复制

```
主库：
1. 事务执行 → redo log (prepare) → binlog → redo log (commit)
2. Binlog 传输到从库
3. 从库重放 binlog，实现数据同步
```

### 场景 2：数据恢复

```
1. 使用最近的全量备份恢复数据
2. 重放 binlog 从备份时间点到故障时间点
3. 恢复到故障前的状态
```

### 场景 3：MVCC 读取

```
1. 事务开始，记录当前活跃事务列表
2. 读取数据时，根据 undo log 版本链找到合适的版本
3. 实现一致性读（不受其他事务影响）
```

---

## 💡 关键要点

1. **两阶段提交保证数据一致性**：确保 redo log 和 binlog 的一致性
2. **WAL 机制提升性能**：先写日志，再写数据页，减少随机 I/O
3. **崩溃恢复自动处理**：通过两阶段提交机制，自动判断事务状态
4. **MVCC 实现并发控制**：通过 undo log 版本链实现多版本并发控制

---

**参考**：
- MySQL InnoDB 存储引擎
- MySQL 官方文档
- 数据库事务处理原理

