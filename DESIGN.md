---
title: "重构 IO 层为可组合、高性能、异步与缓存友好的存储访问架构"
status: implemented
priority: medium
created_by: "codex-research"
assigned_to: ""
created_at: "2026-08-21"
updated_at: "2026-08-25"
target_repo: "antgroup/vsag"
related_files:
  - "src/io/common/basic_io.h"
  - "src/io/common/io_syscall.h"
  - "src/io/io_headers.h"
  - "src/io/memory_io/memory_io.h"
  - "src/io/memory_io/memory_io.cpp"
  - "src/io/memory_block_io/memory_block_io.h"
  - "src/io/memory_block_io/memory_block_io.cpp"
  - "src/io/mmap_io/mmap_io.h"
  - "src/io/mmap_io/mmap_io.cpp"
  - "src/io/buffer_io/buffer_io.h"
  - "src/io/buffer_io/buffer_io.cpp"
  - "src/io/async_io/async_io.h"
  - "src/io/async_io/async_io.cpp"
  - "src/io/uring_io/uring_io.h"
  - "src/io/uring_io/uring_io.cpp"
  - "src/io/reader_io/reader_io.h"
  - "src/io/reader_io/reader_io.cpp"
  - "src/io/noncontinuous_io/noncontinuous_io.h"
  - "src/io/container/io_array.h"
  - "src/io/read_cache/page_cache.h"
  - "src/io/read_cache/lru_page_cache.h"
  - "src/datacell/flatten_datacell.h"
  - "src/datacell/bucket_datacell.h"
  - "src/datacell/graph_datacell.h"
  - "src/datacell/rabitq_split_datacell.h"
  - "src/datacell/flatten_interface.cpp"
  - "src/datacell/bucket_interface.cpp"
  - "src/datacell/graph_interface.cpp"
  - "src/datacell/rabitq_split_datacell_factory_impl.h"
pr_url: ""
---

# 重构 IO 层为可组合、高性能、异步与缓存友好的存储访问架构

## 1. 摘要

VSAG 当前 IO 层能够较好支撑 MemoryIO、MemoryBlockIO、MMapIO、BufferIO、AsyncIO、UringIO 和 ReaderIO 等现有本地同步/批量读取场景，且通过 CRTP 避免了主读取路径的虚函数开销。但随着以下能力进入中长期规划，当前以 `BasicIO<Derived>` 为中心、每个 IO 类型完整实现所有行为的架构会逐渐出现组合困难：

1. 深化 io_uring，例如 registered files、fixed buffers、多批次流水线和取消；
2. 提供真正的异步读取，让 IO 与距离计算重叠；
3. 建立 batch-aware 的共享或多级缓存；
4. 支持高延迟远程 Reader；
5. 增加 buffered/direct/mmap/uring/read-ahead/request-coalescing 等访问策略。

本提案建议将 IO 层逐步演进为：

```text
ByteIO<Backend, CachePolicy>
    ├── Backend：存储介质与真实读写能力
    ├── CachePolicy：缓存命中、miss 规划和回填
    ├── ReadRequest：统一的批量读取请求
    ├── ReadLease：显式且自动管理的读取结果所有权
    ├── ReadOperation：同步与异步统一的完成对象
    └── IOEnvironment：allocator、cache、aio/uring pool 等共享资源
```

其中，文件后端内部继续静态组合：

```text
PosixFileBackend<SingleReadPolicy, BatchReadPolicy, DurabilityPolicy>
```

该方案不在热路径引入虚函数、`std::function` 或通用堆分配。Memory/MMap 等性能敏感路径必须通过模板内联、专用 BorrowedLease 和 `NoCache` 特化生成与当前实现基本等价的机器码。

实施过程采用分阶段验证：先以“性能基线 + 核心类型 + MemoryIO”验证零开销目标；汇编、microbenchmark、端到端 benchmark、编译时间和代码清晰度通过退出条件后，再迁移其他后端。最终代码直接替换正式 IO 类型，不在仓库中长期维护两套实现。

## 2. 背景与动机

### 2.1 重构前架构的优势

重构前架构并非设计失败，已有以下优点：

- DataCell 模板化在具体 `IOTmpl` 上，主读取路径无需运行时 IO 类型判断；
- `BasicIO<Derived>` 使用 CRTP，允许编译器内联具体 `ReadImpl`/`WriteImpl`；
- 已提供单读、批读、DirectRead、序列化、外部 Reader 和基础 page cache；
- AsyncIO 和 UringIO 已具备真正的内核批量提交实现；
- MemoryIO/MMapIO 能返回借用指针，避免无意义复制；
- NonContinuousIO 能将逻辑地址映射到底层非连续存储区域。

因此，本提案的目标不是为了“抽象而抽象”或单纯减少代码行数，而是保留上述性能特征，同时为异步、缓存、远程和策略组合建立可持续的边界。

### 2.2 BasicIO 当前承担的职责过多

`src/io/common/basic_io.h` 当前同时承担：

- CRTP 方法分发；
- 逻辑长度 `size_` 和序列化起点 `start_`；
- 范围检查；
- Read/Write/MultiRead/DirectRead/Release；
- Resize/Shrink fallback；
- read cache 创建、共享、读取和失效；
- Serialize/Deserialize；
- ReaderIO 的 SkipDeserialize 特殊路径；
- 通过 SFINAE 检测可选 `*Impl` 方法；
- cache 开启时替换 DirectRead 的内存所有权语义。

这些职责本身分别合理，但全部集中在一个 CRTP 基类中，使底层存储、缓存编排、序列化策略和读取结果所有权相互耦合。

### 2.3 每个文件 IO 同时表达“介质”和“策略”

当前类型大致隐含以下组合：

```text
BufferIO = POSIX 文件 + buffered pread + 顺序批读 + no flush
AsyncIO  = POSIX 文件 + O_DIRECT + libaio 批读 + 每次写后 fsync
UringIO  = POSIX 文件 + 可选 O_DIRECT + io_uring 批读 + no flush
```

文件生命周期、fd、direct alignment、批量执行、flush、内存释放和 fallback 都位于同一个完整 IO 类中。继续增加访问策略时，容易形成新的完整类组合并复制公共代码。

### 2.4 当前读取结果所有权协议脆弱

大量调用点使用：

```cpp
bool need_release = false;
const auto* data = io_->Read(size, offset, need_release);
try {
    Compute(data);
} catch (...) {
    if (need_release) {
        io_->Release(data);
    }
    throw;
}
if (need_release) {
    io_->Release(data);
}
```

该协议需要调用方理解返回指针可能来自：

- MemoryIO/MMapIO 内部地址；
- Allocator 分配；
- O_DIRECT 对齐分配；
- cache 命中后的新 buffer；
- MemoryBlockIO 跨 block 拼接 buffer。

它容易导致异常路径泄漏、释放方式不匹配、early return 遗漏和 borrowed pointer 生命周期误用。

### 2.5 当前 cache 会切断 backend 批读能力

没有 cache 时：

```text
BasicIO::MultiRead
    → UringIO::MultiReadImpl / AsyncIO::MultiReadImpl
```

开启 cache 后：

```text
BasicIO::MultiRead
    → for each request
        → ReadCached
            → miss 后调用单次 ReadImpl
```

这意味着 cache 与异步批读无法自然组合。部分命中或大量 miss 时，原本可以交给 io_uring/libaio 的批量请求可能退化为逐项、逐页读取。

### 2.6 当前同步 API 无法自然表达真正异步

当前 `MultiRead` 隐含以下契约：

- 函数返回时所有读取已经完成；
- destination buffer 生命周期只需覆盖调用过程；
- 没有 completion、cancel、timeout 和部分完成状态；
- 搜索算法不能在 IO 完成前处理其他计算。

真正异步需要将请求、operation 和 buffer 生命周期提升为一等概念，而不是只在 `MultiReadImpl` 内部使用异步内核 API。

## 3. 目标与非目标

### 3.1 必须实现的目标

1. Release 构建中，MemoryIO/MMapIO 单次读取不增加虚调用、间接调用、锁和堆分配；
2. cache disabled 路径不进行 cache map 查找或加锁；
3. 同步读取接口不因统一 operation 模型产生堆分配；
4. AsyncIO/UringIO 的批量提交次数不得增加；
5. cache miss 必须能够继续通过 backend 的批读能力执行；
6. 保留现有 JSON IO 类型、参数兼容性和序列化格式；
7. 保留现有 IO 类名或提供无行为变化的兼容别名/包装；
8. 明确读取结果所有权，内部热点调用逐步移除 `need_release`；
9. 集中 IO 类型到 C++ 类型的映射，减少工厂遗漏；
10. 明确 IO 并发契约，不在每次读取中加入全局锁；
11. 为 io_uring、真正异步、共享缓存和远程 Reader 留出稳定扩展点；
12. 每个阶段均可停止或回滚，不要求一次性完成完整重构。

### 3.2 非目标

1. 本提案第一阶段不实现远程 Reader；
2. 第一阶段不修改 HGraph/RaBitQ 搜索算法为异步流水线；
3. 第一阶段不实现多级缓存、TinyLFU 或磁盘 cache；
4. 不借重构机会改变 fsync、文件删除、越界返回和异常类型等语义；
5. 不允许用户任意组合所有模板 policy；只实例化官方支持的有限 profile；
6. 不要求所有函数强制内联；复杂 io_uring/cache miss 代码可保留一次普通直接调用；
7. 不以减少总代码行数作为首要验收指标；迁移期代码量可能上升。

## 4. 设计原则

### 4.1 热路径静态组合，冷路径允许普通运行时逻辑

以下路径使用模板静态组合并允许完全内联：

- 范围检查；
- Memory/MMap ReadAt/Acquire；
- `NoCache` 转发；
- 简单 buffered pread 包装；
- BorrowedLease 访问。

以下路径可以保留普通非虚函数调用：

- 文件 open/close/fstat/truncate；
- mmap/mremap；
- io_uring/libaio 批量准备、提交、完成和错误恢复；
- 大批次 cache miss 规划；
- 远程请求提交；
- 序列化大块复制。

### 4.2 禁用能力零成本

- `NoCache` 在编译期完全转发到 backend；
- Memory/MMap 不使用通用 owner tag；
- 同步 IO 不创建堆上的 `ReadOperation`；
- 未启用 O_DIRECT 时不执行对齐计算；
- 小批次不强制进入排序、去重和合并 planner；
- 不支持异步的 backend 返回栈上 `ImmediateOperation`。

### 4.3 显式能力代替 SFINAE 猜测

每个 backend 明确声明：

```cpp
struct Capabilities {
    static constexpr bool InMemory = false;
    static constexpr bool Writable = true;
    static constexpr bool Resizable = true;
    static constexpr bool Borrowable = false;
    static constexpr bool NativeBatchRead = true;
    static constexpr bool AsyncReadable = true;
    static constexpr bool CanBindSerializedRange = false;
};
```

不再通过 `has_ReadImpl`、`has_ResizeImpl` 等检测接口是否存在。

### 4.4 存储身份、逻辑长度和物理容量分离

- ByteIO 持有逻辑长度；
- Backend/Region 持有物理容量、文件大小或远程对象大小；
- CacheKey 使用稳定 StorageId 和 generation；
- MMap 的最小物理映射不再等同于逻辑长度；
- Reader 绑定的序列化区间显式保存 start/size。

### 4.5 不开放无限 policy 组合

内部可以使用策略组合，但用户配置只映射到官方 profile，例如：

```text
memory
block_memory
mmap
buffer_io
async_io
uring_io
reader_io
```

未来新增 `remote_cached_io` 等 profile 时，也应显式评审、实例化和 benchmark，避免模板组合爆炸。

## 5. 总体架构

### 5.1 分层关系

```text
DataCell<IOTmpl>
    ↓
ByteIO<Backend, CachePolicy>
    ├── logical size / bounds / compatibility API
    ├── CachePolicy
    │     ├── NoCache
    │     └── OptionalPageCache / SharedPageCacheCoordinator
    └── Backend
          ├── ContiguousBackend<HeapRegion>
          ├── ContiguousBackend<MMapRegion>
          ├── BlockMemoryBackend
          ├── PosixFileBackend<SingleRead, BatchRead, Durability>
          ├── ExternalReaderBackend
          └── MappedBackend<ExtentMapper, PhysicalBackend>
```

### 5.2 推荐目录结构

```text
src/io/
  core/
    byte_io.h
    io_capabilities.h
    read_request.h
    read_lease.h
    read_operation.h
    io_environment.h
    io_serialization.h
    io_contract_test.h

  backend/
    contiguous_backend.h
    heap_region.h
    heap_region.cpp
    mmap_region.h
    mmap_region.cpp
    block_memory_backend.h
    block_memory_backend.cpp
    posix_file.h
    posix_file.cpp
    posix_file_backend.h
    external_reader_backend.h
    external_reader_backend.cpp

  policy/
    sync_single_read.h
    direct_single_read.h
    sequential_batch_read.h
    libaio_batch_read.h
    libaio_batch_read.cpp
    uring_batch_read.h
    uring_batch_read.cpp
    durability_policy.h
    cache_policy.h
    cache_policy.cpp

  adapter/
    extent_mapper.h
    mapped_backend.h
    compatibility_api_adapter.h

  factory/
    io_kind.h
    io_dispatch.h
    io_profiles.h
```

目录不应继续细分成大量只有几行的小文件。上述边界用于表达稳定职责；实现时可以根据代码量合并相近 header。

## 6. 核心 API

### 6.1 ByteSpan

项目以 C++17 为基础，不能依赖 `std::span`。可以引入轻量内部类型，或复用项目已有等价类型：

```cpp
struct ConstBytes {
    const uint8_t* data{nullptr};
    uint64_t size{0};
};

struct MutableBytes {
    uint8_t* data{nullptr};
    uint64_t size{0};
};
```

### 6.2 ReadRequest

```cpp
struct ReadRequest {
    uint8_t* destination{nullptr};
    uint64_t offset{0};
    uint64_t size{0};
};
```

要求：

- `destination` 指向每个请求自己的目标范围；
- `ReadMany` 不修改请求数组；
- `size == 0` 可允许 `destination == nullptr`；
- 非零 size 时 destination 必须非空；
- ByteIO 在提交 backend 前统一完成 overflow-safe 范围检查；
- 异步 operation 完成前 request 所引用的 destination 必须保持有效。

### 6.3 范围检查

所有路径使用 overflow-safe 检查：

```cpp
inline bool
IsValidRange(uint64_t offset, uint64_t size, uint64_t extent) {
    return offset <= extent and size <= extent - offset;
}
```

禁止用 `offset + size <= extent` 作为唯一判断。

写入结束位置使用 checked addition：

```cpp
uint64_t
CheckedEnd(uint64_t offset, uint64_t size);
```

溢出时抛 `VsagException(ErrorType::INVALID_ARGUMENT, ...)` 或遵循项目统一的参数错误策略。

### 6.4 ByteIO

```cpp
template <typename Backend, typename CachePolicy>
class ByteIO {
public:
    using BackendType = Backend;
    using CacheType = CachePolicy;
    using Lease = typename CachePolicy::template Lease<Backend>;
    using Operation = typename CachePolicy::template Operation<Backend>;

    static constexpr bool InMemory = Backend::Capabilities::InMemory;
    static constexpr bool SkipDeserialize = Backend::Capabilities::CanBindSerializedRange;

    bool
    ReadAt(uint64_t offset, uint64_t size, uint8_t* destination) const;

    bool
    ReadMany(const ReadRequest* requests, uint64_t count) const;

    Operation
    SubmitReads(const ReadRequest* requests, uint64_t count) const;

    Lease
    Acquire(uint64_t offset, uint64_t size) const;

    void
    WriteAt(uint64_t offset, const uint8_t* source, uint64_t size);

    void
    Resize(uint64_t size);

    void
    Shrink(uint64_t size);

    void
    Prefetch(uint64_t offset, uint64_t size);

    uint64_t
    Size() const;

    int64_t
    MemoryUsage() const;

    void
    Serialize(StreamWriter& writer) const;

    void
    Deserialize(StreamReader& reader);

private:
    uint64_t logical_size_{0};
    Backend backend_;
    CachePolicy cache_;
};
```

### 6.5 兼容接口

迁移期继续提供：

```cpp
void
Write(const uint8_t* data, uint64_t size, uint64_t offset) {
    WriteAt(offset, data, size);
}

bool
Read(uint64_t size, uint64_t offset, uint8_t* data) const {
    return ReadAt(offset, size, data);
}

bool
MultiRead(uint8_t* data,
          uint64_t* sizes,
          uint64_t* offsets,
          uint64_t count) const;
```

旧 `Read(size, offset, bool& need_release)` 在迁移期可以通过 CompatibilityReadHandle 适配，但不应成为新代码继续使用的接口。内部 DataCell 热点逐步迁移到 `Acquire()`。

## 7. ReadLease 设计

### 7.1 目标

- 自动释放；
- 移动而不可复制；
- Memory/MMap 路径无析构分支；
- 不使用 `std::function` deleter；
- 不在 borrowed 路径增加引用计数；
- 支持 allocator、aligned buffer、cache page pin 和拼接 buffer。

### 7.2 基础模板

```cpp
template <typename Owner>
class BasicReadLease : private Owner {
public:
    BasicReadLease() = default;

    BasicReadLease(const uint8_t* data, uint64_t size, Owner owner)
        : Owner(std::move(owner)), data_(data), size_(size) {
    }

    BasicReadLease(const BasicReadLease&) = delete;
    BasicReadLease&
    operator=(const BasicReadLease&) = delete;

    BasicReadLease(BasicReadLease&&) noexcept = default;
    BasicReadLease&
    operator=(BasicReadLease&&) noexcept = default;

    explicit operator bool() const {
        return data_ != nullptr;
    }

    const uint8_t*
    Data() const {
        return data_;
    }

    uint64_t
    Size() const {
        return size_;
    }

private:
    const uint8_t* data_{nullptr};
    uint64_t size_{0};
};
```

C++17 下通过私有继承使用 empty-base optimization，使空 `BorrowedOwner` 不增加对象体积。

### 7.3 Owner 类型

```cpp
struct BorrowedOwner {
    // 无状态、空析构。
};

class AllocatorOwner {
public:
    AllocatorOwner(Allocator* allocator, uint8_t* data);
    ~AllocatorOwner();
};

class AlignedOwner {
public:
    explicit AlignedOwner(uint8_t* base);
    ~AlignedOwner();
};

class PageOwner {
public:
    explicit PageOwner(PagePtr page);
};
```

### 7.4 CachePolicy 决定最终 Lease

```cpp
class NoCache {
public:
    template <typename Backend>
    using Lease = typename Backend::Lease;
};

class OptionalPageCache {
public:
    template <typename Backend>
    using Lease = CachedLease<typename Backend::Lease>;
};
```

`CachedLease` 只用于运行时可能命中 cache 的后端。MemoryIO 使用 `NoCache`，不会承担 tagged owner 开销。

### 7.5 第一阶段范围

第一阶段只实现：

- BorrowedLease；
- AllocatorLease；
- compatibility need_release 适配。

AlignedLease、PageLease 和 CachedLease 在对应后端迁移时再实现，避免一次引入过多未验证抽象。

## 8. ReadOperation 设计

### 8.1 同步兼容不是堆对象

同步 backend 返回：

```cpp
class ImmediateOperation {
public:
    explicit ImmediateOperation(bool result) : result_(result) {
    }

    bool
    Poll() const {
        return true;
    }

    bool
    Wait() const {
        return result_;
    }

private:
    bool result_{false};
};
```

它按值返回，可被优化器完全消除。同步 `ReadMany` 也可以直接调用 CachePolicy/Backend 的同步入口，不强制经过 operation 对象。

### 8.2 异步 operation

```cpp
class UringReadOperation {
public:
    UringReadOperation(UringReadOperation&&) noexcept;
    ~UringReadOperation();

    bool
    Poll();

    bool
    Wait();

    void
    Cancel();
};
```

要求：

- operation 析构前必须完成、取消并 drain，或明确将资源移交 executor；
- 不允许 in-flight buffer 在 kernel 仍引用时被释放；
- 部分提交、部分完成和 fatal error 有明确状态；
- 不使用 `std::unique_ptr<IReadOperation>` 作为热路径统一接口；
- Operation 类型由 Backend/CachePolicy 静态决定。

### 8.3 Cache operation

cache 全命中时返回立即完成；部分命中时只为 miss 创建 backend operation：

```text
CachedReadOperation<BackendOperation>
    ├── 已完成的 hit copy
    ├── miss plan
    ├── backend operation
    └── completion 后的 cache fill/copy
```

第一轮 cache 重构可以继续提供同步 `ReadMany`，等 miss planner 稳定后再实现真正异步 CachedReadOperation。

## 9. Backend 契约

Backend 不继承统一虚接口，而是满足明确的静态契约：

```cpp
class ExampleBackend {
public:
    using Capabilities = ...;
    using Lease = ...;
    using Operation = ...;

    uint64_t
    InitialLogicalSize() const;

    StorageIdentity
    Identity() const;

    bool
    ReadAt(uint64_t offset, uint64_t size, uint8_t* destination) const;

    bool
    ReadMany(const ReadRequest* requests, uint64_t count) const;

    Operation
    SubmitReads(const ReadRequest* requests, uint64_t count) const;

    Lease
    Acquire(uint64_t offset, uint64_t size) const;

    void
    WriteAt(uint64_t offset, const uint8_t* source, uint64_t size);

    void
    ResizePhysical(uint64_t size);

    void
    ShrinkPhysical(uint64_t size);
};
```

对于不支持的能力，ByteIO 使用 `if constexpr` 在允许的公共操作中拒绝或选择专门路径，但 backend 的能力必须显式声明，不能由方法是否存在隐式决定。

默认顺序批读以自由函数提供，而不是再创建一个新的 CRTP 基类：

```cpp
template <typename Backend>
bool
SequentialReadMany(const Backend& backend,
                   const ReadRequest* requests,
                   uint64_t count);
```

## 10. 连续存储后端

### 10.1 ContiguousBackend

```cpp
template <typename Region>
class ContiguousBackend {
public:
    using Lease = BasicReadLease<BorrowedOwner>;
    using Operation = ImmediateOperation;

    bool
    ReadAt(uint64_t offset, uint64_t size, uint8_t* destination) const {
        std::memcpy(destination, region_.Data() + offset, size);
        return true;
    }

    Lease
    Acquire(uint64_t offset, uint64_t size) const {
        return Lease(region_.Data() + offset, size, BorrowedOwner{});
    }

    bool
    ReadMany(const ReadRequest* requests, uint64_t count) const {
        for (uint64_t i = 0; i < count; ++i) {
            std::memcpy(requests[i].destination,
                        region_.Data() + requests[i].offset,
                        requests[i].size);
        }
        return true;
    }

    void
    WriteAt(uint64_t offset, const uint8_t* source, uint64_t size) {
        region_.EnsureCapacity(CheckedEnd(offset, size));
        std::memcpy(region_.Data() + offset, source, size);
    }

private:
    Region region_;
};
```

ByteIO 在进入 backend 前完成逻辑范围检查；backend 可在 Debug 构建保留断言，但 Release 不重复检查。

### 10.2 HeapRegion

职责：

- 持有 Allocator；
- 管理 data pointer 和 physical capacity；
- `EnsureCapacity`；
- `ShrinkPhysical`；
- 保留或明确当前 Reallocate 行为；
- 不持有 read cache、序列化状态或 DataCell 语义。

需要明确：

- 逻辑 Resize 是否物理收缩；
- sparse write 形成的 gap 是否要求清零；
- allocation failure 后旧 buffer 必须保持有效；
- Freeze 后不得发生会移动指针的 resize。

第一阶段必须保持当前 MemoryIO 行为，不借机改变 gap 初始化语义。

### 10.3 MMapRegion

职责：

- 文件打开和 fd 生命周期；
- `fstat` 初始化已有文件实际大小；
- 最小映射大小；
- mmap/munmap/mremap；
- macOS 重映射实现；
- truncate 和映射容量同步；
- 映射失败回滚；
- 文件 ownership。

MMapRegion 需要分别维护：

```cpp
uint64_t file_size_;
uint64_t mapped_capacity_;
uint8_t* mapped_data_;
```

ByteIO 持有 `logical_size_`。逻辑长度为 0 时，MMapRegion 可以保留 4 KiB 最小物理映射，但不得把 4 KiB 暴露为可读取逻辑数据。

## 11. BlockMemoryBackend

MemoryBlockIO 不应强行套入 ContiguousBackend，因为它只有单 block 内可零拷贝。

目标行为：

- 单 block Acquire 返回 BorrowedLease；
- 跨 block Acquire 返回 AllocatorLease；
- ReadAt 在多个 block 间复制；
- ReadMany 可根据请求位置直接读取；
- capacity 和 logical size 分离；
- Shrink 释放完整尾部 block；
- block table 在 Freeze 后稳定。

可以使用一个有两种 owner 状态的专用 `BlockReadLease`，但必须 benchmark 析构分支。如果其成本在常见单 block 读取中可见，应通过 `AcquireBorrowed` fast path 或模板化调用点进一步消除。

## 12. POSIX 文件后端

### 12.1 PosixFile

`PosixFile` 是冷路径资源对象，不参与策略分发：

```cpp
class PosixFile {
public:
    PosixFile(const FileOpenOptions& options);
    ~PosixFile();

    int
    ReadFd() const;

    int
    WriteFd() const;

    uint64_t
    Size() const;

    void
    Truncate(uint64_t size);

    StorageIdentity
    Identity() const;
};
```

`FileOpenOptions`：

```cpp
enum class FileOwnership {
    Keep,
    DeleteOnClose,
};

struct FileOpenOptions {
    std::string path;
    bool create{true};
    bool direct_read{false};
    bool separate_read_write_fd{false};
    FileOwnership ownership{FileOwnership::Keep};
};
```

迁移期根据现有“构造前文件是否存在”逻辑生成 ownership，确保行为不变；后续可以在 IO 参数中显式开放 ownership，但不属于本提案首轮范围。

必须统一修复：

- 第二个 fd 打开失败时关闭第一个 fd；
- 新建文件失败路径清理；
- errno 捕获时机；
- close/remove 的 best-effort 语义；
- `fstat` 初始化已有文件大小；
- 单 fd 与双 fd 的配置。

### 12.2 PosixFileBackend

```cpp
template <typename SingleReadPolicy,
          typename BatchReadPolicy,
          typename DurabilityPolicy>
class PosixFileBackend {
public:
    using Lease = typename SingleReadPolicy::Lease;
    using Operation = typename BatchReadPolicy::Operation;

    bool
    ReadAt(uint64_t offset, uint64_t size, uint8_t* destination) const;

    Lease
    Acquire(uint64_t offset, uint64_t size) const;

    bool
    ReadMany(const ReadRequest* requests, uint64_t count) const;

    Operation
    SubmitReads(const ReadRequest* requests, uint64_t count) const;

    void
    WriteAt(uint64_t offset, const uint8_t* source, uint64_t size);

private:
    PosixFile file_;
    SingleReadPolicy single_read_;
    BatchReadPolicy batch_read_;
    DurabilityPolicy durability_;
};
```

## 13. 文件读取策略

### 13.1 BufferedSingleRead

- 调用 IOSyscall::PRead；
- size 为 0 时立即成功；
- 系统调用错误抛异常；
- 短读按当前具体后端语义兼容，最终再统一；
- Acquire 使用 AllocatorOwner；
- 简单包装定义在 header，允许内联到 `pread` 调用附近。

### 13.2 DirectSingleRead

- 计算 aligned offset、prefix 和 aligned size；
- 使用可注入 aligned allocator/buffer pool；
- ReadAt 将有效范围复制到 destination；
- Acquire 返回 AlignedLease；
- direct alignment 来源由 Options 或 profile config 注入；
- 保留当前 AsyncIO/UringIO 的短读和错误处理语义。

### 13.3 SequentialBatchRead

- 对小 batch 顺序调用 SingleReadPolicy；
- 不分配额外 request vector；
- 作为 libaio/io_uring 不可用时的 fallback；
- fallback 不再通过 `#define AsyncIO BufferIO` 替换整个类型。

### 13.4 LibAioBatchRead

负责：

- IOCB 准备；
- DirectIOObject/buffer 生命周期；
- partial submit；
- EINTR；
- completion drain；
- context pool 归还；
- completion 数据复制。

不负责：

- 文件打开；
- logical size；
- cache；
- fsync；
- 序列化。

### 13.5 UringBatchRead

负责：

- SQE 准备；
- batch slicing；
- direct/non-direct destination；
- submit/partial submit；
- CQE wait/drain；
- cancel/failure-safe buffer 生命周期；
- context pool；
- future registered files/fixed buffers/SQPOLL 扩展。

不负责文件 ownership、logical size、cache 和序列化。

### 13.6 DurabilityPolicy

```cpp
struct NoFlush {
    void
    AfterWrite(int) const {
    }
};

struct FsyncAfterWrite {
    void
    AfterWrite(int fd) const {
        if (fsync(fd) != 0) {
            throw ...;
        }
    }
};
```

AsyncIO 当前的每次写后 fsync 行为必须原样保留，除非另立行为变更提案。

## 14. 官方 IO Profile

内部 policy 组合只通过有限 profile 暴露：

```cpp
using MemoryIO = ByteIO<
    ContiguousBackend<HeapRegion>,
    NoCache>;

using MMapIO = ByteIO<
    ContiguousBackend<MMapRegion>,
    OptionalPageCache>;

using BufferIO = ByteIO<
    PosixFileBackend<
        BufferedSingleRead,
        SequentialBatchRead,
        NoFlush>,
    OptionalPageCache>;

using AsyncIO = ByteIO<
    PosixFileBackend<
        DirectSingleRead,
        PlatformLibaioBatchRead,
        FsyncAfterWrite>,
    OptionalPageCache>;

using UringIO = ByteIO<
    PosixFileBackend<
        ConfigurableSingleRead,
        PlatformUringBatchRead,
        NoFlush>,
    OptionalPageCache>;
```

`PlatformLibaioBatchRead`/`PlatformUringBatchRead` 在编译期选择真实 executor 或 SequentialBatchRead fallback，但 AsyncIO/UringIO 名称和参数语义保持稳定。

## 15. Cache 架构

### 15.1 CachePolicy 边界

CachePolicy 位于逻辑 ByteIO 与 backend 之间：

```text
ByteIO logical request
    ↓
CachePolicy
    ├── hit：复制或返回 pinned page
    └── miss：生成 backend ReadRequest
                     ↓
                  Backend
```

Backend 不知道 cache 是否存在。

### 15.2 NoCache

```cpp
class NoCache {
public:
    template <typename Backend>
    bool
    ReadAt(const Backend& backend, ...) const {
        return backend.ReadAt(...);
    }
};
```

要求 Release 汇编中 NoCache 调用层完全消失。

### 15.3 OptionalPageCache

运行时 cache 指针为空时，只允许一次可预测的空指针判断，与当前 BasicIO 路径相当：

```cpp
if (cache_ == nullptr) {
    return backend.ReadMany(requests, count);
}
return coordinator_.ReadMany(cache_, backend, requests, count);
```

### 15.4 Batch-aware ReadMany

目标算法：

```text
1. 验证所有逻辑请求
2. 拆分为逻辑 page fragments
3. 批量查询 cache
4. 立即复制所有 hit
5. 对 miss page 去重
6. 选择性合并相邻 miss
7. 构造 backend ReadRequest 数组
8. backend.ReadMany / SubmitReads
9. 批量插入 cache
10. 完成剩余 destination copy
```

硬性要求：cache miss 不得逐项调用 backend 单读替代原本的 io_uring/libaio 批读。

### 15.5 小批次 fast path

排序、hash 去重和合并存在固定 CPU 成本。需要：

- count 很小时直接查询/提交；
- 单页命中走无分配 fast path；
- 使用 small-vector/inline storage 保存少量 fragment；
- 只有 miss 数量和潜在合并收益达到阈值时才排序；
- 阈值由 benchmark 决定，不硬编码为未经测量的经验值。

### 15.6 StorageIdentity 与 CacheKey

```cpp
struct StorageIdentity {
    uint64_t id{0};
    uint64_t generation{0};
};

struct CacheKey {
    StorageIdentity storage;
    uint64_t logical_page_id{0};
};
```

StorageId 可以来自：

- 文件 inode/device/path version 的内部封装；
- Reader 实例分配的稳定 id；
- 远程 object id；
- 索引/DataCell namespace。

generation 在内容整体替换或绑定新版本时变化。写入局部范围仍可做范围失效。

### 15.7 Miss 合并

共享 cache 后需要防止多个线程同时读取同一 miss page：

```text
Missing → Loading → Ready
                  └→ Failed
```

第一位请求者创建 loading entry 并发起读取，其余请求者等待同一个 completion。该能力可作为 cache 第二阶段，不要求与首次 CachePolicy 抽离同时完成。

### 15.8 多级缓存扩展

本提案只要求接口可扩展：

```text
Request-local cache
    ↓ miss
Shared memory cache
    ↓ miss
Local disk cache
    ↓ miss
Remote backend
```

当前实施范围只建议单层共享内存 page cache。只有远程 Reader 路线明确后再实现本地磁盘层。

## 16. NonContinuousIO 与地址映射

NonContinuousIO 本质是地址转换层，不是新的物理存储介质。建议演进为：

```cpp
template <typename AddressMapper, typename PhysicalBackend>
class MappedBackend;
```

读取流程：

```text
Logical ReadRequest
    ↓
AddressMapper::Map
    ↓
多个 Physical ReadRequest
    ↓
PhysicalBackend::ReadMany
```

要求：

- 跨 extent 请求不逐片单读；
- 使用 batch request 直接交给底层 async executor；
- 小请求使用 inline fragment storage，减少 vector 分配；
- cache 位于逻辑地址层上方；
- extent table 在并发查询阶段只读；
- resize/extent allocation 仍要求上层独占。

迁移期可以保留 `NonContinuousIO<IOTmpl>` 名称，将内部实现逐步替换为 mapper + backend。

## 17. ExternalReaderBackend

ReaderIO 改为显式只读 backend：

```cpp
class ExternalReaderBackend {
public:
    struct Capabilities {
        static constexpr bool InMemory = false;
        static constexpr bool Writable = false;
        static constexpr bool Resizable = false;
        static constexpr bool Borrowable = false;
        static constexpr bool NativeBatchRead = true;
        static constexpr bool AsyncReadable = false;
        static constexpr bool CanBindSerializedRange = true;
    };

    void
    Bind(std::shared_ptr<Reader> reader,
         uint64_t start,
         uint64_t size);

    bool
    ReadAt(...);

    bool
    ReadMany(...);
};
```

不再需要假的 `WriteImpl`。ReaderIO 的逻辑长度由 Bind 的序列化范围确定，而不是通过 Deserialize 中的空写入累加。

未来远程 Reader 可以实现相同 backend 契约，并将 `AsyncReadable` 设为 true，提供 RemoteReadOperation、timeout、retry 和 cancel。

## 18. 序列化与反序列化

序列化逻辑从 backend 中独立为算法，但保留 ByteIO 的兼容成员函数：

```cpp
template <typename IO>
void
SerializeIO(const IO& io, StreamWriter& writer);

template <typename IO>
void
DeserializeIO(IO& io, StreamReader& reader);
```

复制型 backend：

```text
读取 logical size
Resize
按固定 buffer 从 StreamReader 读
WriteAt
```

绑定型 backend：

```text
读取 logical size
BindSerializedRange(reader, cursor, size)
Seek(cursor + size)
```

使用显式 capability：

```cpp
if constexpr (IO::Capabilities::CanBindSerializedRange) {
    BindDeserialize(...);
} else {
    CopyDeserialize(...);
}
```

必须保持二进制格式完全不变，并用旧实现写、新实现读，以及新实现写、旧实现读的交叉测试验证。

## 19. IOEnvironment 与共享资源

```cpp
struct IOEnvironment {
    Allocator* allocator{nullptr};
    PageCache* shared_cache{nullptr};
    AioContextPool* aio_pool{nullptr};
    UringContextPool* uring_pool{nullptr};
    RegisteredBufferPool* registered_buffers{nullptr};
};
```

要求：

- environment 生命周期覆盖所有 IO 和 in-flight operation；
- 可以由 IndexCommonParam 内部持有或引用；
- pool 可以进程共享，也可以索引隔离；
- 单元测试可注入 fake/failing pool；
- 不在每次读取时执行 service lookup；backend 直接保存必要指针；
- 不立即实现 RegisteredBufferPool，但预留注入位置需谨慎，避免未使用字段污染首轮接口；也可在 UringEnvironment 子结构中后续加入。

第一阶段只引入 allocator；aio/uring/cache 在对应迁移阶段接入。

## 20. IO 类型和工厂集中化

### 20.1 IOKind

IOParameter 解析后生成稳定 enum：

```cpp
enum class IOKind {
    Memory,
    BlockMemory,
    MMap,
    Buffer,
    Async,
    Uring,
    Reader,
};
```

字符串只在参数解析边界出现。DataCell 工厂不再重复比较字符串。

### 20.2 类型访问器

C++17 可使用 type tag：

```cpp
template <typename T>
struct TypeTag {
    using Type = T;
};

template <typename Visitor>
auto
VisitIOKind(IOKind kind, Visitor&& visitor);
```

使用：

```cpp
return VisitIOKind(param->io_parameter->Kind(), [&](auto tag) {
    using IO = typename decltype(tag)::Type;
    return MakeFlattenDataCell<IO>(param, common_param);
});
```

Bucket 等需要包装的工厂可以在 visitor 内转换：

```cpp
using BucketIO = NonContinuousIO<IO>;
```

### 20.3 模板代码膨胀控制

- 只实例化当前官方 IO profile；
- 重型 DataCell/IO 组合继续拆分到独立 `.cpp`；
- 不建立运行时 `IORegistry::Create()` 返回虚接口供热路径使用；
- registry/visitor 只用于冷路径选择具体模板实例；
- 监控二进制 text size 和编译时间。

## 21. 并发契约

建议明确：

```text
1. Freeze 后的 ReadAt/ReadMany/Acquire：允许并发；
2. WriteAt/Resize/Shrink：调用方提供独占同步；
3. Resize/Remap 不得与 borrowed lease 或 in-flight operation 并发；
4. 非重叠并发写是否支持由具体 backend 声明，默认不保证；
5. cache 自身线程安全；
6. aio/uring pool 自身线程安全；
7. ByteIO 不为每次读取增加 shared_mutex。
```

可以增加 Debug-only generation/active lease 检查，但 Release 路径不得因该契约新增原子计数，除非 benchmark 证明成本可接受且确有安全需求。

## 22. 错误语义

重构首轮必须行为兼容，后续再单独统一。建议最终契约：

- 逻辑范围无效：返回 false/空 Lease；
- 参数非法或 offset+size 溢出：INVALID_ARGUMENT；
- allocator 失败：NO_ENOUGH_MEMORY；
- 系统调用失败：INTERNAL_ERROR，并包含 errno；
- 短读：本地稳定存储视为 IO 错误；
- async partial submit：operation 负责 drain 已提交请求后再返回错误；
- cancel：明确区分已完成、已取消和无法取消；
- cache fill 失败：不得把未完整页面标记为 Ready。

ByteIO 统一做逻辑范围判断后，backend 不应各自实现不同的越界行为。

## 23. 性能设计约束

### 23.1 禁止进入热路径的机制

- backend 虚函数；
- 每次读取的 `std::function`；
- 同步读取的 operation 堆分配；
- borrowed read 的 shared_ptr；
- cache disabled 路径的 mutex/map；
- 每次单读构造动态 request vector；
- 每次读取运行时判断完整 IOKind；
- 为所有 backend 统一使用 type-erased deleter。

### 23.2 必须内联的简单路径

- `IsValidRange`；
- `CheckedEnd` 的成功 fast path；
- ByteIO → NoCache 转发；
- ContiguousBackend ReadAt/Acquire；
- HeapRegion/MMapRegion Data getter；
- BorrowedLease Data getter 和空析构；
- 简单 profile capability 查询。

### 23.3 不建议强制内联的复杂路径

- io_uring CQE drain；
- libaio completion；
- cache 大批次 miss planner；
- mmap remap 错误处理；
- 文件 open/close；
- 远程请求重试。

复杂代码保留一次普通直接调用通常比复制到多个调用点更有利于 instruction cache。

## 24. 性能基线与验收矩阵

### 24.1 Microbenchmark

| Backend | 场景 | 请求大小/批次 |
| --- | --- | --- |
| MemoryIO | ReadAt、Acquire | 32B、128B、512B、4KiB |
| MemoryBlockIO | 同 block、跨 block | 128B、4KiB、128KiB |
| MMapIO | 热页随机读、冷页读 | 128B、4KiB、128KiB |
| BufferIO | buffered pread | 4KiB、16KiB、128KiB |
| AsyncIO | batch direct read | batch 1、8、32、128 |
| UringIO | batch buffered/direct | batch 1、8、32、128 |
| Cache | 0%、50%、100% hit | batch 1、8、32、128 |
| NonContinuous | 单 extent、跨 extent | 1、4、16 fragments |
| ReaderIO | Read、MultiRead | 本地计数 Reader |

记录：

- ns/op；
- CPU cycles/op；
- instructions/op；
- branch/op 和 branch miss；
- allocation/op；
- syscall/op；
- submission count；
- bytes read amplification；
- P50/P95/P99；
- peak RSS；
- cache hit/miss/fill；
- 二进制 text size。

### 24.2 End-to-end benchmark

至少覆盖：

- HGraph build；
- HGraph random query；
- HGraph ReaderSet + cache query；
- RaBitQ split query；
- BucketDataCell query；
- Memory、MMap、Buffer、Async、Uring profile；
- 数据完全驻内存、部分驻内存、明显大于内存三类场景；
- 单查询低并发和多查询高并发；
- Recall 保持相同。

### 24.3 性能硬门禁

1. MemoryIO/MMapIO borrowed read：额外堆分配为 0；
2. MemoryIO/MMapIO 热路径：额外虚/间接调用为 0；
3. MemoryIO point estimate 回退不得超过 1%，且不得有统计显著回退；
4. BufferIO 单读 syscall 数不得增加；
5. AsyncIO/UringIO 同 batch 的 submission 数不得增加；
6. cache disabled 不得出现 cache 锁和 map 查找；
7. cache miss 必须保留 backend batch；
8. 同步接口不得产生 ReadOperation 堆分配；
9. HGraph/RaBitQ 端到端 QPS/P99 不得有统计显著回退；
10. 编译时间和二进制体积增长超过预设阈值时必须拆分实例化或减少 profile。

对于本地文件 IO，系统噪声可能高于 1%。验收应使用足够重复次数、置信区间、固定 CPU/NUMA、相同文件布局和相同 cache 热度，不得只比较单次结果。

### 24.4 汇编验收

为 MemoryIO 增加最小 codegen probe：

```cpp
float
ComputeOne(const MemoryIO& io, uint64_t offset) {
    auto lease = io.Acquire(offset, 128);
    return ComputeDistance(lease.Data());
}
```

Release 汇编中不得出现：

- ByteIO::Acquire 的真实 call；
- NoCache::Acquire 的真实 call；
- ContiguousBackend::Acquire 的真实 call；
- Lease 析构 call；
- allocator 调用；
- `call *%reg` 类型的间接调用。

## 25. 正确性测试

### 25.1 统一 IO Contract Test

所有支持相应 capability 的 profile 参数化覆盖：

- 空读；
- 空批次；
- 边界末端读；
- 越界读；
- offset+size 溢出；
- 覆盖写；
- append；
- sparse offset write；
- resize grow/shrink/zero；
- Direct/Acquire 生命周期；
- MultiRead 中混合零长度项；
- 重复 offset；
- destination 不连续；
- Serialize/Deserialize；
- Deserialize 覆盖更长旧内容；
- existing file reopen；
- 新建临时文件析构；
- allocator failure；
- syscall short read/failure；
- cache hit/miss/invalidate；
- 并发只读。

### 25.2 新旧实现差分测试

随机生成操作序列同时执行于旧/新实现：

```text
Write / Read / MultiRead / Resize / Shrink / Serialize / Deserialize
```

每一步比较：

- logical size；
- 可读取字节；
- 返回值或异常类别；
- need_release/Lease 数据内容；
- 文件最终内容；
- cache 开关不影响可见数据。

### 25.3 序列化交叉测试

- old write → new read；
- new write → old read；
- old ReaderIO bind → new；
- new Reader backend bind → compatibility API layer；
- 各 profile 写出的逻辑内容一致。

### 25.4 异步故障注入

- partial submit；
- EINTR；
- CQE error；
- short read；
- cancel before submit；
- cancel after partial completion；
- context pool exhausted；
- ring init ENOSYS/EPERM/EACCES；
- operation 析构时仍有 in-flight 请求；
- cache fill 时 backend 失败。

## 26. 分阶段实施计划

### Phase 0：建立基线，不改生产行为

内容：

1. 增加 IO contract test 框架；
2. 增加 microbenchmark；
3. 增加 allocation/syscall/submission 统计；
4. 建立新旧差分测试工具；
5. 记录当前各 profile Release 汇编；
6. 记录 HGraph/RaBitQ 端到端基线；
7. 文档化当前线程安全和错误语义。

预计 1–2 个独立变更批次。

退出条件：基线稳定可重复，否则不得开始架构迁移。

### Phase 1：核心类型 + MemoryIO 试点

内容：

1. `ReadRequest`；
2. `BorrowedLease`/`AllocatorLease` 基础设施；
3. `ImmediateOperation`；
4. `NoCache`；
5. `ByteIO` 最小门面；
6. `HeapRegion`；
7. `ContiguousBackend<HeapRegion>`；
8. `MemoryIO`；
9. compatibility Read/Write/MultiRead 适配；
10. 新旧 MemoryIO 差分、汇编和 benchmark。

预计 2 个独立变更批次。

继续条件：

- Memory 热路径 codegen 满足零额外 call；
- 性能无统计显著回退；
- 代码清晰度评审认可；
- 编译时间和二进制增长可接受。

任一条件失败则停止完整重构，保留可独立使用的测试基础设施。

### Phase 2：MemoryBlockIO 与 MMapIO

内容：

1. BlockMemoryBackend；
2. 同 block borrowed / 跨 block owned lease；
3. MMapRegion；
4. existing file size 初始化；
5. Linux/macOS remap；
6. 逻辑长度和物理映射容量分离；
7. profile benchmark 和差分测试。

预计 2–3 个独立变更批次。

### Phase 3：PosixFile + BufferIO

内容：

1. PosixFile RAII；
2. FileOwnership；
3. BufferedSingleRead；
4. SequentialBatchRead；
5. NoFlush；
6. BufferIO；
7. existing/new file 生命周期差分；
8. 单读、批读、序列化 benchmark。

预计 2 个独立变更批次。

该阶段用于验证文件 backend policy 组合，不同时迁移 cache 或异步 executor。

### Phase 4：AsyncIO

内容：

1. DirectSingleRead；
2. AlignedLease；
3. LibAioBatchRead；
4. FsyncAfterWrite；
5. IOEnvironment 中的 AioContextPool；
6. 无 libaio fallback policy；
7. AsyncIO 差分和性能测试。

预计 2 个独立变更批次。

### Phase 5：UringIO

内容：

1. ConfigurableSingleRead；
2. UringBatchRead；
3. UringReadOperation 基础；
4. pool 注入；
5. fallback policy；
6. 现有错误恢复迁移；
7. direct/non-direct benchmark；
8. 为 future registered buffers/files 保留内部扩展点。

预计 2–3 个独立变更批次。

首轮不要求实现 fixed buffer、SQPOLL 或真正查询流水线。

### Phase 6：CachePolicy 抽离

内容：

1. NoCache/OptionalPageCache 正式接管 BasicIO cache；
2. StorageIdentity/CacheKey；
3. batch-aware hit/miss 分类；
4. miss 继续调用 backend ReadMany；
5. PageLease 或兼容 owned copy；
6. shared cache 分区和 page id 迁移；
7. cache hit rate、P99 和 contention benchmark。

预计 2–3 个独立变更批次。

这是最高风险阶段，必须与 backend 迁移分开评审。

### Phase 7：Reader、序列化与 NonContinuous

内容：

1. ExternalReaderBackend；
2. BindSerializedRange；
3. 序列化算法迁移；
4. ExtentMapper/MappedBackend；
5. NonContinuousIO compatibility；
6. ReaderSet + cache 回归；
7. baseline/candidate serialization cross-test。

预计 2–3 个独立变更批次。

### Phase 8：工厂和 DataCell 调用点

内容：

1. IOKind；
2. VisitIOKind；
3. flatten/bucket/graph/rabitq 工厂迁移；
4. 内部计算热点从 need_release 迁移到 Lease；
5. 保留公共接口兼容层；
6. 监控模板实例化和二进制体积。

预计 2–4 个独立变更批次。

### Phase 9：删除旧实现

前置条件：

- 所有 profile 已迁移；
- 全量测试和 benchmark 门禁持续通过；
- 至少一个版本周期无 fallback 问题；
- 公共/内部兼容接口评审完成。

删除：

- BasicIO CRTP；
- `*Impl` 命名体系；
- 内部 need_release 调用；
- fallback 宏；
- 重复工厂分支；
- compatibility adapter。

预计 1–2 个独立变更批次。

## 27. 建议的变更粒度

每个变更批次应满足：

- 只迁移一个清晰能力或一个 backend；
- 新旧实现可同时测试；
- 不混合行为修复和架构迁移；
- 包含对应 contract test；
- 包含对应 benchmark 或基线对比；
- 提供代码体积/编译时间变化；
- 可以独立回滚。

不建议出现“新增全部 core 类型 + 迁移五个 backend + 删除 BasicIO”的大型单次变更。

## 28. 风险与缓解

### 28.1 抽象层次增加导致性能回退

缓解：静态组合、NoCache 特化、BorrowedLease、汇编门禁、MemoryIO 先行试点。

### 28.2 ReadLease 过重

缓解：按 backend/cache policy 选择具体 Lease，禁止通用 `std::function` deleter；Memory/MMap 使用 EBO 空 owner。

### 28.3 异步 operation 生命周期错误

缓解：operation move-only、明确析构语义、故障注入、partial submit drain、禁止 in-flight buffer 提前释放。

### 28.4 Cache planner 小批次变慢

缓解：小批次 fast path、inline fragment storage、阈值 benchmark、只对有收益的 miss 启用排序/合并。

### 28.5 模板实例化膨胀

缓解：有限官方 profile、重型显式实例化、独立 translation unit、监控 text size 和编译时间。

### 28.6 序列化兼容破坏

缓解：baseline/candidate 交叉测试，迁移期保持格式和字段顺序不变，Reader bind 只改变运行时实现。

### 28.7 平台差异

缓解：macOS/Linux 分别覆盖 mmap；libaio/io_uring 仅在支持平台启用；fallback profile 保持稳定类型。

### 28.8 迁移范围失控

缓解：Phase 1 设硬退出点；远程 Reader、多级缓存、fixed buffers 和查询流水线不进入首轮。

## 29. 被否决的替代方案

### 29.1 在 BasicIO 上继续增加 CRTP 中间层

例如 FileBasedIO/ContiguousIO。

否决原因：

- Async/Uring 的单读、对齐、释放和错误语义并不完全相同；
- 文件资源和访问策略仍混合在继承层次；
- cache 和异步批读的组合问题未解决；
- need_release 协议未解决；
- Reader/远程只读能力仍需伪装完整 IO；
- 继承层次加深但职责没有真正分离。

低层 Region 或 Policy 的复用是合理的，但不建议增加新的“大 IO 基类”。

### 29.2 统一运行时虚接口

否决原因：

- Memory/MMap 热路径增加间接调用；
- 阻碍内联；
- ReadLease/Operation 容易被迫 type erase；
- DataCell 当前已经模板化具体 IO，没有必要退化成运行时多态。

运行时选择只允许发生在冷路径工厂，选择后进入具体模板实例。

### 29.3 一次性重写

否决原因：

- 无法定位性能变化来源；
- cache、backend、序列化、DataCell 同时变化时回归风险过高；
- review 和回滚困难；
- libaio/io_uring/macOS 环境无法在单一开发机完整覆盖。

### 29.4 向用户暴露任意策略组合

否决原因：

- 配置复杂；
- 存在无意义或不安全组合；
- 模板和二进制膨胀；
- 测试矩阵不可控。

内部组合、外部 profile 是更合适的平衡。

## 30. 预期收益

### 30.1 近期收益

- 建立统一 IO contract 和性能基线；
- 消除内部 need_release 手工释放；
- 明确范围、所有权和并发契约；
- 文件资源错误回滚集中化；
- IO 类型分发集中化；
- cache 开启后保留批读能力；
- 后端测试和故障注入更容易。

### 30.2 中长期收益

- io_uring fixed buffers/registered files 只修改 Uring executor；
- 真正异步读取复用 ReadRequest/ReadOperation；
- 搜索算法可逐步采用 SubmitReads + Wait/Poll 流水线；
- 共享缓存可以批量处理 hit/miss；
- 远程 Reader 复用 cache、async、request planner 和 ownership；
- buffered/direct/uring/read-ahead 等策略不再要求复制完整 IO 类；
- 新 IO profile 不再遗漏多个 DataCell 工厂。

### 30.3 不承诺的收益

- 不承诺所有现有场景立即提速；
- 不承诺总代码行数立刻下降；
- 不承诺编译时间下降；
- 不承诺仅替换架构即可获得真正异步查询收益；算法仍需单独流水线改造。

Memory/MMap/Buffer 首要目标是性能持平和结构清晰。直接性能提升主要可能来自 cache + batch、miss 去重、buffer pool、请求合并和后续 IO/计算重叠。

## 31. 投入估算

完整迁移粗略估算：

- 6–10 周有效开发时间；
- 15–20 个可独立审查的变更批次；
- 修改约 40–70 个文件；
- 新增/迁移约 4,000–8,000 行代码和测试；
- 需要 Linux libaio/io_uring、macOS mmap 和真实 NVMe benchmark 环境。

首轮试点估算：

- Phase 0 + Phase 1；
- 2–3 周；
- 3–4 个变更批次；
- 不改默认生产 IO 路径；
- 完成后可明确决定继续或停止。

## 32. 决策检查点

### Checkpoint A：是否批准试点

建议批准条件：

- 中长期路线至少可能包含共享缓存、深化 io_uring、真正异步或远程 Reader 中的两项；
- 团队愿意维护性能基线；
- 可以提供真实 Linux NVMe 测试环境；
- 接受迁移期新旧代码并存。

### Checkpoint B：MemoryIO 后是否继续

必须同时满足：

1. 汇编无额外热路径调用；
2. microbenchmark 无显著回退；
3. DataCell/测试使用 Lease 后代码更清晰；
4. 编译时间和二进制增长可接受；
5. 没有为了统一接口引入复杂 type erasure。

### Checkpoint C：BufferIO 后是否迁移异步后端

必须满足：

1. PosixFile 生命周期和行为完全兼容；
2. 单读 syscall 和性能无回退；
3. policy 组合比原完整类更清晰；
4. fallback 设计稳定。

### Checkpoint D：CachePolicy 后是否删除 BasicIO

必须满足：

1. cache hit/miss 正确；
2. cache miss 保留批量执行；
3. 共享 cache key 无碰撞和版本问题；
4. contention/P99 可接受；
5. 所有 DataCell 兼容测试通过。

## 33. 最终验收条件

完整重构只有在以下条件全部满足时才视为完成：

1. 现有所有 IO profile 对外配置保持兼容；
2. 序列化格式保持兼容；
3. 所有 IO contract test 通过；
4. baseline/candidate 差分测试通过；
5. Linux/macOS 相关测试通过；
6. Memory/MMap 热路径无新增间接调用和分配；
7. Buffer/Async/Uring 性能无显著回退；
8. cache miss 继续使用 backend batch；
9. 内部热点代码不再使用 need_release；
10. ReaderIO 不再依赖假 WriteImpl；
11. IO 类型分发集中；
12. BasicIO 旧架构和 compatibility adapter 被安全删除；
13. IO 并发契约形成项目文档；
14. benchmark 进入持续回归流程。

## 34. 推荐结论

不建议直接批准完整重写并立即迁移全部后端。建议将本提案作为长期目标架构，只批准 Phase 0 和 Phase 1：

```text
性能/正确性基线
    ↓
ReadRequest + ReadLease + ImmediateOperation
    ↓
ByteIO + HeapRegion + ContiguousBackend
    ↓
MemoryIO 基线/候选实现对比
```

MemoryIO 是最严格的零开销试金石。如果该试点无法证明生成代码、运行性能和维护性同时改善，应停止后续架构迁移；如果试点成立，再按 Buffer → Async/Uring → Cache → Reader/NonContinuous → Factory/DataCell 的顺序逐步推进。

该策略使团队能够以较小前期投入验证核心假设，同时保留最终支持深化 io_uring、真正异步读取、batch-aware 共享缓存、远程 Reader 和更多访问策略的架构空间。

## 35. 相关提案

- `P5建议-2026-06-01-重构-io层抽象统一basicio公共骨架.md`：早期 BasicIO 骨架建议；本提案采用组合式 Backend/Policy，而不是继续加深 IO 继承层次。
- `P5建议-2026-06-08-架构-IO层缺乏线程安全契约-datacell依赖隐式保证.md`：本提案第 21 节吸收并明确 IO 并发契约。
- `2026-08-04-增强-hgraph-readerset-反序列化-read-cache-弹性.md`：ReaderSet 与共享 read cache 的现有需求。

## 36. 实施结果与最终架构

本设计已在 Linux 环境中完整落地。最终实现采用静态组合：

```text
ByteIO<Backend, CachePolicy>
    ├── Memory / MemoryBlock / MMap / PosixFile / Reader backend
    ├── libaio / io_uring batch-read policy
    ├── NoCache / OptionalPageCache
    └── NonContinuousBackend<PhysicalIO>
```

关键实施结果：

1. 新架构已直接使用正式 IO 名称，旧 BasicIO、旧后端实现和旧 io_uring helper 已删除。
2. DataCell 与 Layout 不再持有 `BasicIO<IOTmpl>`，热点读取使用 `ReadLease` 表达所有权。
3. OptionalPageCache 支持 private/shared namespace、single-flight、批量 miss 和失效；同步单页
   保持 streaming fill，异步多页 miss 保持 backend batch。
4. NonContinuousIO 已成为地址映射 backend，128 个 physical fragments 以内使用 inline request
   storage，热路径不产生项目 allocator 分配，超限时安全 spill。
5. ReaderIO 使用显式 ExternalReaderBackend，配置、序列化和 compatibility MultiRead 边界行为兼容。
6. libaio 与 io_uring 的 partial submit、EINTR、completion error、short completion 和 fallback
   恢复均由独立探针覆盖。

最终代码已 rebase 到上游 `b3fd925e58ff745bff02e0904c2fcf46ca12ee2e`；由于之后新增的
上游提交只涉及构建性能与并发，精确运行时验收继续以
`06ab479ec638a6f07df8dbd5cd21dfad221e2fe0` 为对照。默认 Debug
非长测套件 825 cases / 85,379,810 assertions 通过；最终快路径修正后的 no-aio、liburing、
ASan/UBSan、TSan 定向矩阵再次通过；syscall、allocation 和 submission count 未增加。跨版本与
端到端实验未发现稳定性能退化，HGraph async read-cache 与 RaBitQ hybrid 获得显著加速，
SINDIV2 async 与基线持平。验收明确排除 `[daily]` 与 `[tune]`，由定向测试、sanitizer、构建矩阵、
故障探针和固定 CPU 多样本基准覆盖本次改动风险。

Linux 最终定向覆盖率矩阵 49 cases / 125,145 assertions 通过，`src/io` 行覆盖率为
59.9%，函数覆盖率为 50.1%。本次重构的构建、测试和权威性能验收均在
`lht.dev` 的 Linux 工作环境内完成。

定时性能工作流已启用三组有界 IO benchmark，采集 MemoryIO、backend profiles 和
NonContinuousIO 的 11 样本 CSV 并上传为构建产物。共享 runner 不设置易抖动的硬阈值；发现趋势
变化后仍使用固定 CPU、相同输入和 paired samples 做精确判定。至此第 33 节的 14 项最终验收条件
均有对应实现或验证证据。
