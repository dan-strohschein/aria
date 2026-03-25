# Aria Programming Language

**Aria is a programming language designed for AI assistants.** It is not meant for humans to write by hand — it is designed to be maximally efficient for LLMs to generate, reason about, and maintain. Every design decision optimizes for how AI models process and produce code: unambiguous grammar, minimal syntax noise, explicit types, no hidden behavior, and exhaustive safety checks that catch the mistakes LLMs make.

Aria is statically typed, compiles to native executables via LLVM, and the compiler is **self-hosting** — it compiles its own source code.

## Why a Language for AI?

Human-oriented languages are full of ambiguity, implicit behavior, and ceremony that LLMs struggle with: semicolons they forget, null they don't check, exceptions they don't handle, overloaded syntax they misparse. Aria eliminates all of these:

- **No boilerplate** — no semicolons, type inference, expression-body functions. Less tokens = cheaper and more accurate generation.
- **No ambiguity** — every construct has exactly one meaning. The grammar requires zero lookahead for most constructs.
- **No implicit behavior** — no null, no exceptions, no implicit conversions, no hidden control flow. What the AI writes is what runs.
- **Exhaustive safety** — the compiler catches missing match arms, unhandled errors, mutability violations, and effect mismatches. LLMs make these mistakes constantly; the compiler fixes them.
- **Algebraic types** — sum types and pattern matching give LLMs a structured way to model domain logic without class hierarchies.
- **Structured concurrency** — `spawn`, `scope`, channels, `select`. Safe concurrency that's hard to get wrong.
- **Effect tracking** — `with [Io, Fs, Net]` makes side effects visible to the AI reasoning about code.
- **GC with opt-out** — garbage collected by default; `@stack`, `@arena`, `Pool[T]` when the AI needs control.
- **FFI** — `extern fn` calls C functions directly for system-level work.
- **164 test programs** covering every language feature (158 single-file pass, 6 require external dependencies or multi-file build).
- **Standard library** — JSON parser, HTTP server, PostgreSQL client in `lib/`.

## Quick Example

```aria
mod main

type Shape =
    | Circle(f64)
    | Rectangle(f64, f64)
    | Point

fn area(s: Shape) -> f64 = match s {
    Circle(r) => 3.14159 * r * r
    Rectangle(w, h) => w * h
    Point => 0.0
}

fn fetch_all(urls: [str]) -> [str] {
    scope {
        // All spawned tasks auto-join at scope exit
        t1 := spawn fetch(urls[1])
        t2 := spawn fetch(urls[2])
    }
}

entry {
    shape := Circle(5.0)
    println("Area: {area(shape)}")

    config := read_file("config.json") or "{}"
    port := parse_int(config) or 8080

    println("Listening on port {port}")
}

test "area calculation" {
    assert area(Circle(1.0)) == 3.14159
    assert area(Rectangle(3.0, 4.0)) == 12.0
}
```

## Building

### Prerequisites

- macOS on Apple Silicon (ARM64) — or Linux ARM64/AMD64 (cross-compilation supported)
- Go 1.21+ (for the bootstrap compiler)
- clang (for LLVM IR compilation)

### Build the Compiler

```bash
# Clone both repos
git clone https://github.com/dan-strohschein/aria-compiler-go.git
git clone https://github.com/dan-strohschein/aria.git

# Build the bootstrap compiler
cd aria-compiler-go
go build -o aria ./cmd/aria
cd ../aria

# Build the self-hosting compiler (multi-file)
../aria-compiler-go/aria build src/main.aria src/lexer/lexer.aria src/lexer/token.aria \
  src/parser/parser.aria src/parser/ast.aria src/parser/precedence.aria \
  src/resolver/resolver.aria src/resolver/scope.aria \
  src/checker/checker.aria src/checker/types.aria src/checker/traits.aria \
  src/codegen/codegen.aria src/codegen/lower.aria src/codegen/ir.aria src/codegen/llvm.aria \
  src/diagnostic/diagnostic.aria

# Rename the output
mv main src/aria_generated
```

### Self-Compile (Compiler Compiles Itself)

```bash
./src/aria_generated build src/
# Produces src/src — a native ARM64 binary built by its own source code
```

### Use the Compiler

```bash
# Type-check
./src/aria_generated check myfile.aria

# Compile to native executable
./src/aria_generated build myfile.aria

# Compile and run
./src/aria_generated run myfile.aria

# Run tests
./src/aria_generated test myfile.aria

# Run benchmarks
./src/aria_generated bench myfile.aria

# Explain an error
./src/aria_generated explain E0400

# Apply auto-fixes
./src/aria_generated fix myfile.aria
```

### Directory Arguments

The compiler accepts directories — it recursively finds all `.aria` files:

```bash
./src/aria_generated build src/        # compiles entire compiler
./src/aria_generated check myproject/  # checks all files in project
```

## Language Overview

### Variables and Types

```aria
x := 42                    // immutable, type inferred (i64)
mut count := 0             // mutable
name: str = "Aria"         // explicit type
const PI: f64 = 3.14159    // compile-time constant

// Duration and size literals
timeout := 30s             // nanoseconds
buffer := 4kb              // bytes
```

### Collections

```aria
// Arrays
nums := [1, 2, 3, 4, 5]
doubled := [x * 2 for x in nums]          // list comprehension
evens := [x for x in nums where x % 2 == 0]  // with filter

// Maps (FNV hash table)
m := Map()
m.set("key", 42)
val := m.get("key")
if m.has("key") { println("found") }

// Sets (FNV hash set)
s := Set()
s.add("hello")
s.add("world")
assert s.contains("hello")
assert s.len() == 2

// Tuples
pair := (1, "hello")
```

### Error Handling

```aria
// Functions declare error types with !
fn parse(input: str) -> Config ! ParseError {
    if input.len() == 0 { return Err(ParseError.Empty) }
    Ok(Config{...})
}

// ? propagates errors
fn load() -> App ! AppError {
    config := parse(read_file("app.json")?) ? |e| AppError.Config{source: e}
    Ok(App{config: config})
}

// Recovery options
val := parse(input) catch |e| { default_config }
val := parse(input) or default_config
val := parse(input) must "config required"
val := parse(input)!  // panic on error
```

### Pattern Matching

```aria
match result {
    Ok(value) if value > 0 => println("positive: {value}")
    Ok(0) => println("zero")
    Ok(n) => println("negative: {n}")
    Err(e) => println("error: {e}")
}

// Or-patterns, named patterns, nested patterns
match shape {
    s @ Circle(r) if r > 10.0 => large_circle(s)
    Circle(_) | Point => small()
    Rectangle(w, h) => area(w, h)
}

// Refutable bindings
Ok(config) := load_config() else {
    return default_config()
}
```

### Concurrency

```aria
// Structured concurrency — tasks auto-join at scope exit
scope {
    users := spawn fetch_users()
    orders := spawn fetch_orders()
}
// Both complete here

// Channels
ch := _ariaChanNew(10)
ch.send(42)
val := ch.recv()

// Iterate channel until closed
for msg in ch {
    process(msg)
}

// Detached tasks (not auto-joined)
spawn.detach fn() -> i64 { background_work() }
```

### Traits, Generics, and Dynamic Dispatch

```aria
trait Display {
    fn display(self) -> str
}

impl Display for Point {
    fn display(self) -> str = "({self.x}, {self.y})"
}

fn print_all[T: Display](items: [T]) {
    for item in items {
        println(item.display())
    }
}

struct Pair[A, B] {
    first: A
    second: B
} derives [Eq, Clone, Debug]

// Dynamic dispatch via trait objects
fn describe(obj: dyn Display) {
    println(obj.display())
}

entry {
    describe(dyn Display(Point{x: 1, y: 2}))
}
```

### FFI (Foreign Function Interface)

```aria
extern fn abs(n: i64) -> i64

extern "sqlite3" {
    fn sqlite3_open(filename: i64, db: i64) -> i64
    fn sqlite3_close(db: i64) -> i64
}

entry {
    result := abs(-42)  // calls C abs()
}
```

### Memory Management

```aria
// Default: GC-managed (no annotation needed)
data := MyStruct{x: 1, y: 2}

// Opt-out: stack allocation
buf := @stack Buffer.new(4096)

// Opt-out: arena (bulk allocate, bulk free)
arena := _ariaArenaNew(2mb)
// ... many allocations ...
_ariaArenaFree(arena)

// Opt-out: object pool
pool := _ariaPoolNew(100, 64)
obj := _ariaPoolGet(pool)
_ariaPoolPut(pool, obj)

// Deterministic cleanup
impl Drop for Connection {
    fn drop(self) { self.close() }
}

with conn := connect(url)? {
    conn.query("SELECT ...")
}  // conn.drop() called here
```

## Architecture

### Compilation Pipeline

```
Source (.aria) --> Lexer --> Parser (AST) --> Resolver --> Checker --> Lowerer --> LLVM IR --> clang --> Native Binary
```

| Stage | Directory | Purpose |
|-------|-----------|---------|
| Diagnostics | `src/diagnostic/` | Error codes, rendering, JSON output |
| Lexer | `src/lexer/` | Tokenization (keywords, literals, interpolation) |
| Parser | `src/parser/` | Pratt parser, AST construction, NodePool |
| Resolver | `src/resolver/` | Name resolution, scope hierarchy, imports |
| Checker | `src/checker/` | Type inference, trait bounds, generics, effects, exhaustiveness |
| Codegen | `src/codegen/` | AST-walking IR lowering, LLVM IR generation, clang invocation |
| Runtime | `runtime/` | C runtime (GC, strings, arrays, maps, sets, channels, networking) |
| Stdlib | `lib/` | JSON parser, HTTP server, PostgreSQL client |
| CLI | `src/main.aria` | Command dispatch, directory expansion, argument parsing |

### Project Stats

- **27 source files** across 7 modules + 3 stdlib libraries
- **164 test programs** in `testdata/programs/`
- **11 example applications** in `examples/`
- **~31,000 lines** of Aria source
- **~2,200 lines** of C runtime
- **Self-compiling** — the compiler builds itself

## Cross-Compilation

The compiler supports 6 target platforms:

| Target | Triple |
|--------|--------|
| `darwin-arm64` | `arm64-apple-macosx14.0.0` (default) |
| `darwin-amd64` | `x86_64-apple-macosx14.0.0` |
| `linux-amd64` | `x86_64-unknown-linux-gnu` |
| `linux-arm64` | `aarch64-unknown-linux-gnu` |
| `windows-amd64` | `x86_64-pc-windows-msvc` |
| `wasm32` | `wasm32-unknown-unknown` |

## AI Integration

Aria is designed for AI-assisted development. Three files enable any AI to generate correct Aria code:

| File | Purpose |
|------|---------|
| [`aria-lang-ai-guide.md`](aria-lang-ai-guide.md) | Universal AI context — paste into any LLM |
| [`.cursorrules`](.cursorrules) | Auto-loaded by Cursor IDE |
| [`.github/copilot-instructions.md`](.github/copilot-instructions.md) | Auto-loaded by GitHub Copilot |

## Documentation

- **Language specification**: [`aria-docs`](https://github.com/dan-strohschein/aria-docs) (33 spec files)
- **Bootstrap compiler**: [`aria-compiler-go`](https://github.com/dan-strohschein/aria-compiler-go) (Go)
- **AI guide**: [`aria-lang-ai-guide.md`](aria-lang-ai-guide.md) (complete syntax reference)

## License

MIT License. See [LICENSE](LICENSE) for details.

---

Designed by Dan Strohschein. Inspired by the giants who came before: Golang, Rust, and C.
