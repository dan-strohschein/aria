# Contributing to Aria

Thanks for your interest in contributing to Aria! This is an early-stage language and we welcome bug reports, test cases, and code contributions.

## Building the Compiler

Aria is a self-hosting compiler — it compiles its own source code. To build it, you need the bootstrap compiler first.

### Prerequisites

- Go 1.21+ (for the bootstrap compiler)
- clang (for LLVM IR compilation)
- macOS ARM64 or Linux x86_64/ARM64

### Build Steps

```bash
# 1. Clone both repos
git clone https://github.com/dan-strohschein/aria-compiler-go.git
git clone https://github.com/dan-strohschein/aria.git

# 2. Build the bootstrap compiler
cd aria-compiler-go
go build -o aria ./cmd/aria
cd ../aria

# 3. Build the self-hosting compiler (gen0)
../aria-compiler-go/aria build src/

# 4. The compiler is now at src/aria_generated
# You can self-compile (gen1):
./src/aria_generated build src/
```

**Important:** When building the compiler from source, the bootstrap compiler handles file ordering automatically. The `diagnostic.aria` module must be compiled early in the pipeline.

## Running Tests

```bash
# Run the full test suite (161 tests)
bash testdata/programs/run_tests.sh
```

The test harness reads directives from the top of each `.aria` file:
- `// test-expect: expected output` — checks stdout contains this text
- `// test-requires: feature` — skips the test (external dependency)
- `// test-files: a.aria b.aria` — multi-file test
- `// test-support: true` — support file, not run directly

## Reporting Bugs

Please file bugs on [GitHub Issues](https://github.com/dan-strohschein/aria/issues). Include:

1. The `.aria` source file that triggers the bug
2. The error message or unexpected behavior
3. What you expected to happen
4. Your platform (macOS ARM64, Linux x86_64, etc.)

## Pull Requests

1. Fork the repo and create a branch from `main`
2. Make your changes
3. Run the test suite: `bash testdata/programs/run_tests.sh`
4. Describe what your change does and why in the PR description
5. If adding a new language feature, add a test in `testdata/programs/`

## Code Style

- Aria source uses `snake_case` for functions/variables, `PascalCase` for types
- One declaration per line
- Tests use `entry` blocks with `assert` statements
- Each test file should print `"<name>_test passed"` on success

## Where to Look

| Area | Directory |
|------|-----------|
| Lexer | `src/lexer/` |
| Parser | `src/parser/` |
| Name resolver | `src/resolver/` |
| Type checker | `src/checker/` |
| Code generation | `src/codegen/` |
| Diagnostics | `src/diagnostic/` |
| C runtime | `runtime/` |
| Test programs | `testdata/programs/` |
| Language spec | [`aria-docs`](https://github.com/dan-strohschein/aria-docs) |

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
