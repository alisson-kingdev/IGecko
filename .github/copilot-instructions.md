# Gecko Workspace Instructions

## What this repository is
A modern C++20 interpreter project for the `.gk` scripting language, featuring a dedicated package manager (`gpm`).

Key components:
- `src/cli/main.cpp` — Interpreter entrypoint (`gecko`)
- `src/cli/gpm/` — Package manager entrypoint and logic (`gpm`)
- `src/lexer` — Modern tokenization with template string support
- `src/parser` — AST construction (C++20 recursive descent)
- `src/interpreter` — Runtime execution with module support
- `src/ast` — AST node definitions (Visitor Pattern)
- `src/stdlib` — Core language libraries
- `src/utils` — Logging and shared utilities
- `scripts/` — Optimized automation scripts (build, test, install, format, tidy)

## Build and run
Use the provided scripts or CMake with Ninja (preferred):

```bash
# Full build
./scripts/build.sh

# Run a script
./build/bin/gecko examples/00_hello.gk

# Run package manager
./build/bin/gpm help
```

## Important conventions
- **C++20 Mandatory**: Use modern features (concepts, ranges, etc.) where appropriate.
- **Strict Styling**: Always follow `.clang-format` and `.clang-tidy`.
- **Safety First**: NEVER remove existing features or language constructs unless explicitly requested.
- **Validation**: Every change MUST be verified using `./scripts/test.sh` and `ctest`.
- **Global Installation**: On Termux, use `./scripts/install-in-termux.sh`.

## Mandatory Agent Protocol (AGENTS.md)
All AI agents MUST follow the protocols defined in `AGENTS.md`. This includes:
- Verification of regressions before every "finish".
- No "silent" removals of code or documentation.
- Adherence to the established architecture.

## Good tasks for the assistant
- Extend the `gecko` language with new expressions or statements.
- Enhance `gpm` with new package management capabilities.
- Fix bugs in the lexer, parser, or interpreter.
- Improve standard library (`src/stdlib`) functionality.
- Optimize the build system and automation scripts.

## Prompt examples
- "Add a new `foreach` loop to the Gecko language, updating the lexer, parser, and interpreter."
- "Implement a `download` command in `gpm` to fetch remote resources."
- "Analyze `Interpreter.cpp` for memory leaks or performance bottlenecks."
- "Update the CI workflow to include clang-tidy and address any new warnings."
