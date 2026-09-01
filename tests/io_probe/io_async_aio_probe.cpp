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

#include <libaio.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "io/async_io/async_io.h"
#include "io/core/io_environment.h"
#include "io/core/read_request.h"
#include "vsag/allocator.h"
#include "vsag_exception.h"

using Iocb = struct iocb;
using IoEvent = struct io_event;
using Timespec = struct timespec;

namespace {

enum class Injection : uint8_t {
    None,
    SubmitEintr,
    SubmitFailure,
    PartialSubmit,
    GetEventsEintr,
    GetEventsFailure,
    ReverseCompletions,
    CompletionError,
    ShortCompletion,
};

std::atomic<Injection> injection{Injection::None};
std::atomic<bool> injected{false};
std::atomic<uint64_t> setup_calls{0};
std::atomic<uint64_t> destroy_calls{0};
std::atomic<uint64_t> submit_calls{0};
std::atomic<uint64_t> submitted_requests{0};
std::atomic<uint64_t> getevents_calls{0};
std::atomic<uint64_t> completed_events{0};

bool
TakeInjection(Injection expected) {
    if (injection.load(std::memory_order_relaxed) != expected) {
        return false;
    }
    bool was_injected = false;
    return injected.compare_exchange_strong(was_injected, true, std::memory_order_relaxed);
}

void
ResetCounters(Injection next = Injection::None) {
    injection.store(next, std::memory_order_relaxed);
    injected.store(false, std::memory_order_relaxed);
    setup_calls.store(0, std::memory_order_relaxed);
    destroy_calls.store(0, std::memory_order_relaxed);
    submit_calls.store(0, std::memory_order_relaxed);
    submitted_requests.store(0, std::memory_order_relaxed);
    getevents_calls.store(0, std::memory_order_relaxed);
    completed_events.store(0, std::memory_order_relaxed);
}

}  // namespace

extern "C" int
__real_io_setup(int maxevents, io_context_t* context);

extern "C" int
__real_io_destroy(io_context_t context);

extern "C" int
__real_io_submit(io_context_t context, long count, Iocb** control_blocks);

extern "C" int
__real_io_getevents(
    io_context_t context, long minimum, long maximum, IoEvent* events, Timespec* timeout);

extern "C" int
__wrap_io_setup(int maxevents, io_context_t* context) {
    setup_calls.fetch_add(1, std::memory_order_relaxed);
    return __real_io_setup(maxevents, context);
}

extern "C" int
__wrap_io_destroy(io_context_t context) {
    destroy_calls.fetch_add(1, std::memory_order_relaxed);
    return __real_io_destroy(context);
}

extern "C" int
__wrap_io_submit(io_context_t context, long count, Iocb** control_blocks) {
    submit_calls.fetch_add(1, std::memory_order_relaxed);
    if (TakeInjection(Injection::SubmitEintr)) {
        return -EINTR;
    }
    if (TakeInjection(Injection::SubmitFailure)) {
        return -EAGAIN;
    }
    long submitted_count = count;
    if (TakeInjection(Injection::PartialSubmit)) {
        submitted_count = std::max(0L, count - 1);
    }
    int result = __real_io_submit(context, submitted_count, control_blocks);
    if (result > 0) {
        submitted_requests.fetch_add(static_cast<uint64_t>(result), std::memory_order_relaxed);
    }
    return result;
}

extern "C" int
__wrap_io_getevents(
    io_context_t context, long minimum, long maximum, IoEvent* events, Timespec* timeout) {
    getevents_calls.fetch_add(1, std::memory_order_relaxed);
    if (TakeInjection(Injection::GetEventsEintr)) {
        return -EINTR;
    }
    if (TakeInjection(Injection::GetEventsFailure)) {
        return -EIO;
    }
    int result = __real_io_getevents(context, minimum, maximum, events, timeout);
    if (result <= 0) {
        return result;
    }
    completed_events.fetch_add(static_cast<uint64_t>(result), std::memory_order_relaxed);
    if (injection.load(std::memory_order_relaxed) == Injection::ReverseCompletions and
        not injected.exchange(true, std::memory_order_relaxed)) {
        std::reverse(events, events + result);
    } else if (injection.load(std::memory_order_relaxed) == Injection::CompletionError and
               not injected.exchange(true, std::memory_order_relaxed)) {
        events[0].res2 = EIO;
    } else if (injection.load(std::memory_order_relaxed) == Injection::ShortCompletion and
               not injected.exchange(true, std::memory_order_relaxed)) {
        events[0].res = 0;
    }
    return result;
}

namespace vsag {
namespace {

class ProbeAllocator : public Allocator {
public:
    std::string
    Name() override {
        return "AsyncAioProbeAllocator";
    }

    void*
    Allocate(uint64_t size) override {
        return std::malloc(size);
    }

    void
    Deallocate(void* data) override {
        std::free(data);
    }

    void*
    Reallocate(void* data, uint64_t size) override {
        return std::realloc(data, size);
    }
};

[[noreturn]] void
Fail(const std::string& message) {
    std::cerr << "FAIL," << message << '\n';
    std::exit(1);
}

void
Require(bool condition, const std::string& message) {
    if (not condition) {
        Fail(message);
    }
}

std::vector<uint8_t>
MakeData(uint64_t size) {
    std::vector<uint8_t> data(size);
    for (uint64_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(i * 53 + 17);
    }
    return data;
}

struct ScatterBatch {
    explicit ScatterBatch(uint64_t count, uint64_t data_size)
        : destinations(count), requests(count) {
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t size = i % 31 + 1;
            uint64_t offset = (i * 7919 + i * i * 17) % (data_size - size + 1);
            destinations[i].resize(size);
            requests[i] = {destinations[i].data(), offset, size};
        }
    }

    void
    Verify(const std::vector<uint8_t>& data) const {
        for (uint64_t i = 0; i < requests.size(); ++i) {
            Require(std::memcmp(destinations[i].data(),
                                data.data() + requests[i].offset,
                                requests[i].size) == 0,
                    "scatter content mismatch at request " + std::to_string(i));
        }
    }

    std::vector<std::vector<uint8_t>> destinations;
    std::vector<ReadRequest> requests;
};

void
RunSuccessful(AsyncIO& io,
              const std::vector<uint8_t>& data,
              Injection mode,
              const std::string& label,
              uint64_t count = 37) {
    ScatterBatch batch(count, data.size());
    ResetCounters(mode);
    Require(io.ReadMany(batch.requests.data(), batch.requests.size()), label + " returned false");
    Require(injected.load(std::memory_order_relaxed) or mode == Injection::None,
            label + " injection was not exercised");
    Require(submitted_requests.load(std::memory_order_relaxed) == count,
            label + " changed the submitted request count");
    Require(completed_events.load(std::memory_order_relaxed) == count,
            label + " changed the completion count");
    batch.Verify(data);
    std::cout << "PASS," << label << ',' << submit_calls.load() << ',' << submitted_requests.load()
              << ',' << getevents_calls.load() << ',' << completed_events.load() << '\n';
}

void
RunFailureThenRecovery(AsyncIO& io,
                       const std::vector<uint8_t>& data,
                       Injection mode,
                       const std::string& label) {
    ScatterBatch batch(17, data.size());
    ResetCounters(mode);
    bool threw = false;
    try {
        (void)io.ReadMany(batch.requests.data(), batch.requests.size());
    } catch (const VsagException&) {
        threw = true;
    }
    Require(threw, label + " did not throw");
    Require(injected.load(std::memory_order_relaxed), label + " injection was not exercised");
    uint64_t failed_submit_calls = submit_calls.load(std::memory_order_relaxed);
    uint64_t failed_submitted = submitted_requests.load(std::memory_order_relaxed);
    uint64_t failed_completed = completed_events.load(std::memory_order_relaxed);
    uint64_t failed_destroy = destroy_calls.load(std::memory_order_relaxed);
    uint64_t failed_setup = setup_calls.load(std::memory_order_relaxed);

    RunSuccessful(io, data, Injection::None, label + "_recovery", 17);
    std::cout << "PASS," << label << ',' << failed_submit_calls << ',' << failed_submitted << ','
              << failed_completed << ',' << failed_destroy << ',' << failed_setup << '\n';
}

}  // namespace
}  // namespace vsag

int
main() {
    using namespace vsag;
    ProbeAllocator allocator;
    std::string suffix = std::to_string(static_cast<uint64_t>(getpid()));
    std::string path = "/tmp/vsag_async_aio_submission_" + suffix;
    std::filesystem::remove(path);

    auto data = MakeData(4 * 1024 * 1024 + 113);
    IOContextPool pool(1, &allocator);
    IOEnvironment environment{&allocator, &pool};
    AsyncIO io(path, environment);
    io.WriteAt(0, data.data(), data.size());

    std::cout << "result,case,submit_calls,submitted,getevents_or_completed,extra1,extra2\n";
    RunSuccessful(io, data, Injection::None, "normal_sliced", 237);
    Require(submit_calls.load(std::memory_order_relaxed) == 3,
            "normal batch was not sliced at 100 requests");
    RunSuccessful(io, data, Injection::ReverseCompletions, "reverse_completions", 100);
    RunSuccessful(io, data, Injection::SubmitEintr, "submit_eintr");
    Require(submit_calls.load(std::memory_order_relaxed) == 2,
            "EINTR did not retry io_submit exactly once");
    RunSuccessful(io, data, Injection::GetEventsEintr, "getevents_eintr");
    RunFailureThenRecovery(io, data, Injection::SubmitFailure, "submit_failure");
    RunSuccessful(io, data, Injection::PartialSubmit, "partial_submit");
    Require(submit_calls.load(std::memory_order_relaxed) == 2,
            "partial submit did not submit the remaining request");
    RunFailureThenRecovery(io, data, Injection::CompletionError, "completion_error");
    RunFailureThenRecovery(io, data, Injection::ShortCompletion, "short_completion");
    RunFailureThenRecovery(io, data, Injection::GetEventsFailure, "getevents_failure");
    return 0;
}
