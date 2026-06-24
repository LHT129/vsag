---
title: "修复 Footer Parse 流长度小于 16 字节时无符号下溢崩溃"
status: in_progress
priority: medium
created_by: "claude-opus"
assigned_to: "opencode"
created_at: "2026-06-08"
updated_at: "2026-06-24"
target_repo: "antgroup/vsag"
related_files:
  - "src/storage/serialization.cpp"
pr_url: ""
---

# 修复 Footer Parse 流长度小于 16 字节时无符号下溢崩溃

## 问题描述

在 `src/storage/serialization.cpp` 约第 48-50 行，`Footer::Parse` 计算 `reader.Length() - 16` 用于 seek：

```cpp
reader.PushSeek(reader.Length() - 16);
```

当流长度小于 16 字节时（如空文件或损坏的极短文件），`reader.Length() - 16` 产生 `uint64_t` 无符号下溢（变为极大值），seek 到一个无效位置，后续 Read 崩溃。

## 建议修复

在函数开头添加最小长度检查：
```cpp
if (reader.Length() < 16) {
    return nullptr;  // 太短，不可能是有效 footer
}
```
