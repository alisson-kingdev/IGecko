# IGecko Modern C++ Standards (50+ Practices)

A summary of 50+ modern C++ practices for high-performance systems development in IGecko.

## C++20/23 Language Features
- **Concepts & Constraints**: Use for template interface definitions and compile-time error checking.
- **Ranges & Views**: Prefer range-based algorithms and views over raw loops for data manipulation.
- **std::expected**: Use for functional error handling instead of exceptions in high-performance paths.
- **std::format/std::print**: Preferred over `<iostream>` or `printf` for type-safe, performant I/O.
- **Coroutines**: Use for asynchronous I/O and generator patterns.
- **Three-way comparison (`<=>`)**: Default implementation of equality and comparison operators.
- **Modules**: Prefer modules over headers for faster compilation where applicable.

## Testing & Tooling
- **CTest**: The canonical test runner. Integrate all unit/integration tests with `add_test`.
- **Sanitizers**: Build with ASan, UBSan, and TSan in CI for regression detection.
- **Clang-Tidy**: Enforce strict linting; no warnings permitted in CI.
- **CMake Presets**: Use `CMakePresets.json` for consistent build/test configurations across environments.
- **CI/CD**: Maintain clean passing status for all jobs in `.github/workflows/`.

## I/O & Resource Management
- **Smart Pointers**: `std::unique_ptr` for ownership, `std::shared_ptr` for shared access.
- **RAII**: Resource acquisition is initialization for all handles, files, and sockets.
- **Move Semantics**: Enforce by default in constructors/assignment operators for performance.
- **Const Correctness**: Apply `const`, `constexpr`, and `consteval` aggressively.

*(Full 50+ checklist available in `resources/cpp-core-guidelines`.)*
