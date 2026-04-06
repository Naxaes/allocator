# Allocators

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

## Test binaries

- `allocators_tests` – positive allocator behavior checks
- `allocators_tests_sanitized` – positive tests under AddressSanitizer and UndefinedBehaviorSanitizer
- `allocators_tests_leakcheck` – positive tests used by macOS `leaks`
- `allocators_misuse_tests` – misuse scenarios that are expected to fail

