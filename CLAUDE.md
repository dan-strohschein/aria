# Aria Self-Hosting Compiler — AI Assistant Guide

## Tooling — Use Squire/Chisel/Cartograph First

This repo has `.aidocs/` with AID skeletons and a cartograph cache. **Always prefer the AID-based tools for code searches, reads, and refactors.** Fall back to Grep/Read/Edit only when these tools cannot answer the question.

| Task | Tool | Notes |
|---|---|---|
| Find callers of a function | `cartograph callstack <fn> --up` | Uses semantic graph |
| Find callees | `cartograph callstack <fn> --down` | |
| Find what produces an error type | `cartograph errors <Type>` | |
| Find what touches a struct field | `cartograph field <Type.Field>` | |
| Search nodes by name pattern | `cartograph search "<glob>"` | Glob/regex on symbols |
| Rename a symbol across files | `chisel rename` | Semantic, not text |
| Move a function between modules | `chisel move` | |
| Generate/update AID skeletons | `aid-gen` | Run after big changes |
| Unified search/read/refactor | `squire` | Wraps the above |

The Skill tool exposes `cartograph`, `chisel`, `aid`, and `squire` skills. Invoke them via the Skill tool when relevant. Only use Grep/Read/Edit/Glob when:
- Looking at a specific known file/line
- Reading test programs in `testdata/programs/`
- Editing a single file you've already opened
- The cartograph graph doesn't include what you need (e.g., raw text in comments)

## What This Is

This is the **self-hosting Aria compiler**, written in Aria. It compiles Aria source code to native executables. Once this compiler can compile itself, the bootstrap compiler (`../aria-compiler-go/`) is retired.

## The Spec Is the Authority

The language specification lives in a sibling repository:

```
../aria-docs/
```

**Every design question is answered there.** The spec repo contains 33 formal specification files, a high-level design document, an AI guide, 16 example programs, and a design decisions document. When in doubt, read the spec.

### Key spec files for compiler implementation

| File | What it tells you |
|---|---|
| `../aria-docs/high-level-design.md` | Language overview — start here |
| `../aria-docs/CLAUDE.md` | Design principles, conventions, anti-patterns |
| `../aria-docs/spec/formal-grammar.md` | **EBNF grammar — the parser's blueprint** |
| `../aria-docs/spec/operator-precedence.md` | Precedence table — drives the expression parser |
| `../aria-docs/spec/scoping-rules.md` | Name resolution algorithm |
| `../aria-docs/spec/trait-system.md` | Traits, bounds, derives, method resolution |
| `../aria-docs/spec/generics-type-parameters.md` | Generics, monomorphization |
| `../aria-docs/spec/type-conversions.md` | Convert/TryConvert, three conversion mechanisms |
| `../aria-docs/spec/equality-comparison.md` | Eq, Ord, Hash — operator trait mapping |
| `../aria-docs/spec/newtype-aliases.md` | Newtypes vs aliases — parser disambiguation |
| `../aria-docs/spec/pattern-matching.md` | Match expressions, exhaustiveness checking |
| `../aria-docs/spec/error-handling.md` | Result types, ?, catch, error traces |
| `../aria-docs/spec/effect-system.md` | Effect declarations, purity verification |
| `../aria-docs/spec/memory-management.md` | GC, @stack, @arena, @inline, Drop |
| `../aria-docs/spec/closures-capture-semantics.md` | Closures, capture, method references |
| `../aria-docs/spec/concurrency-design.md` | Tasks, scope, channels, select |
| `../aria-docs/spec/string-handling.md` | str representation, SSO, indexing semantics |
| `../aria-docs/spec/compiler-architecture.md` | Compilation pipeline overview |
| `../aria-docs/spec/compiler-diagnostics.md` | Error message format, error codes, JSON output |
| `../aria-docs/spec/testing-framework.md` | Test blocks, assertions, mocking |
| `../aria-docs/spec/design-decisions-v01.md` | Resolved design questions |

## The Bootstrap Compiler

The bootstrap compiler lives at:

```
../aria-compiler-go/
```

It is a Go program that transpiles Aria to Go source code, then calls `go build`. It supports:

- **Lexer**: Full tokenization per the formal grammar
- **Parser**: Recursive descent, all declarations, expressions, statements
- **Resolver**: Name resolution, scope hierarchy, import resolution
- **Checker**: Type inference, trait bounds, generics, exhaustiveness, effects, mutability
- **Codegen**: Aria → Go transpilation (structs, sum types, enums, traits, functions, match, closures, entry blocks, constants)
- **Multi-file compilation**: Compiles multiple `.aria` files into a single Go binary

### Bootstrap limitations

The bootstrap compiler intentionally omits:
- Concurrency (`spawn`, `scope`, channels, `select`)
- FFI
- Memory annotations (`@stack`, `@arena`, `@inline`)
- Duration/size literals
- Bench blocks
- LLVM backend
- Full stdlib — only what's needed to write a compiler

### Building with the bootstrap compiler

```bash
# From this directory, build the bootstrap compiler first
cd ../aria-compiler-go && go build -o aria ./cmd/aria

# Check an Aria file for errors
../aria-compiler-go/aria check src/main.aria

# Build an Aria project
../aria-compiler-go/aria build src/main.aria

# Run an Aria project
../aria-compiler-go/aria run src/main.aria
```

## Project Structure

```
aria/
├── CLAUDE.md              # This file
├── plan.md                # Implementation plan with milestones
├── README.md              # Project overview
├── src/
│   ├── main.aria          # Entry point — CLI dispatcher
│   ├── lexer/
│   │   ├── lexer.aria     # Lexer implementation
│   │   ├── token.aria     # Token types and definitions
│   │   └── tests.aria     # Lexer tests
│   ├── parser/
│   │   ├── parser.aria    # Recursive descent parser
│   │   ├── ast.aria       # AST node types
│   │   ├── precedence.aria # Operator precedence climbing
│   │   └── tests.aria     # Parser tests
│   ├── resolver/
│   │   ├── resolver.aria  # Name resolution, scope building
│   │   ├── scope.aria     # Scope hierarchy
│   │   └── tests.aria     # Resolver tests
│   ├── checker/
│   │   ├── checker.aria   # Type checker main loop
│   │   ├── types.aria     # Type representations
│   │   ├── traits.aria    # Trait resolution, bounds checking
│   │   ├── generics.aria  # Generic instantiation
│   │   ├── effects.aria   # Effect checking
│   │   ├── patterns.aria  # Exhaustiveness checking
│   │   └── tests.aria     # Checker tests
│   ├── codegen/
│   │   ├── codegen.aria   # Native code generation
│   │   ├── ir.aria        # IR / SSA form
│   │   ├── target.aria    # Target platform abstraction
│   │   └── tests.aria     # Codegen tests
│   ├── diagnostic/
│   │   ├── diagnostic.aria # Diagnostic types
│   │   ├── codes.aria     # Error code registry
│   │   ├── render.aria    # Human-readable rendering
│   │   └── tests.aria     # Diagnostic tests
│   └── stdlib/
│       ├── io.aria        # I/O operations
│       ├── str.aria       # String operations
│       ├── collections.aria # Map, Set, Vec
│       └── fs.aria        # File system operations
└── testdata/
    ├── lexer/             # Lexer test inputs
    ├── parser/            # Parser test inputs
    ├── checker/           # Type checker test inputs
    └── programs/          # End-to-end test programs
```

## The 5 Design Pillars (Non-Negotiable)

These govern the language. The compiler must enforce them:

1. **Every token carries meaning** — no boilerplate, no ceremony
2. **The type system is the AI's pair programmer** — sum types, exhaustive matching, effects
3. **Compilation is instantaneous** — unambiguous grammar, minimal lookahead
4. **Performance is opt-in granular** — GC default, manual per-block
5. **No implicit behavior ever** — no implicit conversions, no hidden exceptions, no null

## Coding Conventions for Aria

### General style

- Prefer short, descriptive names
- Use `snake_case` for functions, variables, and modules
- Use `PascalCase` for types, traits, and sum type variants
- Use `SCREAMING_SNAKE_CASE` for constants
- One declaration per line; group related declarations
- Tests live in `tests.aria` files alongside the code they test

### Error handling

- Use `Result` types (`! ErrorType`) for operations that can fail
- Use `?` for error propagation
- Use `catch` blocks for error recovery
- Reserve `assert` for invariants that indicate compiler bugs

### Module organization

- Each compiler stage is its own module directory under `src/`
- Public API functions are declared without underscore prefix
- Internal helpers use underscore prefix convention: `_helper_name`
- Circular dependencies between stages are not allowed

### Self-hosting constraints

This compiler must be compilable by the bootstrap compiler. That means:
- **No concurrency** — no `spawn`, `scope`, channels, or `select`
- **No FFI** — pure Aria only
- **No memory annotations** — no `@stack`, `@arena`, `@inline`
- **No duration/size literals**
- Only stdlib functions that the bootstrap compiler provides

## Testing Strategy

- **Lexer tests**: input string → expected token sequence
- **Parser tests**: input string → expected AST structure
- **Resolver tests**: input program → expected scope/binding information
- **Checker tests**: input file → expected diagnostics (or success)
- **Codegen tests**: input AST → expected output code
- **End-to-end tests**: `.aria` file → expected output, exit code, or compile errors

Use `test` blocks with `assert` for all tests:

```aria
test "lexer tokenizes integer literal" {
    tokens := tokenize("42")
    assert tokens.len() == 2
    assert tokens[0].kind == TokenKind.IntLit
    assert tokens[0].text == "42"
    assert tokens[1].kind == TokenKind.Eof
}
```

## Native Binary Generation (ARM64 macOS)

The compiler generates native Mach-O executables with embedded ad-hoc code signatures. No external tools (`codesign`, `ld`) are needed.

### macOS ARM64 Mach-O Requirements

These are **non-negotiable** — the kernel silently SIGKILLs binaries missing any of them:

- **MH flags**: Must set all four: `MH_PIE | MH_TWOLEVEL | MH_DYLDLINK | MH_NOUNDEFS` (0x200085)
- **LC_UUID**: Required by dyld — crashes with "missing LC_UUID" without it
- **LC_DYLD_CHAINED_FIXUPS + LC_DYLD_EXPORTS_TRIE**: Modern dyld binding format (LC_DYLD_INFO_ONLY is obsolete)
- **LC_MAIN + LC_LOAD_DYLINKER + LC_LOAD_DYLIB**: Dynamic linking through dyld is mandatory on ARM64
- **LC_BUILD_VERSION**: Must target the host macOS version
- **LINKEDIT strict validation**: No gaps between data regions, filesize = exact data total, vmsize page-aligned
- **Ad-hoc code signature**: SuperBlob + CodeDirectory with SHA-256 page hashes, all big-endian

### Debugging Mach-O issues

- `codesign --verify --strict` can pass while the kernel still rejects the binary
- `DYLD_PRINT_APIS=1 ./binary` — if no output appears, the kernel killed it before dyld started
- Exit code 137 (SIGKILL) with no log output = Mach-O structure rejected by kernel
- Compare against `cc -nostdlib -lSystem` reference binary with `otool -l`

## Common Pitfalls

- **`[T]` is both generics and array type** — parser disambiguates by context
- **`!` is both logical NOT (prefix) and assert-success (postfix)** — position determines meaning
- **`|` is both bitwise OR and sum-type variant separator** — context determines meaning
- **No semicolons** — newline termination rules are critical
- **`struct` is sugar for `type`** — both produce identical AST nodes
- **`as` is ONLY for import aliases** — NOT for type casts
- **Recursive types are auto-boxed** — compiler must detect and insert indirection
- **Mutability is on bindings, not fields** — `mut x` makes everything mutable
- **Closures are GC-boxed** — one type `fn(A) -> B`, uniform representation
- **No integer literal suffixes** — `42` is always `i64`, use type annotation for others
