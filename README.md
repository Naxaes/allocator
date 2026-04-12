# Allocators

This project currently includes:

- `SystemAllocator`
- `StackAllocator`
- `PoolAllocator`
- `SlabAllocator`

## Alignment semantics

Allocator alignment settings are expressed as a base-2 exponent, not raw bytes.

- `alignment = 4` means `2^4 = 16` byte alignment
- `alignment = 5` means `2^5 = 32` byte alignment
- `alignment = 0` means "use the allocator default alignment"

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run the allocator test suite

```sh
ctest --test-dir build --output-on-failure -V -R '^allocators\.'
```

## Run analyzer-backed tests

```sh
cmake --build build --target analyze
```

## Run the benchmark

For more meaningful timing numbers, prefer a release build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target allocators_benchmark
./build-release/allocators_benchmark
```

You can optionally pass the requested operation count:

```sh
./build-release/allocators_benchmark 500000
```

You can also benchmark multiple payload sizes automatically and collect repeated samples:

```sh
./build-release/allocators_benchmark --operations 500000 --sizes 8,16,32,64,128,256 --warmup 1 --repeat 7
```

Structured output is available as CSV or JSON:

```sh
./build-release/allocators_benchmark --format csv --output benchmark.csv
./build-release/allocators_benchmark --format json --output benchmark.json
```

If you want the individual timed repeats in machine-readable output, add `--raw-samples`:

```sh
./build-release/allocators_benchmark --format json --raw-samples --output benchmark.json
./build-release/allocators_benchmark --format csv --raw-samples --output benchmark.csv
```

You can turn benchmark JSON into a standalone SVG chart with the bundled plotting script:

```sh
python3 scripts/plot_benchmark_results.py benchmark.json benchmark.svg
python3 scripts/plot_benchmark_results.py benchmark.json benchmark.svg median_ns
```

## One-command benchmark report target

You can also have CMake build the benchmark, export text/CSV/JSON artifacts, and generate an SVG automatically:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target benchmark_report
```

Artifacts are written to:

```text
build-release/benchmark/benchmark.txt
build-release/benchmark/benchmark.csv
build-release/benchmark/benchmark.json
build-release/benchmark/benchmark.svg
```

You can customize the target inputs at configure time:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DALLOCATORS_BENCHMARK_OPERATIONS=1000000 \
  -DALLOCATORS_BENCHMARK_SIZES=16,32,64,128 \
  -DALLOCATORS_BENCHMARK_WARMUP=1 \
  -DALLOCATORS_BENCHMARK_REPEAT=7 \
  -DALLOCATORS_BENCHMARK_PLOT_METRIC=median_ns
```

## Test binaries

- `allocators_tests` – positive allocator behavior checks
- `allocators_tests_sanitized` – positive tests under AddressSanitizer and UndefinedBehaviorSanitizer
- `allocators_tests_leakcheck` – positive tests used by macOS `leaks`
- `allocators_misuse_tests` – misuse scenarios that are expected to fail
- `allocators_benchmark` – simple timing comparison for system, stack, pool, and slab allocators

Benchmark reports now include both:

- `direct malloc/free` – raw libc allocation baseline
- `allocator api system` – the same underlying system allocator reached through this allocator API

## Covered behaviors

- system allocation, reallocation, and deallocation
- stack allocator aligned allocation and null-on-OOM behavior
- pool allocator fixed-size allocation, exhaustion, and slot reuse
- slab allocator multi-slab growth and deallocation
- misuse paths for unsupported reallocation/deallocation and invalid requests

