# IO Refactor Validation Log

## Environment

- Host: `lht.dev`
- Worktree: `/home/tianlan.lht/code/workdir/2026-08-21-重构-IO层为可组合高性能存储访问架构`
- Branch: `refactor/io-composable-architecture`
- Current upstream code baseline: `5096a0a6`
- Exact runtime performance baseline: `06ab479ec638a6f07df8dbd5cd21dfad221e2fe0`
- OS: Linux 5.10, x86_64
- CPU: Intel Xeon Platinum 8269CY, 26 physical cores available to the container
- Compiler: GCC 11.4.0
- Build system: CMake 3.22.1, GNU Make
- Formatting and lint: clang-format 15, clang-tidy 15
- Default IO capabilities: libaio enabled; liburing disabled by the default build option

The phase-by-phase tables below preserve development-stage evidence. For final acceptance after the
upstream rebase, the correctness matrix, end-to-end table and "Rebase performance details" section
supersede earlier point estimates.

The repository contains one production implementation under the established `MemoryIO`,
`MemoryBlockIO`, `MMapIO`, `BufferIO`, `AsyncIO`, `UringIO`, and `ReaderIO` names. In historical
performance tables, **baseline** means the pre-refactor executable and **candidate** means the
refactored executable measured during development; they are evidence, not two implementations kept
in the current source tree. Current continuous benchmarks exercise only the production types and
distinguish compatibility API calls from canonical API calls where that distinction is useful.

## Dependency setup

The host cannot reliably download GitHub archives during configuration. Builds use the repository-supported `VSAG_FETCHCONTENT_BASE_DIR` overrides and reuse already-populated dependency source directories. The ANTLR archive is reused through `VSAG_THIRDPARTY_DOWNLOAD_DIR`.

Dependency source base:

```text
/home/tianlan.lht/code/workdir/2026-08-04-优化-read-cache-批量-aio-miss-page-回填/build/_deps
```

Download cache:

```text
/home/tianlan.lht/code/workdir/2026-08-21-重构-IO层为可组合高性能存储访问架构/build-release-baseline/.vsag-downloads
```

## Baseline builds

### Default Release

Command:

```bash
make release \
  RELEASE_BUILD_DIR=./build-release-baseline/ \
  COMPILE_JOBS=32 \
  VSAG_FETCHCONTENT_BASE_DIR=/home/tianlan.lht/code/workdir/2026-08-04-优化-read-cache-批量-aio-miss-page-回填/build/_deps
```

Result: passed. Both `vsag` and `vsag_static` reached 100%.

### Debug tests

Configuration and build command:

```bash
make test \
  COMPILE_JOBS=32 \
  VSAG_FETCHCONTENT_BASE_DIR=/home/tianlan.lht/code/workdir/2026-08-04-优化-read-cache-批量-aio-miss-page-回填/build/_deps \
  VSAG_THIRDPARTY_DOWNLOAD_DIR=/home/tianlan.lht/code/workdir/2026-08-21-重构-IO层为可组合高性能存储访问架构/build-release-baseline/.vsag-downloads \
  CASE="[io]"
```

The build passed. The repository has no `[io]` tag, so the test invocation matched no cases. IO validation is run with the actual tags below.

## Baseline IO correctness

Command:

```bash
./build/tests/unittests -d yes \
  "[MemoryIO],[MemoryIOParameter],[MemoryBlockIO],[MemoryBlockIOParameter],[MMapIO],[MMapIOParameters],[BufferIO],[BufferIOParameters],[AsyncIO],[AsyncIOParameters],[UringIO],[UringIOParameters],[ReaderIO],[ReadCache],[PageCache],[LRUPageCache],[NonContinuousIO],[IOArray]" \
  --allow-running-no-tests
```

Result: passed, 57 test cases and 560453 assertions.

Representative debug timings from the successful run:

| Test | Time |
| --- | ---: |
| AsyncIO Read And Write | 1.255 s |
| BufferIO Read & Write | 0.064 s |
| MemoryBlockIO Read and Write | 0.279 s |
| MemoryIO Read and Write | 0.062 s |
| MMapIO Read & Write | 0.063 s |
| NonContinuousIO Basic Test | 0.400 s |
| IOArray AsyncIO Basic Test | 7.097 s |

These are correctness-run wall times, not performance acceptance measurements.

## Required build matrix

The final implementation must pass all of the following configurations:

1. Default Debug tests: libaio enabled, liburing disabled.
2. Default Release: libaio enabled, liburing disabled.
3. Release tests: optimized IO contract, differential, concurrency, and compatibility tests.
4. Explicit liburing Debug and Release: `ENABLE_LIBURING=ON`.
5. Feature fallback build: libaio and liburing disabled.
6. ASan and TSan focused IO suites.
7. clang-format 15 and clang-tidy 15.

## Performance gates

Baseline and final measurements must use the same compiler, flags, CPU affinity, input generation, warmup, sample count, and storage path.

- MemoryIO copy read, borrowed read, and batch scatter read.
- MemoryBlockIO cross-block and same-block reads.
- MMapIO copy, borrowed, sequential, and random reads.
- BufferIO cached and cold reads.
- AsyncIO single read and batched scatter read.
- UringIO single read and batched scatter read with liburing explicitly enabled.
- Read cache hit, miss, duplicate miss, and mixed batch workloads.
- NonContinuousIO fragment batching.
- HGraph and RaBitQ end-to-end build/load/search cases that exercise each official IO profile.

No hot-path gate is accepted from one timing sample. Report median, p95, dispersion, allocation count, syscall/submission count where relevant, and generated assembly for the MemoryIO no-cache profile.

## Phase 1 validation: core types and MemoryIO

The initial composable profile contains `ByteIO`, `ReadRequest`, move-only `ReadLease`,
`ImmediateOperation`, `NoCache`, `HeapRegion`, `ContiguousBackend`, and `MemoryIO`.

Focused tests passed with 9189 assertions in 5 test cases. Coverage includes overflow-safe bounds,
scatter batches, compatibility adapters, borrowed lifetime, 2000-step randomized differential execution,
baseline/candidate serialization in both directions, and allocator failure preservation.

Release code generation for `MemoryIO::Acquire` contains only bounds checks, pointer arithmetic,
and the requested byte load. It has no virtual/indirect call, allocator call, cache call, lease
destructor call, or heap operation. The corresponding baseline probe retains an out-of-line call to
`MemoryIO::DirectReadImpl`.

Fixed-CPU 31-sample medians after the final rebuild:

| Operation | Size | Baseline median | Candidate median |
| --- | ---: | ---: | ---: |
| copy | 32 B | 16.733 ns | 15.840 ns |
| copy | 128 B | 21.489 ns | 19.674 ns |
| copy | 512 B | 58.601 ns | 56.698 ns |
| copy | 4 KiB | 353.392 ns | 352.787 ns |
| acquire | 32 B | 12.660 ns | 12.592 ns |
| acquire | 128 B | 12.606 ns | 12.628 ns |
| acquire | 512 B | 12.604 ns | 12.625 ns |
| acquire | 4 KiB | 12.526 ns | 12.556 ns |

Copy is equal or faster. Acquire differences are between -0.5% and +0.24%, change direction with
request size, and are smaller than the measured dispersion; generated code favors the candidate. No hot-loop
allocation or reallocation occurred. Phase 1 therefore passes its performance and code-generation
gate.

## Phase 2 validation: MemoryBlockIO and MMapIO

Phase 2 adds `BlockMemoryBackend`, `MMapRegion`, `MemoryBlockIO`, and `MMapIO`.

- same-block block reads use a borrowed lease without allocation;
- cross-block reads use allocator-owned leases with automatic release;
- scatter reads keep independent destinations;
- block capacity and logical length are separate;
- mmap tracks initial logical size, file size, and mapped capacity independently;
- existing file length is discovered through `fstat`;
- zero logical length retains a minimum 4 KiB physical mapping;
- Linux remap and the macOS remap fallback preserve the stable Region contract.

The isolated Debug build used a dedicated worktree-local build directory; it uses only dependency source
directories so Debug and Release dependency build metadata cannot overwrite each other.

Focused Phase 2 tests passed with 4845 assertions in 6 test cases. The joint baseline/candidate
Memory/MemoryBlock/MMap suite passed with 85661 assertions in 27 test cases. Tests cover block lease
ownership, same/cross-block scatter reads, 1200-step randomized block differential execution,
baseline/candidate serialization in both directions, existing mmap files, logical/physical resize behavior,
and backing-file ownership.

Fixed-CPU 21-sample Release medians after optimizing the cross-block state machine:

| Profile | Operation | Size | Baseline median | Candidate median |
| --- | --- | ---: | ---: | ---: |
| block | copy same block | 128 B | 24.311 ns | 22.914 ns |
| block | copy cross block | 128 B | 10.728 ns | 10.061 ns |
| block | acquire same block | 128 B | 11.985 ns | 8.891 ns |
| block | acquire cross block | 128 B | 34.661 ns | 32.986 ns |
| block | copy same block | 4 KiB | 219.192 ns | 218.381 ns |
| block | copy cross block | 4 KiB | 100.953 ns | 100.692 ns |
| block | acquire same block | 4 KiB | 11.637 ns | 8.667 ns |
| block | acquire cross block | 4 KiB | 137.108 ns | 132.417 ns |
| mmap | copy random | 128 B | 18.901 ns | 14.669 ns |
| mmap | acquire random | 128 B | 8.114 ns | 4.712 ns |
| mmap | copy random | 4 KiB | 193.720 ns | 192.253 ns |
| mmap | acquire random | 4 KiB | 7.850 ns | 4.535 ns |

Every measured Phase 2 median is equal or faster. Same-block candidate acquire performs no allocation;
baseline and candidate cross-block paths execute exactly one allocation and one deallocation per operation. The
initial 4 KiB cross-block copy regression was removed by advancing block index/offset state instead
of recomputing global offsets for every segment.

## Phase 3 validation: PosixFile and BufferIO

Phase 3 introduces a cold-path `PosixFile` RAII object, `BufferedSingleRead`,
`SequentialBatchRead`, durability policies, the statically composed `PosixFileBackend`, and
`BufferIO`.

`PosixFile` now centralizes descriptor cleanup, optional separate read/write descriptors, direct
read flags, existing-file `fstat`, truncate state, and keep/delete-on-close ownership. Failure after
opening the first descriptor closes it and removes a newly-created owned file. BufferIO uses the
official buffered/synchronous/no-flush policy combination.

Focused BufferIO tests passed with 3227 assertions in 4 test cases. The joint baseline/candidate Buffer suite
passed with 20034 assertions in 8 test cases. Coverage includes existing-file size discovery,
temporary-file ownership, owned lease lifetime, scatter reads, 800-step randomized differential
execution, bidirectional serialization compatibility, and automatic buffer release after a kernel
short read throws.

Fixed-CPU 51-sample cached Release medians:

| Operation | Size | Baseline median | Candidate median | Interpretation |
| --- | ---: | ---: | ---: | --- |
| copy | 128 B | 554.068 ns | 547.597 ns | Candidate faster |
| acquire | 128 B | 613.097 ns | 597.609 ns | Candidate faster |
| 32-request batch | 128 B | 10.834 us | 10.924 us | 0.8% delta, far below 4.5 us dispersion |
| copy | 4 KiB | 868.771 ns | 866.710 ns | equal/slightly faster |
| acquire | 4 KiB | 958.428 ns | 944.725 ns | Candidate faster |
| 32-request batch | 4 KiB | 29.141 us | 29.181 us | 0.14% delta, far below 2.6 us dispersion |

The batch differences are not statistically meaningful and change relative magnitude with request
size; single reads and acquires are equal or faster. Both implementations allocate exactly once per
Acquire and never allocate in copy or batch operations.

A linker-wrapped syscall probe exercises the compatibility and canonical API paths separately
on `BufferIO` and verifies identical kernel-call counts:

| Operation | Compatibility API path | Canonical API path |
| --- | ---: | ---: |
| one write | 1 `pwrite64` | 1 `pwrite64` |
| 100 copy reads | 100 `pread64` | 100 `pread64` |
| 100 batches of 8 | 800 `pread64` | 800 `pread64` |
| 100 acquire reads | 100 `pread64` | 100 `pread64` |

Phase 3 therefore passes its correctness, compatibility, allocation, syscall-count, and cached
microbenchmark gates. Cold-storage and end-to-end measurements remain part of final validation.

## Phase 4 validation: AsyncIO and libaio batch policy

Phase 4 adds `DirectSingleRead`, `AlignedLease`, `LibAioBatchRead`, `FsyncAfterWrite`, an
`IOEnvironment`-supplied `IOContextPool`, and the stable `AsyncIO` profile. When libaio is
disabled, the same type is composed from buffered single reads, sequential batches, and `NoFlush`;
the implementation no longer replaces the whole type with a preprocessor alias.

The libaio executor slices batches at 100 requests, skips zero-sized entries, maps completions
through `io_event::data`, and stores the final destination pointer in each pending entry. The latter
keeps completion-order independence without allocating a second destination-offset vector. A
partial submit drains every submitted request before reporting failure. Fatal `io_getevents`
errors destroy and recreate the context before it can return to the pool, preventing in-flight
buffers from being released against a live request.

Focused AsyncIO Debug tests passed with 3244 assertions in 4 test cases. The joint baseline/candidate suite
passed with 20051 assertions in 7 test cases. Coverage includes unaligned direct copy/acquire,
237-request batches crossing multiple slices, zero-sized scatter entries, 1000 randomized
differential reads, bidirectional serialization compatibility, an 8-thread shared-context-pool
workload, and short reads after external file truncation.

An isolated Debug configuration with both libaio and liburing disabled builds the complete
`unittests` target. Its baseline/candidate AsyncIO suite passed with 20050 assertions in 6 test cases,
verifying the compile-time fallback profile and serialization compatibility.

The linker-wrapped libaio probe verifies:

- 237 non-empty requests produce exactly 3 submissions in both baseline and candidate, with 237 total
  submitted requests;
- a 17-request partial submit of 16 requests drains all 16 completions before throwing;
- injected `EINTR` retries successfully;
- reversed completion order produces correct destination data;
- injected submit failure, completion error, short completion, and fatal `io_getevents` all fail
  safely and allow a subsequent successful batch;
- fatal `io_getevents` performs one context destroy and one context recreation.

Fixed-CPU 51-sample Release single-read medians (the final batch-only optimization does not affect
these paths):

| Operation | Size | Baseline median | Candidate median | Baseline p95 | Candidate p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| copy | 128 B | 81.319 us | 81.477 us | 82.629 us | 82.391 us |
| acquire | 128 B | 81.327 us | 81.738 us | 82.194 us | 82.934 us |
| copy | 4 KiB | 91.129 us | 91.353 us | 92.496 us | 92.368 us |
| acquire | 4 KiB | 90.717 us | 90.982 us | 91.702 us | 91.659 us |

Median deltas are +0.19% to +0.51%, change independently of p95, and remain below run dispersion.
Both implementations perform exactly one aligned allocation per single operation.

Fixed-CPU 101-sample Release batch medians after storing the final destination pointer in each
pending entry:

| Batch | Size | Baseline median | Candidate median | Baseline p95 | Candidate p95 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 128 B | 225.281 us | 223.627 us | 228.474 us | 226.098 us |
| 100 | 128 B | 530.575 us | 514.206 us | 533.522 us | 518.829 us |
| 32 | 4 KiB | 288.587 us | 289.166 us | 291.025 us | 295.606 us |
| 100 | 4 KiB | 641.943 us | 637.343 us | 658.780 us | 656.069 us |

Three batch profiles are faster in both median and p95. The 4 KiB x 32 result has a +0.20% median
delta; an immediately preceding 51-sample run measured the candidate 1.77% faster for the same profile, so the
small difference is not stable. Aligned allocations equal the request count for both
implementations. Submission count does not increase. Phase 4 therefore passes its correctness,
compatibility, failure-safety, fallback, allocation, submission-count, and performance gates.

## Phase 5 validation: UringIO and io_uring batch policy

Phase 5 adds `ConfigurableSingleRead`, `ConfigurableOwner`/`ConfigurableLease`,
`UringBatchRead`, injectable `UringIOContextPool`, the stable `UringReadOperation` contract, and the
official `UringIO` profile. The profile is statically composed as:

```text
ByteIO<
  PosixFileBackend<
    ConfigurableSingleRead,
    PlatformUringBatchRead,
    NoFlush>,
  NoCache>
```

When liburing is disabled, `UringIO` remains the same public type and uses the sequential buffered
fallback internally. Runtime `direct_read` selection is preserved when liburing is available.

The io_uring executor slices at the 512-entry ring size, skips zero-sized entries, maps CQEs through
their pending-read pointer, retries partial submits, drains submitted requests before reporting a
submit failure, and destroys a contaminated ring before any potentially in-flight direct buffer is
released. `ENOSYS`, `EPERM`, and `EACCES` ring initialization failures permanently select sequential
fallback; transient initialization failures retry on the next call.

The final direct/buffered implementation selects the mode once at the batch entry and instantiates a
statically specialized pending-read path. It computes direct alignment once per batch, removes the
per-request runtime mode branch and redundant fields, releases completed direct buffers as soon as
their CQE is consumed, avoids `free(nullptr)` in the later RAII destructor, and initializes newly
constructed direct buffers without a redundant release/reset sequence.

### Correctness and configuration matrix

The final Debug matrices passed:

| libaio | liburing | Uring baseline + candidate | Async baseline + candidate |
| --- | --- | ---: | ---: |
| ON | ON | 23932 assertions / 8 cases | 101961 assertions / 9 cases |
| ON | OFF | 23931 assertions / 7 cases | 101961 assertions / 9 cases |
| OFF | OFF | 23931 assertions / 7 cases | 101960 assertions / 8 cases |

Focused UringIO coverage passed with 7127 assertions in 4 cases. It covers buffered and direct
copy, acquire, and compatibility release, 1037-request batches crossing three 512-entry slices, zero-sized
entries, scatter and contiguous destinations, 1000 randomized differential reads, bidirectional
serialization, an 8-thread shared context pool, and externally truncated short reads.

### Fault injection and fallback

The linker-wrapped io_uring probe covers queue initialization/exit, submit, and the exported
`__io_uring_get_cqe` wait path. It verifies:

- 1037 requests use exactly 3 submissions in both baseline and candidate;
- reversed completions preserve destination mapping;
- `EINTR` is retried;
- partial submit retries the remaining SQEs;
- partial-then-failure drains all 36 submitted completions and destroys the ring once;
- completion error and short completion fail without corrupting a subsequent recovery read;
- fatal completion wait destroys the ring before recovery creates a new context;
- permanent fallback initializes once and performs zero ring submissions across two calls;
- transient fallback initializes twice and the second call performs one 37-request submission.

The normal 1037-request probe reports exactly 1037 waits/completions and identical baseline/candidate
submission counts.

### Performance methodology correction

Initial benchmark versions used separate baseline and candidate files. That is not sufficiently controlled
for sub-percent O_DIRECT comparisons because the two files may receive different physical extents.
The final benchmark pre-creates one shared file, opens independent baseline and candidate descriptors on that
same file, alternates measurement order on every sample, pins execution to CPU 2, and checks equal
checksums. Results below use this shared-file setup.

### Final 101-sample batch results

Buffered batches:

| Size | Batch | Baseline median | Candidate median | Delta |
| ---: | ---: | ---: | ---: | ---: |
| 128 B | 32 | 16.065 us | 15.799 us | -1.66% |
| 128 B | 100 | 49.800 us | 48.843 us | -1.92% |
| 128 B | 512 | 266.311 us | 261.279 us | -1.89% |
| 4 KiB | 32 | 32.485 us | 32.086 us | -1.23% |
| 4 KiB | 100 | 108.547 us | 107.736 us | -0.75% |
| 4 KiB | 512 | 625.331 us | 620.727 us | -0.74% |

Direct batches:

| Size | Batch | Baseline median | Candidate median | Delta |
| ---: | ---: | ---: | ---: | ---: |
| 128 B | 32 | 222.043 us | 221.970 us | -0.03% |
| 128 B | 100 | 368.382 us | 366.745 us | -0.44% |
| 128 B | 512 | 1.090 ms | 1.088 ms | -0.18% |
| 4 KiB | 32 | 263.721 us | 264.025 us | +0.12% |
| 4 KiB | 100 | 496.352 us | 492.012 us | -0.87% |
| 4 KiB | 512 | 1.602 ms | 1.599 ms | -0.16% |

All buffered medians are faster. Five direct medians are faster and the remaining +0.12% result is
far below run dispersion. Independent direct-only 101-sample runs also bounded the worst slower
median to +0.43% while changing which profile was slower. Some direct p95 samples contain host-level
outliers in both implementations; repeated runs change their direction, while median and minimum
remain aligned.

### Final 51-sample single-read results

On the shared file, buffered copy medians improve by 1.9% to 2.3%; buffered acquire is equal or
slightly faster. Direct copy/acquire medians fluctuate around equality across two independent runs:

| Operation | Size | Run A delta | Run B delta |
| --- | ---: | ---: | ---: |
| copy | 128 B | -0.08% | +1.48% |
| acquire | 128 B | -0.06% | +0.42% |
| copy | 4 KiB | -0.04% | +0.67% |
| acquire | 4 KiB | +2.73% | -1.57% |

The sign reversal and large direct-I/O p95/stddev variation show no stable implementation
regression. Equal shared-file physical layout, alternating order, equal checksums, and the batch
results provide stronger evidence than either isolated direct single run.

Buffered copy/batch operations allocate zero times; buffered acquire allocates once. Direct single
operations allocate one aligned buffer; direct batches allocate exactly once per non-empty request.
Baseline and candidate allocation counts are identical in every profile. Phase 5 therefore passes its
correctness, compatibility, failure-safety, fallback, allocation, submission-count, and performance
gates.

## Final architecture and Phase 6/7 validation

### Correctness and safety matrix

| Configuration | Result |
| --- | --- |
| Default Debug non-long suite | 825 cases / 85,379,810 assertions, PASS |
| Post-fast-path focused Debug | 24 cases / 114,378 assertions, PASS |
| Explicit liburing focused suite | 49 cases / 125,138 assertions, PASS |
| no-aio/no-uring focused suite | 48 cases / 125,144 assertions, PASS |
| ASan + UBSan | 49 cases / 125,138 assertions, PASS; no leak/address/UB report |
| TSan concurrency-focused suite | 33 cases / 117,018 assertions, PASS; no race/deadlock report |

The final no-aio run also covers the compatibility mapping from `async_io` configuration to the
buffered fallback parameter. The candidate constructor accepts that fallback explicitly and no longer
dereferences an incompatible parameter type.

### Syscall and asynchronous submission probes

- Buffer write: 1 syscall in both implementations.
- 100 buffered copies: 100 `pread` calls in both implementations.
- 100 batches of eight requests: 800 `pread` calls in both implementations.
- 100 acquire reads: 100 `pread` calls in both implementations.
- libaio: 237 requests in three submissions; all partial/failure/recovery probes pass.
- io_uring: 1,037 requests in three submissions; partial submit, EINTR, completion/short-read
  failures, permanent fallback and transient reinitialization probes pass.

### NonContinuousIO hot path

The physical request planner now uses inline storage for up to 128 fragments and spills safely for
larger batches. Project allocator calls per normal batch dropped from four to zero.

| Profile | Baseline | Current | Result |
| --- | ---: | ---: | --- |
| 128 B fragment batch | 2,295.273 ns | 2,118.784 ns | 7.7% faster |
| 4 KiB fragment batch | 7,953.431 ns | 7,959.479 ns | +0.08%, equal within dispersion |

The final suite includes a 160-fragment spill case and an all-zero batch that must not touch the
physical backend.

### End-to-end baseline/candidate validation

All comparisons use the same benchmark source linked separately against the baseline and current
libraries. Serialized indexes are produced once by the baseline implementation and loaded by both.

| Workload | Baseline | Current | Result |
| --- | ---: | ---: | --- |
| HGraph async read-cache | 1,203.409 us | 846.410 us | 29.67% faster |
| RaBitQ async | 22,600.043 us | 22,828.289 us | +1.01%; dominated by O_DIRECT tail noise |
| RaBitQ hybrid | 2,315.273 us | 1,923.664 us | 16.91% faster |
| RaBitQ memory, long-window rerun | 1,180.887 us | 1,187.346 us | +0.55%, below run dispersion |
| SINDIV2 async term/rerank | 808.714 us | 809.787 us | +0.13%, equal within dispersion |

RaBitQ direct async results are dominated by shared-host `O_DIRECT` tail noise and reverse direction
between run orders; identical checksums, submission counts and the stable memory/hybrid profiles do
not show an implementation regression.

### Rebase performance details

All final comparisons use the exact `06ab479e` detached baseline worktree. The baseline and current
executables use byte-identical benchmark sources; serialized end-to-end inputs are generated once by
the baseline and loaded by both builds.

The initial rebase benchmark found a 5%–10% BufferIO no-cache regression because the baseline `Read` and
`MultiRead` were paying the candidate logical-range validation cost before `pread`. A backend capability,
`LegacyUncheckedReadable`, now preserves the historical behavior only for the buffered synchronous
profile. Canonical `ReadAt`/`ReadMany`, Memory, MMap, Reader, Async, Uring and NonContinuous retain
the strict canonical range contract.

A 20-run fixed-CPU BufferIO stability matrix produced these median-of-medians results:

| Operation | Baseline | Current | Delta |
| --- | ---: | ---: | ---: |
| copy 128 B | 386.292 ns | 371.629 ns | -3.80% |
| copy 4 KiB | 1,025.883 ns | 1,017.985 ns | -0.77% |
| acquire 128 B | 424.236 ns | 401.130 ns | -5.45% |
| acquire 4 KiB | 1,098.698 ns | 1,049.685 ns | -4.46% |
| batch 128 B | 11,754.770 ns | 11,655.867 ns | -0.84% |
| batch 4 KiB | 32,942.458 ns | 32,960.923 ns | +0.06% |

The final 51-sample MemoryIO paired benchmark is effectively identical: copy differs by at most
0.4%, and acquire differs by at most 0.4% across 32 B, 128 B, 512 B and 4 KiB. The 101-sample
MemoryBlock rerun likewise preserves the hot same-block path; candidate acquire is about 1.1% faster.
Cross-block copy/acquire differences remain within -0.02% to +1.4%, with exactly one allocation and
one deallocation per acquire in both implementations. No extra ownership allocation was introduced.

The final baseline/candidate profile shows cache hits 3.6%–64% faster, cache-miss batches 31%–35% faster,
duplicate-miss batches 18%–37% faster, and NonContinuousIO 2.45%–21.9% faster. Individual file miss
operations fluctuate by about 0.5%–3.9% and reverse direction across runs, consistent with local
file-I/O dispersion. All checksums match.

### Static checks and binary size

`clang-format-15 --dry-run --Werror` passes for the final modified IO files. The production IO
translation unit passes clang-analyzer, bugprone and performance clang-tidy checks after excluding
the known interface-shape and template-instantiation noise checks
(`bugprone-easily-swappable-parameters`, `bugprone-narrowing-conversions`,
`bugprone-implicit-widening-of-multiplication-result`, and `performance-move-const-arg`). Release
assembly for MemoryIO Acquire contains no call, indirect call, allocator operation, or lease
destructor call.

| Artifact | Baseline | Current | Delta |
| --- | ---: | ---: | ---: |
| static library | 1,246,090,980 B | 1,285,073,870 B | +3.1% |
| shared library | 549,351,296 B | 572,759,376 B | +4.3% |

The size increase is the remaining explicit tradeoff of keeping several statically composed backend
and policy profiles. Runtime hot paths do not gain virtual dispatch, `std::function`, or extra heap
allocation.

### Latest rebase and Linux coverage validation

The authoritative performance and coverage acceptance was completed on upstream `b3fd925e`. The
exact runtime comparisons intentionally retain `06ab479e` as their performance baseline so both
executables use identical benchmark sources, compiler flags, input data, and measurement protocol.

The final Linux coverage build was rebuilt after the rebase. The same focused IO matrix, again
explicitly excluding `[daily]` and `[tune]`, passed 49 test cases and 125,145 assertions. An lcov 2.3
report restricted to `src/io` measured:

| Metric | Covered | Total | Rate |
| --- | ---: | ---: | ---: |
| Lines | 1,246 | 2,081 | 59.9% |
| Functions | 385 | 768 | 50.1% |

The branch was subsequently rebased without conflicts onto upstream `5096a0a6` on 2026-09-01. The
last upstream change after `158ad8c7` only updates dense-index lifecycle documentation and does not
touch production or benchmark code. The
post-rebase build completed successfully. The focused ReaderIO suite passed 8 cases and 1,097
assertions, and the persistent streaming hybrid-load example completed build, streaming serialize,
streaming load, and search. This rebase validation is a correctness check; the fixed-CPU performance
numbers above remain the controlled `06ab479e` baseline comparison and were not relabeled as new
measurements.

### Continuous IO performance collection

The scheduled performance workflow now enables `ENABLE_IO_BENCHMARKS` and collects three bounded
11-sample CSV datasets:

- MemoryIO compatibility/canonical API microbenchmarks;
- phase-two MemoryBlock/MMap/Buffer profiles;
- NonContinuousIO storage-profile batch measurements.

The CSV files are uploaded as a retained workflow artifact. A Linux dry run produced 57 CSV lines
across the three datasets. The workflow deliberately records trends without a hard pass/fail
threshold because shared runners are too noisy for a reliable sub-percent gate; exact regression
decisions continue to use fixed-CPU paired comparisons.

### DESIGN.md final acceptance audit

| # | Acceptance condition | Evidence | Result |
| ---: | --- | --- | --- |
| 1 | Public IO configuration compatibility | Existing type names and parameter parsing retained; compatibility tests pass | Pass |
| 2 | Serialization compatibility | Compatibility/canonical API round-trip tests and shared serialized end-to-end inputs pass | Pass |
| 3 | IO contract tests | Linux sanitizer and backend-specific focused matrices pass | Pass |
| 4 | Behavioral differential tests | Randomized byte, scatter, ownership and model-based comparisons pass | Pass |
| 5 | Linux tests | Debug, fallback, liburing, ASan+UBSan and TSan matrices pass on `lht.dev` | Pass |
| 6 | Memory/MMap hot path | Release assembly has no extra indirect call or ownership allocation | Pass |
| 7 | Buffer/Async/Uring performance | Paired benchmarks, syscall counts and submission probes show no stable regression | Pass |
| 8 | Cache miss backend batching | Unique/duplicate miss tests and submission probes preserve batch execution | Pass |
| 9 | No `need_release` in internal hot call sites | DataCell/algorithm hot reads use leases or caller buffers; compatibility signatures remain compatibility boundaries only | Pass |
| 10 | ReaderIO has no fake `WriteImpl` | ExternalReaderBackend binds explicit serialized ranges | Pass |
| 11 | Central IO type dispatch | `VisitIOKind` is the single stable cold-path visitor and has exhaustive tests | Pass |
| 12 | Old BasicIO architecture removed | No BasicIO implementation or migration-only adapter remains; public compatibility overloads delegate to the new architecture | Pass |
| 13 | Concurrency contract documented | `DESIGN.md` section 21 defines read, write, remap, cache and async-pool rules | Pass |
| 14 | Continuous benchmark regression flow | Scheduled performance workflow builds, runs and uploads bounded IO CSV samples | Pass |

### Final conclusion

The refactor and its authoritative acceptance were completed in the Linux work environment on `lht.dev`.
It meets the correctness, compatibility, sanitizer, syscall, allocation, submission and
runtime-performance gates. Focused coverage and continuous benchmark collection are also in place.
No stable performance regression was observed. The only material cost identified by final acceptance
is a roughly 3%–4% binary-size increase, which should be tracked and can be reduced later through
explicit-instantiation and profile-pruning work without changing the architecture.
