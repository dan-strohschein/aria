# Aria Self-Hosting Compiler — Implementation Plan

This document defines the complete implementation plan for the Aria self-hosting compiler. Each milestone builds on the previous one. The compiler is written in Aria and compiled by the bootstrap compiler (`../aria-compiler-go/`).

## Constraints

- Must be compilable by the bootstrap compiler (Aria → Go transpilation)
- No concurrency features (spawn, scope, channels, select)
- No FFI, no memory annotations
- Only bootstrap-provided stdlib functions
- Single-threaded compilation

---

## Milestone 1: Project Foundation & Diagnostics

**Goal:** Establish the project skeleton, error reporting infrastructure, and basic I/O so all subsequent milestones have a foundation to build on.

### Phase 1.1: Diagnostic System

**Why first:** Every compiler stage produces diagnostics. Building this first means all stages get structured error reporting from day one.

- **US-1.1.1: Diagnostic types** — Define `Diagnostic`, `Severity` (Error, Warning, Info), `Span` (file, line, col, offset), and `Label` (span + message) types.
- **US-1.1.2: Error code registry** — Define error code enum (E0001, E0002, ..., W0001, ...) with descriptions. Codes per `compiler-diagnostics.md`.
- **US-1.1.3: Human-readable rendering** — Render diagnostics with source line, caret underlining, labels, and suggestions.
- **US-1.1.4: JSON output** — Render diagnostics as JSON (for editor integration and `--format=json`).
- **US-1.1.5: Diagnostic collector** — A `DiagnosticBag` that collects diagnostics during compilation and can report whether any errors occurred.

**Verification:** Can create diagnostics, render them to human-readable and JSON formats, collect them, and query error state.

### Phase 1.2: CLI Entry Point

- **US-1.2.1: Argument parsing** — Parse `check`, `build`, `run`, and `test` subcommands with `--format` flag.
- **US-1.2.2: File reading** — Read `.aria` source files from disk.
- **US-1.2.3: Pipeline skeleton** — Wire up the compilation pipeline: read → lex → parse → resolve → check → codegen, with each stage returning diagnostics.

**Verification:** `aria check file.aria` reads the file and prints "no errors" or diagnostic output.

---

## Milestone 2: Lexer

**Goal:** Tokenize Aria source files into a token stream per `formal-grammar.md` §2.

### Phase 2.1: Core Tokenization

- **US-2.1.1: Token types** — Define `TokenKind` enum covering all keywords, operators, punctuation, literals, and special tokens (Eof, Newline, Error).
- **US-2.1.2: Token struct** — `Token` with kind, text, span, and value (for literals).
- **US-2.1.3: Basic lexer** — Lex identifiers, keywords, integer literals, float literals, and boolean literals.
- **US-2.1.4: Operators and punctuation** — All single-char and multi-char operators (`==`, `!=`, `<=`, `>=`, `->`, `=>`, `..`, `...`, `|>`, etc.).
- **US-2.1.5: String literals** — Double-quoted strings with escape sequences (`\n`, `\t`, `\\`, `\"`, `\{`, `\0`).
- **US-2.1.6: String interpolation** — Tokenize `"hello {name}"` as a sequence: StringStart, StringPart, Expr tokens, StringEnd.
- **US-2.1.7: Comments** — Line comments (`//`), skip whitespace (except newlines).

### Phase 2.2: Newline Handling

- **US-2.2.1: Newline-as-terminator rules** — Implement `formal-grammar.md` §5: insert implicit statement terminators after tokens that can end a statement, suppress after tokens that cannot.
- **US-2.2.2: Continuation lines** — Lines ending with operators, open brackets, or commas continue to the next line.

### Phase 2.3: Lexer Tests

- **US-2.3.1: Unit tests** — Test each token kind, edge cases (empty input, unterminated strings, invalid characters).
- **US-2.3.2: Integration tests** — Lex complete Aria snippets and verify full token streams.

**Verification:** Can tokenize all Aria syntax from the spec examples. All tests pass.

---

## Milestone 3: Parser

**Goal:** Parse token streams into a typed AST per `formal-grammar.md` §3-4.

### Phase 3.1: AST Node Types

- **US-3.1.1: Expression nodes** — Literal, Identifier, Binary, Unary, Call, FieldAccess, Index, Match, If, Block, Closure, Tuple, Array, StringInterp, Range.
- **US-3.1.2: Statement nodes** — VarDecl, Assignment, ExprStmt, Return, Yield, Break, Continue, Defer, ForLoop, WhileLoop.
- **US-3.1.3: Declaration nodes** — FnDecl, TypeDecl (struct), SumTypeDecl, EnumDecl, AliasDecl, NewtypeDecl, TraitDecl, ImplDecl, ConstDecl, UseDecl, ModDecl, EntryBlock, TestBlock.
- **US-3.1.4: Type expression nodes** — NamedType, ArrayType, MapType, TupleType, FnType, ResultType, GenericType.
- **US-3.1.5: Pattern nodes** — LiteralPattern, IdentPattern, VariantPattern, TuplePattern, StructPattern, WildcardPattern, OrPattern, RestPattern.

### Phase 3.2: Expression Parsing

- **US-3.2.1: Precedence climbing** — Implement operator precedence per `operator-precedence.md` (15 levels from assignment to postfix).
- **US-3.2.2: Primary expressions** — Literals, identifiers, parenthesized expressions, block expressions, array/map literals.
- **US-3.2.3: Postfix expressions** — Function calls, field access (`.`), indexing (`[]`), error propagation (`?`), assert-success (`!`).
- **US-3.2.4: Prefix expressions** — Unary minus, logical not (`!`), bitwise not (`~`).
- **US-3.2.5: Control flow expressions** — `if`/`else`, `match`, `for`, `while`.
- **US-3.2.6: Closures** — `|params| expr` and `|params| { body }`.
- **US-3.2.7: Pipe operator** — `value |> fn` desugaring to `fn(value)`.
- **US-3.2.8: String interpolation** — Parse interpolated expressions within strings.
- **US-3.2.9: Range expressions** — `a..b` and `a...b`.

### Phase 3.3: Statement and Declaration Parsing

- **US-3.3.1: Variable declarations** — `x := expr`, `mut x := expr`, `x: Type = expr`.
- **US-3.3.2: Assignments** — `x = expr`, `x += expr`, compound assignments.
- **US-3.3.3: Function declarations** — Full signature: name, generics, params, return type, error type, effect clause, body (block or `= expr`).
- **US-3.3.4: Type declarations** — `struct Name { fields }`, sum types `type Name = Variant1 | Variant2`, enums, newtypes, aliases.
- **US-3.3.5: Trait and impl declarations** — `trait Name { methods }`, `impl Trait for Type { methods }`.
- **US-3.3.6: Use declarations** — `use module.path`, `use module.path as alias`, `use module.{a, b}`.
- **US-3.3.7: Entry and test blocks** — `entry { ... }`, `test "name" { ... }`.
- **US-3.3.8: Const declarations** — `const NAME: Type = expr`.
- **US-3.3.9: Match expressions** — Full pattern matching with arms, guards.

### Phase 3.4: Parser Tests

- **US-3.4.1: Expression tests** — Each expression kind parses correctly with proper precedence and associativity.
- **US-3.4.2: Declaration tests** — Each declaration kind, including edge cases.
- **US-3.4.3: Error recovery** — Parser produces diagnostics for malformed input and recovers to continue parsing.
- **US-3.4.4: Full program tests** — Parse complete Aria programs from testdata.

**Verification:** Can parse all Aria syntax from the spec. AST structure matches expected output. Parser error messages are helpful.

---

## Milestone 4: Name Resolution

**Goal:** Resolve all identifiers to their declarations, build scope hierarchy per `scoping-rules.md`.

### Phase 4.1: Scope Infrastructure

- **US-4.1.1: Scope types** — Universe scope (builtins), package scope, module scope, import scope, function scope, block scope.
- **US-4.1.2: Symbol table** — Map from name to declaration, with support for shadowing.
- **US-4.1.3: Scope hierarchy** — Parent pointers, child creation, lookup with upward traversal.

### Phase 4.2: Resolution Algorithm

- **US-4.2.1: Top-level declarations** — Register all top-level declarations before resolving bodies (allows forward references).
- **US-4.2.2: Import resolution** — Resolve `use` declarations to modules/declarations.
- **US-4.2.3: Expression resolution** — Resolve identifiers in expressions to their declarations.
- **US-4.2.4: Pattern resolution** — Resolve pattern identifiers, distinguish variable bindings from variant references.
- **US-4.2.5: Type reference resolution** — Resolve type names in annotations to type declarations.
- **US-4.2.6: Trait method resolution** — Resolve method calls to trait implementations.

### Phase 4.3: Multi-file Resolution

- **US-4.3.1: Module graph** — Build dependency graph from `use` declarations.
- **US-4.3.2: Cross-file resolution** — Resolve references across files within the same package.
- **US-4.3.3: Cycle detection** — Detect and report circular module dependencies.

### Phase 4.4: Resolver Tests

- **US-4.4.1: Scope tests** — Variable shadowing, block scoping, forward references.
- **US-4.4.2: Import tests** — Cross-module resolution, alias imports.
- **US-4.4.3: Error tests** — Undefined names, duplicate definitions, circular deps.

**Verification:** All identifiers in valid programs resolve correctly. Undefined/ambiguous references produce clear diagnostics.

---

## Milestone 5: Type Checker

**Goal:** Type-check resolved ASTs. This is the largest and most complex milestone.

### Phase 5.1: Type Representation

- **US-5.1.1: Core types** — Primitives (i8–i64, u8–u64, f32, f64, bool, str, char, unit), arrays, maps, tuples, functions, named types.
- **US-5.1.2: Type variables** — For inference: unknown types that get unified during checking.
- **US-5.1.3: Generic types** — Types parameterized by type variables with trait bounds.
- **US-5.1.4: Result types** — `T ! E` representation for error-returning functions.

### Phase 5.2: Type Inference

- **US-5.2.1: Bidirectional inference** — Push expected types inward (from context), pull actual types outward (from expressions).
- **US-5.2.2: Literal typing** — `42` is `i64`, `3.14` is `f64`, `true` is `bool`, `"hello"` is `str`.
- **US-5.2.3: Variable type inference** — Infer types from initializers: `x := 42` gives `x: i64`.
- **US-5.2.4: Function return inference** — Infer return types from body when not annotated.
- **US-5.2.5: Generic instantiation** — Infer type arguments from usage: `identity(42)` instantiates `fn identity[T](x: T) -> T` with `T = i64`.
- **US-5.2.6: Unification** — Unify type variables with concrete types, propagating constraints.

### Phase 5.3: Type Checking Rules

- **US-5.3.1: Binary/unary operators** — Desugar to trait method calls per `design-decisions-v01.md` Decision 2. `a + b` → `a.add(b)`.
- **US-5.3.2: Function calls** — Check argument types match parameter types, check arity.
- **US-5.3.3: Field access** — Check field exists on struct type, check visibility.
- **US-5.3.4: Index expressions** — Check indexable types (arrays, maps, strings).
- **US-5.3.5: Assignment checking** — Check mutability (`mut` required), check type compatibility.
- **US-5.3.6: Control flow** — Check if/else arms have compatible types, check match is exhaustive.
- **US-5.3.7: Error propagation** — Check `?` is used inside functions with compatible error types.
- **US-5.3.8: Closures** — Infer parameter/return types from context, check capture semantics.

### Phase 5.4: Trait System

- **US-5.4.1: Trait bounds checking** — Verify that types used with bounded generics implement required traits.
- **US-5.4.2: Impl validation** — Check that impl blocks provide all required methods with correct signatures.
- **US-5.4.3: Method resolution** — Resolve `x.method()` to the correct impl, handling traits and inherent methods.
- **US-5.4.4: Derive checking** — Validate `derive` clauses and generate implicit implementations.

### Phase 5.5: Advanced Checking

- **US-5.5.1: Exhaustiveness checking** — Verify match expressions cover all variants per `pattern-matching.md`.
- **US-5.5.2: Effect checking** — Pure functions cannot call effectful functions per `effect-system.md`.
- **US-5.5.3: Recursive type detection** — Detect recursive types and ensure auto-boxing per Decision 4.
- **US-5.5.4: Send/Share checking** — (Deferred — not needed without concurrency in bootstrap).
- **US-5.5.5: Const evaluation** — Evaluate constant expressions at compile time per `const-evaluation.md`.

### Phase 5.6: Checker Tests

- **US-5.6.1: Type inference tests** — Verify inferred types for all expression kinds.
- **US-5.6.2: Error detection tests** — Type mismatches, missing trait impls, non-exhaustive matches.
- **US-5.6.3: Generic tests** — Monomorphization, bounded generics, type argument inference.
- **US-5.6.4: Trait tests** — Method resolution, trait bounds, derive.
- **US-5.6.5: Effect tests** — Purity violations detected.

**Verification:** Type checker accepts well-typed programs and rejects ill-typed programs with clear diagnostics. All spec-compliant programs type-check. All invalid programs produce correct errors.

---

## Milestone 6: Code Generation (Tier 1 — Native)

**Goal:** Generate native machine code from type-checked ASTs. Unlike the bootstrap compiler (which transpiles to Go), the self-hosting compiler produces native code directly.

### Phase 6.1: IR Design

- **US-6.1.1: IR node types** — SSA-form IR: basic blocks, instructions (arithmetic, comparison, control flow, memory, calls).
- **US-6.1.2: AST to IR lowering** — Translate typed AST to IR, handling all expression/statement kinds.
- **US-6.1.3: IR validation** — Validate SSA properties (single assignment, dominance).

### Phase 6.2: Native Code Emission

- **US-6.2.1: Target abstraction** — Abstract over x86-64 and ARM64 (macOS, Linux).
- **US-6.2.2: Register allocation** — Simple register allocator (linear scan or graph coloring).
- **US-6.2.3: Instruction selection** — Map IR instructions to native instructions.
- **US-6.2.4: Function calling convention** — System V ABI for function calls.
- **US-6.2.5: Object file emission** — Emit Mach-O (macOS) or ELF (Linux) object files.

### Phase 6.3: Linking

- **US-6.3.1: Runtime linking** — Link compiled code with the Aria runtime.
- **US-6.3.2: System linker** — Invoke the system linker to produce final executables.

### Phase 6.4: Codegen Tests

- **US-6.4.1: IR lowering tests** — Verify IR structure for representative programs.
- **US-6.4.2: End-to-end tests** — Compile and run Aria programs, verify output.

**Verification:** Can compile Aria programs to native executables that produce correct output.

---

## Milestone 7: Runtime

**Goal:** Implement the Aria runtime that is linked into every compiled program.

### Phase 7.1: Memory Management

- **US-7.1.1: GC implementation** — Tracing garbage collector per `garbage-collector.md`. (Bootstrap version can use a simple mark-and-sweep.)
- **US-7.1.2: Allocator** — Heap allocator for GC-managed objects.
- **US-7.1.3: Stack management** — Goroutine-style growable stacks or fixed stacks.

### Phase 7.2: Core Runtime

- **US-7.2.1: String representation** — SSO (small string optimization), UTF-8 encoding per `string-handling.md`.
- **US-7.2.2: Array/slice runtime** — Growable arrays with bounds checking.
- **US-7.2.3: Map runtime** — Hash map implementation.
- **US-7.2.4: Panic/error infrastructure** — Error trace construction, panic handling.

### Phase 7.3: I/O Runtime

- **US-7.3.1: Standard I/O** — `print`, `println`, `eprintln`, stdin reading.
- **US-7.3.2: File I/O** — File open/read/write/close via system calls.
- **US-7.3.3: Process control** — Exit codes, command-line arguments.

**Verification:** Runtime supports all operations needed by compiler programs. GC correctly collects unreachable objects.

---

## Milestone 8: Standard Library (Compiler Subset)

**Goal:** Implement the minimal stdlib needed for the compiler itself to function.

### Phase 8.1: Core Types

- **US-8.1.1: str operations** — Concatenation, slicing, indexing, `contains`, `starts_with`, `ends_with`, `split`, `trim`, `replace`, `len`.
- **US-8.1.2: Vec[T]** — Dynamic array: `push`, `pop`, `len`, `get`, `set`, `iter`, `map`, `filter`.
- **US-8.1.3: Map[K, V]** — Hash map: `get`, `set`, `contains`, `remove`, `keys`, `values`, `iter`.
- **US-8.1.4: Option[T]** — `Some(T)` / `None` with `map`, `unwrap`, `unwrap_or`, `is_some`, `is_none`.
- **US-8.1.5: Result[T, E]** — `Ok(T)` / `Err(E)` with `map`, `map_err`, `unwrap`, `?` support.

### Phase 8.2: I/O

- **US-8.2.1: io module** — `print`, `println`, `eprintln`, `read_line`.
- **US-8.2.2: fs module** — `read`, `write`, `exists`, `list_dir`, `create_dir`, `path_join`.

### Phase 8.3: Utility

- **US-8.3.1: fmt module** — String formatting, `Display` trait default implementations.
- **US-8.3.2: process module** — `args()`, `exit()`, `env_var()`.
- **US-8.3.3: sort** — Sorting for arrays/vecs (needed for deterministic output).

**Verification:** All stdlib functions used by the compiler itself work correctly. Compiler can read files, process strings, manage collections, and produce output.

---

## Milestone 9: Self-Hosting

**Goal:** The compiler can compile itself.

### Phase 9.1: Self-Compilation

- **US-9.1.1: Compile self with bootstrap** — Use `../aria-compiler-go/aria build` to compile this compiler into a native executable.
- **US-9.1.2: Compile self with self** — Use the bootstrapped executable to compile the compiler source again.
- **US-9.1.3: Binary comparison** — The output of step 2 should be functionally identical (may not be byte-identical due to timestamps etc., but behavior must match).

### Phase 9.2: Validation

- **US-9.2.1: Test suite passes** — All compiler tests pass when run through the self-compiled compiler.
- **US-9.2.2: Spec examples** — All 16 example programs from `../aria-docs/examples/` compile and run correctly.
- **US-9.2.3: Bootstrap retirement** — Document that the bootstrap compiler is no longer needed.

**Verification:** The self-hosting compiler can compile itself and produce a working compiler. The bootstrap compiler can be retired.

---

## Implementation Order Summary

| Order | Milestone | Key Output |
|-------|-----------|------------|
| 1 | Diagnostics & CLI | Error reporting infrastructure |
| 2 | Lexer | Token stream from source |
| 3 | Parser | AST from tokens |
| 4 | Name Resolution | Resolved AST with scope info |
| 5 | Type Checker | Type-checked AST |
| 6 | Code Generation | Native executables |
| 7 | Runtime | GC, strings, I/O |
| 8 | Stdlib | Collections, file I/O, formatting |
| 9 | Self-Hosting | Compiler compiles itself |

Each milestone is independently testable. Early milestones (1–5) can be developed and tested without native codegen by using the bootstrap compiler for compilation.
