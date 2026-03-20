# Aria — Self-Hosting Compiler

The self-hosting Aria compiler, written in Aria. It compiles Aria source code to native ARM64 macOS executables — no external toolchain required.

## What Is Aria?

Aria is a programming language designed for AI-assisted development. Its core principles:

- **Every token carries meaning** — no boilerplate, no ceremony, no semicolons
- **Strong type system** — sum types, exhaustive pattern matching, effect tracking
- **Instant compilation** — unambiguous grammar, minimal lookahead, single-pass where possible
- **Opt-in performance** — garbage collected by default, manual memory control per-block
- **No implicit behavior** — no implicit conversions, no hidden exceptions, no null

The full language specification lives in [`../aria-docs/`](https://github.com/dan-strohschein/aria-docs).

## Who Is This For?

- **Language implementers** studying how a compiler bootstraps from a Go transpiler to native code generation
- **AI tooling researchers** exploring language design optimized for LLM code generation
- **Contributors** interested in the Aria language and its compiler infrastructure

## Current Status

The compiler is in active development toward self-hosting. Here's what works today:

### Working Features

| Feature | Status |
|---------|--------|
| Lexer (full tokenization) | Done |
| Parser (recursive descent) | Done |
| Name resolution & scoping | Done |
| Type checker (inference, traits, generics) | Done |
| Native ARM64 code generation | Done |
| Mach-O executable output (macOS ARM64) | Done |
| Ad-hoc code signing (no codesign needed) | Done |
| Integer arithmetic & comparisons | Done |
| String operations (concat, equality, contains, charAt, substring) | Done |
| Struct allocation & field access | Done |
| Array operations (create, append, index, len, growth) | Done |
| String arrays with 16-byte stride | Done |
| Sum types & pattern matching | Done |
| Control flow (if/else, while, for, loop, break, continue) | Done |
| Function calls & return values | Done |
| Const declarations | Done |
| CLI argument access | Done |
| Multi-file compilation | Done |
| `aria build` command dispatch | Done |
| File I/O (read, write binary) | Done |

### Test Programs

12 test programs verify correctness: function calls, entry blocks, structs, arrays, array growth, pattern matching, string operations, string concatenation, array indexing in expressions, integration tests, and multi-file compilation.

## Architecture

### Compilation Pipeline

```
Source (.aria) → Lexer → Tokens → Resolver → Checker → Lowerer → IR → Emitter → ARM64 → Mach-O
```

Each stage is a separate module under `src/`:

| Stage | Directory | What it does |
|-------|-----------|-------------|
| Diagnostics | `src/diagnostic/` | Error codes, rendering, JSON output, diagnostic bags |
| Lexer | `src/lexer/` | Tokenization per the formal grammar |
| Parser | `src/parser/` | Recursive descent AST construction |
| Resolver | `src/resolver/` | Name resolution, scope hierarchy |
| Checker | `src/checker/` | Type inference, trait bounds, generics, effects |
| Codegen | `src/codegen/` | Lowering, ARM64 emission, Mach-O generation |
| CLI | `src/main.aria` | Command dispatch, argument parsing |

### Key Architectural Choices

**Direct native code generation.** The compiler emits ARM64 machine code directly — no LLVM, no assembler, no linker. The entire pipeline from source to executable is self-contained.

**Spill-everything register allocation.** Every IR temporary gets a stack slot. Values are loaded before use and stored after definition. Simple and correct; optimization comes later.

**Mach-O with ad-hoc signing.** Executables include a full Mach-O header with LC_MAIN, LC_UUID, LC_BUILD_VERSION, chained fixups, and an embedded ad-hoc code signature with SHA-256 page hashes. The kernel accepts these without `codesign`.

**PC-relative string addressing (ADR).** String constants are placed directly after code in the `__TEXT` segment. ADR instructions compute addresses relative to the program counter, making binaries position-independent and ASLR-compatible.

**Sentinel arrays.** All arrays use index 0 as a sentinel element. Real data starts at index 1. This simplifies bounds handling and matches the Aria runtime convention.

**Fat-pointer strings.** Strings are represented as two consecutive stack slots: a pointer and a length. String-returning functions pass both values in X0/X1.

**String arrays use 16-byte stride.** `[str]` arrays store each element as a (ptr, len) pair. The emitter distinguishes these from `[i64]` arrays via type_id tracking.

**Immutable functional state threading.** The lowerer, emitter, and other passes thread state through function return values rather than mutating shared state. Every helper returns a new `Lowerer` or `Emitter` struct.

### Project Structure

```
aria/
├── src/
│   ├── main.aria              # CLI entry point
│   ├── diagnostic/            # Error reporting (2 files, ~400 lines)
│   ├── lexer/                 # Tokenization (3 files, ~1800 lines)
│   ├── parser/                # AST construction (4 files, ~2500 lines)
│   ├── resolver/              # Name resolution (3 files, ~1500 lines)
│   ├── checker/               # Type checking (5 files, ~3000 lines)
│   └── codegen/               # Native code gen (9 files, ~4000 lines)
│       ├── arm64.aria         # ARM64 instruction encoding
│       ├── ir.aria            # IR types and module
│       ├── emit.aria          # IR → ARM64 machine code
│       ├── runtime.aria       # Runtime stubs (exit, write, alloc, memcpy, args, etc.)
│       ├── macho.aria         # Mach-O binary format
│       ├── lower.aria         # Token stream → IR
│       ├── codegen.aria       # Pipeline wiring, multi-file, patching
│       └── e2e_test.aria      # End-to-end test suite
├── testdata/programs/         # 12 test programs
├── plan.md                    # Implementation roadmap
└── CLAUDE.md                  # AI assistant guide
```

**27 source files, ~13,500 lines of Aria.**

## Building

### Prerequisites

- macOS on Apple Silicon (ARM64)
- Go 1.21+ (for the bootstrap compiler)
- The [bootstrap compiler](https://github.com/dan-strohschein/aria-compiler-go) cloned as a sibling directory

### Build with the Bootstrap Compiler

The bootstrap compiler transpiles Aria to Go, then calls `go build`:

```bash
# Build the bootstrap compiler (one-time)
cd ../aria-compiler-go
go build -o aria ./cmd/aria
cd ../aria

# Build the self-hosting compiler
../aria-compiler-go/aria build \
  src/diagnostic/diagnostic.aria \
  src/lexer/token.aria \
  src/lexer/lexer.aria \
  src/resolver/scope.aria \
  src/resolver/resolver.aria \
  src/checker/types.aria \
  src/checker/traits.aria \
  src/checker/checker.aria \
  src/codegen/arm64.aria \
  src/codegen/ir.aria \
  src/codegen/emit.aria \
  src/codegen/runtime.aria \
  src/codegen/macho.aria \
  src/codegen/lower.aria \
  src/codegen/codegen.aria \
  src/main.aria
```

This produces `./diagnostic` — the self-hosting compiler as a Go binary.

### Use the Self-Hosting Compiler

```bash
# Show help
./diagnostic

# Show version
./diagnostic version

# Build a single-file program
./diagnostic build testdata/programs/entry_test.aria
./testdata/programs/entry_test

# Build a multi-file program
./diagnostic build testdata/programs/multi_a.aria testdata/programs/multi_b.aria
./testdata/programs/multi_a
```

The compiler reads Aria source, runs the full pipeline (lex → resolve → check → lower → emit), and writes a native ARM64 Mach-O executable.

### Run the Test Suite

Build and run the end-to-end test suite:

```bash
../aria-compiler-go/aria build \
  src/diagnostic/diagnostic.aria \
  src/lexer/token.aria \
  src/lexer/lexer.aria \
  src/resolver/scope.aria \
  src/resolver/resolver.aria \
  src/checker/types.aria \
  src/checker/traits.aria \
  src/checker/checker.aria \
  src/codegen/arm64.aria \
  src/codegen/ir.aria \
  src/codegen/emit.aria \
  src/codegen/runtime.aria \
  src/codegen/macho.aria \
  src/codegen/lower.aria \
  src/codegen/codegen.aria \
  src/codegen/e2e_test.aria

./diagnostic    # Compiles and writes 11 test binaries to /tmp/

# Verify all pass
for t in entry_test call_test index_expr_test match_test struct_test \
         array_test integration_test string_test growth_test str_simple; do
  /tmp/aria_${t}
  echo "${t}: $?"
done
```

## What's Planned

Based on the [implementation plan](plan.md) and current progress, here's what's ahead:

### Near-Term (Toward Full Self-Hosting)

- **String interpolation** in codegen — `"hello {name}"` currently works in the lexer/parser but not yet in native code emission
- **Closures** — closure capture and GC-boxed representation
- **Error handling** — `Result` types, `?` propagation, `catch` blocks
- **For loops with iterators** — currently simplified; needs proper range/collection iteration
- **Growable string buffers** — currently uses mmap-per-allocation; needs bump allocator or arena
- **More stdlib** — `Map`, `Set`, sorting, formatting utilities needed by the compiler itself

### Medium-Term (Post Self-Hosting)

- **Optimization passes** — constant folding, dead code elimination, inlining
- **Better register allocation** — move from spill-everything to linear scan
- **Linux ARM64 support** — ELF output alongside Mach-O
- **x86-64 backend** — second architecture target
- **Incremental compilation** — only recompile changed files
- **Proper garbage collector** — tracing GC to replace mmap-per-allocation

### Long-Term (Full Language)

- **Concurrency** — tasks, structured concurrency with `scope`, channels, `select`
- **Effect system** — purity verification, effect declarations
- **FFI** — foreign function interface for C interop
- **Memory annotations** — `@stack`, `@arena`, `@inline` for granular control
- **LLVM backend** — Tier 2 optimizing backend
- **Full standard library** — networking, JSON, crypto, HTTP

## Language Quick Reference

```aria
mod main

use std.fs

// Variables
x := 42
mut y := 0
name: str = "Aria"

// Functions
fn add(a: i64, b: i64) -> i64 = a + b

fn greet(name: str) {
    println("Hello, " + name)
}

// Structs
struct Point { x: f64, y: f64 }

// Sum types
type Shape =
    | Circle(f64)
    | Rect(f64, f64)
    | Dot

// Pattern matching
fn describe(s: Shape) -> str = match s {
    Circle(r) => "circle"
    Rect(w, h) => "rectangle"
    Dot => "dot"
}

// Constants
const MAX_SIZE: i64 = 1024

// Entry point
entry {
    println("Hello from Aria")
}

// Tests
test "addition" {
    assert add(2, 3) == 5
}
```

## License

See repository for license details.
