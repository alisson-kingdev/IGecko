# Agent Instructions

## Package Manager
Use **CMake**: `./scripts/build.sh`, `./scripts/test.sh`, `./scripts/tidy.sh`

## Commit Attribution
AI commits MUST include:
```
Co-Authored-By: (the agent model's name and attribution byline)
```

## File-Scoped Commands
| Task | Command |
|------|---------|
| Build | `./scripts/build.sh` |
| Test | `./scripts/test.sh` |
| Lint | `./scripts/tidy.sh` |
| Format | `./scripts/format.sh` |
| CTest | `ctest --test-dir build` |

## Productivity Standards (100% Efficiency)
- **Plan-Act-Validate**: NEVER `Act` without a `Plan` + `Verification Strategy`.
- **Surgical Tooling**: Parallel `grep`/`glob` + surgical `read_file` (start_line/end_line).
- **TDD Enforcement**: Every fix MUST include a new test case.
- **Minimal Output**: Text output only for critical communication.

## Advanced Security & Regression Protocols
- **Mandatory Backup**: Before starting ANY work, create a stash checkpoint: `git stash -u -m "STABLE-BEFORE-TASK"`.
- **Regression Guard**: NEVER alter existing functionality without authorization.
- **Sanitizers**: CI must build with ASan, UBSan, and TSan enabled (`-fsanitize=address,undefined,thread`).
- **Static Analysis**: Enforce `clang-tidy` (bugprone, cppcoreguidelines, readability).
- **Memory Safety**: Zero-leak policy (Valgrind check required for major changes).
- **Failsafe**: If build/test fails >3 times, stop and re-examine architectural assumptions.

## Core Architectural Protocols
- **Standards**: C++20/23 (Concepts, Ranges, Modules, Coroutines).
- **Pattern**: Strict Visitor Pattern for AST/Interpreter.
- **Memory**: RAII/Smart Pointers only (No manual `delete`).
- **I/O**: Prefer `std::format`/`std::print`.
- **Performance**: Zero-overhead abstractions; avoid heap allocations in hot paths.
- **Modern Standards**: Refer to `docs/modern-cpp-standards.md` for 50+ practices.

## Documentation
- Sync `README.md` and `docs/` on feature completion.
- Comments must explain *why* (intent), not *what* (code).
