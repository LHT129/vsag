// Copyright 2024-present the vsag project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "io/core/io_utils.h"
#include "io/core/read_lease.h"
#include "io/core/read_operation.h"
#include "io/core/read_request.h"
#include "io/external_storage_io/external_storage_io_parameter.h"
#include "vsag/allocator.h"
#include "vsag/readerset.h"
#include "vsag_exception.h"

namespace vsag {

struct ExternalStorageBackendCapabilities {
    static constexpr bool InMemory = false;
    static constexpr bool RequiresInitialization = true;
    static constexpr bool CanBindSerializedRange = false;
    static constexpr bool LegacyBatchRangeThrows = false;
    static constexpr bool LegacyUncheckedReadable = false;
    static constexpr bool BorrowedReadable = false;
    static constexpr bool BatchReadable = true;
    // SubmitReads completes inline, matching the synchronous memory backend contract.
    static constexpr bool AsyncReadable = false;
    static constexpr bool Writable = true;
    static constexpr bool Resizable = true;
    static constexpr bool CanResizeForOverwrite = false;
};

class ExternalStorageBackend {
public:
    using Capabilities = ExternalStorageBackendCapabilities;
    using Lease = AllocatorLease;
    using Operation = ImmediateOperation;

    explicit ExternalStorageBackend(Allocator* allocator) : allocator_(allocator) {
    }

    [[nodiscard]] uint64_t
    InitialLogicalSize() const {
        return 0;
    }

    [[nodiscard]] uint64_t
    Initialize(const IOParamPtr& io_param, bool, uint64_t, uint64_t) {
        auto param = std::dynamic_pointer_cast<ExternalStorageIOParameter>(io_param);
        if (param == nullptr or param->reader == nullptr or param->writer == nullptr) {
            throw VsagException(ErrorType::INVALID_ARGUMENT,
                                "external_storage_io requires a non-null Reader and Writer pair");
        }
        reader_ = param->reader;
        writer_ = param->writer;
        return reader_->Size();
    }

    // The bool result is required by ByteIO's backend concept; Reader::Read failures propagate.
    [[nodiscard]] bool
    ReadAt(uint64_t offset, uint64_t size, uint8_t* destination) const {
        EnsureInitialized();
        if (size == 0) {
            return true;
        }
        reader_->Read(offset, size, destination);
        return true;
    }

    [[nodiscard]] bool
    ReadMany(const ReadRequest* requests, uint64_t count) const {
        EnsureInitialized();
        for (uint64_t i = 0; i < count; ++i) {
            if (requests[i].size > 0) {
                reader_->Read(requests[i].offset, requests[i].size, requests[i].destination);
            }
        }
        return true;
    }

    // Preserve Reader::MultiRead's contract: false reports callback failures; validation and
    // custom Reader implementations may still throw. ByteIO callers must handle both channels.
    [[nodiscard]] bool
    ReadManyContiguous(uint8_t* destination,
                       const uint64_t* sizes,
                       const uint64_t* offsets,
                       uint64_t count) const {
        EnsureInitialized();
        if (count == 0) {
            return true;
        }
        return reader_->MultiRead(destination, sizes, offsets, count);
    }

    [[nodiscard]] Operation
    SubmitReads(const ReadRequest* requests, uint64_t count) const {
        return Operation(ReadMany(requests, count));
    }

    [[nodiscard]] Lease
    Acquire(uint64_t offset, uint64_t size) const {
        if (size == 0) {
            return {};
        }
        auto* data = static_cast<uint8_t*>(allocator_->Allocate(size));
        if (data == nullptr) {
            throw VsagException(ErrorType::NO_ENOUGH_MEMORY,
                                "ExternalStorageBackend allocation failed");
        }
        AllocatorOwner owner(allocator_, data);
        // ReadAt returns true or throws; owner releases the allocation if the read fails.
        static_cast<void>(ReadAt(offset, size, data));
        return Lease(data, size, std::move(owner));
    }

    [[nodiscard]] const uint8_t*
    LegacyRead(uint64_t offset, uint64_t size, bool& need_release) const {
        if (size == 0) {
            need_release = false;
            return nullptr;
        }
        auto* data = static_cast<uint8_t*>(allocator_->Allocate(size));
        if (data == nullptr) {
            throw VsagException(ErrorType::NO_ENOUGH_MEMORY,
                                "ExternalStorageBackend allocation failed");
        }
        try {
            static_cast<void>(ReadAt(offset, size, data));
        } catch (...) {
            allocator_->Deallocate(data);
            throw;
        }
        need_release = true;
        return data;
    }

    void
    Release(const uint8_t* data) const {
        allocator_->Deallocate(const_cast<uint8_t*>(data));
    }

    void
    WriteAt(uint64_t offset, const uint8_t* source, uint64_t size) {
        EnsureInitialized();
        writer_->Write(offset, size, source);
    }

    void
    ResizePhysical(uint64_t size) {
        EnsureInitialized();
        writer_->Resize(size);
    }

    void
    ShrinkPhysical(uint64_t size) {
        ResizePhysical(size);
    }

    void
    BindSerializedRange(uint64_t, uint64_t) {
        throw VsagException(ErrorType::UNSUPPORTED_INDEX_OPERATION,
                            "external_storage_io cannot bind serialized ranges");
    }

    void
    Prefetch(uint64_t, uint64_t) {
    }

    [[nodiscard]] int64_t
    MemoryUsage(uint64_t) const {
        return 0;
    }

    [[nodiscard]] const uint8_t*
    Data() const {
        return nullptr;
    }

    [[nodiscard]] Allocator*
    AllocatorPtr() const {
        return allocator_;
    }

private:
    void
    EnsureInitialized() const {
        if (reader_ == nullptr or writer_ == nullptr) {
            throw VsagException(ErrorType::INTERNAL_ERROR,
                                "external_storage_io is not initialized");
        }
    }

    Allocator* allocator_{nullptr};
    ReaderPtr reader_;
    WriterPtr writer_;
};

}  // namespace vsag
