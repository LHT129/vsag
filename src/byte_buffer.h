
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

#include "vsag/allocator.h"

namespace vsag {
class ByteBuffer {
public:
    ByteBuffer(uint64_t size, Allocator* allocator) : allocator_(allocator) {
        data_ = static_cast<uint8_t*>(allocator_->Allocate(size));
    }

    ~ByteBuffer() {
        allocator_->Deallocate(data_);
    }

    uint8_t*
    GetData() {
        return data_;
    }

    const uint8_t*
    GetData() const {
        return data_;
    }

public:
    uint8_t* data_;

    Allocator* const allocator_;
};
}  // namespace vsag
