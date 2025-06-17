
// Copyright 2024-present the vsag project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <string>

namespace vsag {

class DirectIOObject {
public:
    DirectIOObject() = default;

    DirectIOObject(uint64_t size, uint64_t offset) {
        this->Set(size, offset);
    }

    virtual ~DirectIOObject() = default;

    void
    Set(uint64_t size, uint64_t offset) {
        this->size_ = size;
        this->offset_ = offset;
        if (align_data_) {
            free(align_data_);
        }
        auto new_offset = (offset_ >> ALIGN_BIT) << ALIGN_BIT;
        auto inner_offset = offset_ & ALIGN_MASK;
        auto new_size = (((size_ + inner_offset) + ALIGN_MASK) >> ALIGN_BIT) << ALIGN_BIT;
        this->align_data_ = static_cast<uint8_t*>(std::aligned_alloc(ALIGN_SIZE, new_size));
        this->data_ = align_data_ + inner_offset;
        this->size_ = new_size;
        this->offset_ = new_offset;
    }

    void
    Release() {
        free(this->align_data_);
        this->align_data_ = nullptr;
        this->data_ = nullptr;
    }

    uint64_t
    GetSize() const {
        return size_;
    }

    uint64_t
    GetOffset() const {
        return offset_;
    }

    uint8_t*
    GetData() const {
        return data_;
    }

    uint8_t*
    GetAlignData() const {
        return align_data_;
    }

public:
    static constexpr int64_t ALIGN_BIT = 9;

    static constexpr int64_t ALIGN_SIZE = 1 << ALIGN_BIT;

    static constexpr int64_t ALIGN_MASK = (1 << ALIGN_BIT) - 1;

private:
    uint8_t* data_{nullptr};
    uint64_t size_{0};
    uint64_t offset_{0};
    uint8_t* align_data_{nullptr};
};
}  // namespace vsag
